#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include "infra/logger.h"
#include "infra/config_manager.h"
#include "infra/database_connection_manager.h"
#include "services/theme_manager.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "services/scheduling_service.h"
#include "services/branch_manager.h"
#include "infra/auto_backup_scheduler.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "services/audit_service.h"
#include "ui/login/login_dialog.h"
#include "ui/main_window.h"
#include "ui/intro_splash.h"
#include "ui/pos/pos_window.h"  // Phase 1: POS interface

// ── Windows SEH crash handler — writes crash point to log ─────────────────────
#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <fstream>
#include <sstream>

// Hide the console window that appears when running from a terminal.
// This is safe to call even if there is no console attached.
static void hideConsole() {
    HWND console = GetConsoleWindow();
    if (console) ShowWindow(console, SW_HIDE);
}

static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    // Write minimal crash info to a file — no Logger (may be dead)
    std::ofstream f("crash_report.txt", std::ios::app);
    if (f.is_open()) {
        f << "=== CRASH ===\n";
        f << "Exception code: 0x" << std::hex
          << ep->ExceptionRecord->ExceptionCode << "\n";
        f << "Exception addr: 0x" << std::hex
          << (uintptr_t)ep->ExceptionRecord->ExceptionAddress << "\n";

        // Walk the stack
        HANDLE process = GetCurrentProcess();
        HANDLE thread  = GetCurrentThread();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(process, NULL, TRUE);

        // Print exe base address so we can calculate relative offsets
        HMODULE hExe = GetModuleHandleA(NULL);
        f << "EXE base: 0x" << std::hex << (uintptr_t)hExe << "\n\n";

        CONTEXT ctx = *ep->ContextRecord;
        STACKFRAME64 sf = {};
        sf.AddrPC.Offset    = ctx.Rip;
        sf.AddrPC.Mode      = AddrModeFlat;
        sf.AddrFrame.Offset = ctx.Rbp;
        sf.AddrFrame.Mode   = AddrModeFlat;
        sf.AddrStack.Offset = ctx.Rsp;
        sf.AddrStack.Mode   = AddrModeFlat;

        char symBuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)symBuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;

        f << "\nStack trace (with module+offset):\n";
        for (int i = 0; i < 30; ++i) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                             &sf, &ctx, NULL,
                             SymFunctionTableAccess64,
                             SymGetModuleBase64, NULL)) break;
            if (sf.AddrPC.Offset == 0) break;

            // Try symbol name first
            DWORD64 disp = 0;
            if (SymFromAddr(process, sf.AddrPC.Offset, &disp, sym)) {
                f << "  #" << std::hex << i << "  " << sym->Name
                  << " + 0x" << std::hex << disp
                  << "  [0x" << std::hex << sf.AddrPC.Offset << "]\n";
            } else {
                // Fallback: print module name + relative offset
                IMAGEHLP_MODULE64 modInfo = {};
                modInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
                if (SymGetModuleInfo64(process, sf.AddrPC.Offset, &modInfo)) {
                    DWORD64 relOffset = sf.AddrPC.Offset - modInfo.BaseOfImage;
                    f << "  #" << std::hex << i
                      << "  [" << modInfo.ModuleName << "]"
                      << " +0x" << std::hex << relOffset
                      << "  (base=0x" << std::hex << modInfo.BaseOfImage << ")\n";
                } else {
                    f << "  #" << std::hex << i
                      << "  ?? [0x" << std::hex << sf.AddrPC.Offset << "]\n";
                }
            }
        }
        f << "=============\n";
        f.close();
    }
    return EXCEPTION_CONTINUE_SEARCH; // let Windows handle normally
}
#endif

// ── Forward declaration ───────────────────────────────────────────────────────
static void showLoginThenMain(IUserRepository* userRepo,
                              IRoleRepository* roleRepo,
                              const QString&   exeDir,
                              bool             useSqlite);

// ── Main window launcher (called after successful login) ──────────────────────
static void showMainWindow(IUserRepository* userRepo,
                           IRoleRepository* roleRepo,
                           const QString&   exeDir,
                           bool             useSqlite)
{
    // Show intro video splash (non-blocking: uses a QDialog::exec that
    // runs a LOCAL nested event loop, not the main one)
    QString introPath = exeDir + "/intro.mp4";
    if (!QFile::exists(introPath))
        introPath = exeDir + "/../src/intro.mp4";
    if (QFile::exists(introPath)) {
        IntroSplash splash(introPath);
        if (splash.isValid())
            splash.exec();
    }

    // Create MainWindow on heap so we can control its lifetime
    auto* window = new MainWindow;
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->show();
    window->raise();
    window->activateWindow();
    Logger::instance().info("Main window opened for user: " +
                            SessionManager::instance().currentUser().username());

    // Start auto-backup scheduler
    AutoBackupScheduler::instance().start();

    // Due-today check
    QTimer::singleShot(800, window, [window, useSqlite]() {
        auto* schedRepo = useSqlite
            ? static_cast<IScheduledOrderRepository*>(new SQLiteScheduledOrderRepository)
            : static_cast<IScheduledOrderRepository*>(new AccessScheduledOrderRepository);
        auto* custRepo = useSqlite
            ? static_cast<ICustomerRepository*>(new SQLiteCustomerRepository)
            : static_cast<ICustomerRepository*>(new AccessCustomerRepository);

        SchedulingService svc(schedRepo, custRepo);
        QList<ScheduledOrder> due = svc.checkDueToday();
        if (!due.isEmpty()) {
            QStringList lines;
            for (const auto& s : due) {
                QString name = s.customerName.isEmpty()
                    ? QString("Customer %1").arg(s.customerId)
                    : s.customerName;
                QString timeStr = s.time.isValid()
                    ? s.time.toString("hh:mm AP")  // e.g. "09:00 AM"
                    : "";
                lines << (timeStr.isEmpty()
                    ? QString("• %1").arg(name)
                    : QString("• %1  at  %2").arg(name, timeStr));
            }
            QMessageBox::information(window, "Scheduled Orders Due Today",
                QString("<b>%1 recurring order(s) are due today:</b><br><br>%2")
                    .arg(due.size()).arg(lines.join("<br>")));
        }
        delete schedRepo;
        delete custRepo;
    });

    // On logout → close window, then re-show login (via deferred call so
    // the current stack unwinds cleanly before we open a new dialog)
    QObject::connect(window, &MainWindow::logoutRequested, window,
        [window, userRepo, roleRepo, exeDir, useSqlite]() {
            Logger::instance().info("Logout signal received, closing main window...");
            window->close();   // triggers WA_DeleteOnClose

            // Defer login re-show until after window is fully destroyed
            QTimer::singleShot(100, [userRepo, roleRepo, exeDir, useSqlite]() {
                Logger::instance().info("Showing login dialog after logout...");
                showLoginThenMain(userRepo, roleRepo, exeDir, useSqlite);
            });
        });
}

// ── POS window launcher (after login for POS interface) ──────────────────────
static void showPosWindow(IUserRepository* userRepo,
                         IRoleRepository* roleRepo,
                         const QString&   exeDir,
                         bool             useSqlite)
{
    // Show intro video splash (same as admin)
    QString introPath = exeDir + "/intro.mp4";
    if (!QFile::exists(introPath))
        introPath = exeDir + "/../src/intro.mp4";
    if (QFile::exists(introPath)) {
        IntroSplash splash(introPath);
        if (splash.isValid())
            splash.exec();
    }
    
    // Get current user's open register session
    const User& currentUser = SessionManager::instance().currentUser();
    IRegisterSessionRepository* sessionRepo = useSqlite
        ? static_cast<IRegisterSessionRepository*>(new SQLiteRegisterSessionRepository)
        : static_cast<IRegisterSessionRepository*>(new AccessRegisterSessionRepository);
    
    RegisterSession openSession = sessionRepo->getOpenSessionByUserId(currentUser.id());
    delete sessionRepo;
    
    // Create POS window on heap
    auto* posWindow = new PosWindow();
    posWindow->setRegisterSession(openSession);
    posWindow->setAttribute(Qt::WA_DeleteOnClose);
    posWindow->show();
    posWindow->raise();
    posWindow->activateWindow();
    
    Logger::instance().info("POS window opened for user: " + currentUser.username());
    
    // On window close → logout and re-show login
    QObject::connect(posWindow, &QMainWindow::destroyed, [userRepo, roleRepo, exeDir, useSqlite]() {
        SessionManager::instance().logout();
        QTimer::singleShot(0, [userRepo, roleRepo, exeDir, useSqlite]() {
            showLoginThenMain(userRepo, roleRepo, exeDir, useSqlite);
        });
    });
}

// ── Login dialog launcher ─────────────────────────────────────────────────────
static void showLoginThenMain(IUserRepository* userRepo,
                              IRoleRepository* roleRepo,
                              const QString&   exeDir,
                              bool             useSqlite)
{
    auto* loginDlg = new LoginDialog(userRepo, roleRepo);
    loginDlg->setAttribute(Qt::WA_DeleteOnClose, true);

    // Handle successful login
    QObject::connect(loginDlg, &QDialog::accepted, loginDlg,
        [loginDlg, userRepo, roleRepo, exeDir, useSqlite]() {
            bool isPosMode = loginDlg->isPosMode();
            loginDlg->close();
            
            // Defer to let login dialog fully close
            QTimer::singleShot(0, [isPosMode, userRepo, roleRepo, exeDir, useSqlite]() {
                if (isPosMode) {
                    // Launch POS interface (with intro)
                    showPosWindow(userRepo, roleRepo, exeDir, useSqlite);
                } else {
                    // Launch Admin interface (with intro)
                    showMainWindow(userRepo, roleRepo, exeDir, useSqlite);
                }
            });
        });

    QObject::connect(loginDlg, &QDialog::rejected, loginDlg,
        [](){ QApplication::quit(); });

    loginDlg->show();
    loginDlg->raise();
    loginDlg->activateWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(crashHandler);
#endif
    // Suppress Qt 6.8+ DPI stall on Windows 10
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough");

#ifdef Q_OS_WIN
    hideConsole();   // suppress the black console window
#endif

    QApplication app(argc, argv);

    // App icon — prefer ICO on Windows for best taskbar/title-bar rendering
    QString exeDir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
    if (exeDir.isEmpty() || !QDir(exeDir).exists())
        exeDir = QDir::currentPath();

    QString iconPath = exeDir + "/logo.ico";
    if (!QFile::exists(iconPath)) iconPath = exeDir + "/../src/logo.ico";
    if (!QFile::exists(iconPath)) iconPath = exeDir + "/logo.png";
    if (!QFile::exists(iconPath)) iconPath = exeDir + "/../src/logo.png";
    if (QFile::exists(iconPath))
        app.setWindowIcon(QIcon(iconPath));

    // Prevent Qt from quitting when a dialog closes
    QApplication::setQuitOnLastWindowClosed(false);

    // ── Logger ────────────────────────────────────────────────────────────────
    Logger& logger = Logger::instance();
    // Write app.log to AppData\Local\DeliHub\ so it is writable when installed
    // under Program Files. Fall back to exeDir (e.g. portable/dev runs).
    {
        QString logDir = QStandardPaths::writableLocation(
                             QStandardPaths::AppLocalDataLocation);
        if (logDir.isEmpty()) logDir = exeDir;
        QDir().mkpath(logDir);
        logger.init(logDir + "/app.log");
    }
    logger.info("DeliHub starting...");

    // ── Writable data directory ───────────────────────────────────────────────
    // Resolved once here; used for config, database, and any future runtime
    // files. Must come before config and branch loading.
    //
    // DESIGN CHOICE: GenericDataLocation → C:\ProgramData\DeliHub\
    //   All Windows user accounts on this machine share the same database and
    //   configuration. This matches a shared branch terminal where multiple
    //   staff log in with different Windows accounts but work on the same data.
    //   Change to AppLocalDataLocation for per-user isolation instead.
    QString writableDataDir = QStandardPaths::writableLocation(
                                  QStandardPaths::GenericDataLocation)
                              + "/DeliHub";
    if (writableDataDir.isEmpty() || writableDataDir == "/DeliHub")
        writableDataDir = exeDir;   // fallback: portable / dev run
    QDir().mkpath(writableDataDir);
    logger.info("Writable data directory: " + writableDataDir);

    // ── Config ────────────────────────────────────────────────────────────────
    // config.ini strategy:
    //   - The installer places a read-only template in the exe directory.
    //   - The live/writable copy lives in writableDataDir so all users on
    //     this machine share the same settings (branch list, company name, etc.)
    //     even if they log into Windows with different accounts.
    //   - On first run, copy the template to the writable dir if absent.
    QString exeConfigPath      = exeDir + "/config.ini";
    QString writableConfigPath = writableDataDir + "/config.ini";

    if (!QFile::exists(writableConfigPath)) {
        // First run or fresh install — seed from the exe-dir template
        QString templatePath = exeConfigPath;
        if (!QFile::exists(templatePath)) templatePath = exeDir + "/../config.ini";
        if (QFile::exists(templatePath)) {
            QFile::copy(templatePath, writableConfigPath);
            logger.info("Seeded config from template: " + templatePath);
        }
    }

    // Use writable copy; fall back to exe-dir template (portable/dev)
    QString configPath = QFile::exists(writableConfigPath)
                         ? writableConfigPath
                         : exeConfigPath;
    if (!QFile::exists(configPath)) configPath = exeDir + "/../config.ini";
    if (!QFile::exists(configPath)) configPath = "config.ini";

    ConfigManager::instance().init(configPath);
    logger.info("Config loaded: " + configPath);

    // Load branch definitions from config, supplying the writable data dir
    // so the default branch's db file lands in the right place.
    BranchManager::instance().loadBranches(configPath, writableDataDir);

    // ── Theme & Language ──────────────────────────────────────────────────────
    ThemeManager::instance().applyTheme(ConfigManager::instance().theme());
    LangManager::instance().setLanguage(ConfigManager::instance().language());

    // ── Database ──────────────────────────────────────────────────────────────
    if (!DatabaseConnectionManager::instance().openConnection()) {
        QString err = DatabaseConnectionManager::instance().lastError();
        logger.error("DB connect failed: " + err);
        QMessageBox::critical(nullptr, "Database Error",
            "Failed to initialize database:\n" + err);
        return -1;
    }
    logger.info(DatabaseConnectionManager::instance().isSqliteFallbackActive()
        ? "Connected (SQLite Fallback)" : "Connected (Primary DB)");

    // ── Repos (kept alive for the entire app lifetime) ────────────────────────
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    auto* userRepo = useSqlite
        ? static_cast<IUserRepository*>(new SQLiteUserRepository)
        : static_cast<IUserRepository*>(new AccessUserRepository);
    auto* roleRepo = useSqlite
        ? static_cast<IRoleRepository*>(new SQLiteRoleRepository)
        : static_cast<IRoleRepository*>(new AccessRoleRepository);

    // ── Init AuditService ─────────────────────────────────────────────────────
    auto* auditRepo = useSqlite
        ? static_cast<IAuditLogRepository*>(new SQLiteAuditLogRepository)
        : static_cast<IAuditLogRepository*>(new AccessAuditLogRepository);
    AuditService::instance().init(auditRepo);

    // ── Kick off the login → main flow (event-driven, no while loop) ─────────
    showLoginThenMain(userRepo, roleRepo, exeDir, useSqlite);

    int ret = app.exec();   // ONE exec() for the entire app lifetime

    delete userRepo;
    delete roleRepo;
    return ret;
}
