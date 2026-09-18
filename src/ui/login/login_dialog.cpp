#include "login_dialog.h"
#include "attendance_dialog.h"
#include "forgot_password_dialog.h"
#include "services/auth_service.h"
#include "services/session_manager.h"
#include "services/theme_manager.h"
#include "services/lang_manager.h"
#include "services/branch_manager.h"
#include "ui/svg_icon_helper.h"
#include "infra/config_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include "services/barcode_auth.h"
#include "services/audit_service.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/pos/pos_window.h"  // Phase 1: POS interface
#include "core/attendance.h"
#include <QApplication>
#include <QPointer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QLabel>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QCoreApplication>
#include <QPixmap>
#include <QFrame>
#include <QPainter>
#include <QTimer>
#include <QSvgWidget>
#include <QBuffer>
#include <QIcon>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

LoginDialog::LoginDialog(IUserRepository* userRepo,
                         IRoleRepository* roleRepo,
                         QWidget* parent)
    : QDialog(parent), m_userRepo(userRepo), m_roleRepo(roleRepo)
{
    setWindowFlags(Qt::Dialog);
    setupUi();
    
    // Install event filter to capture barcode scanner input
    installEventFilter(this);
    
    // Main Branch first launch: if DB has no users at all, show setup immediately.
    // For branch selection, the check runs inside onLogin() after connection switches.
    if (m_userRepo->getAll().isEmpty()) {
        m_isFirstRun = true;
        QTimer::singleShot(400, this, [this]() {
            bool ok = showFirstRunSetup();
            if (!ok) m_isFirstRun = true;  // keep blocking close if cancelled
        });
    }
}

LoginDialog::~LoginDialog() {
    // Disconnect BEFORE child widgets are destroyed.
    // Without this, a queued languageChanged event arriving after destruction
    // will call setText() on already-deleted QLabel/QPushButton → crash.
    QObject::disconnect(m_langConn);
}

void LoginDialog::setupUi() {
    setWindowTitle("DeliHub — Login");
    setMinimumWidth(380);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(36, 32, 36, 28);

    // Apply icon
    QString iconPath = QCoreApplication::applicationDirPath() + "/logo.png";
    if (!QFile::exists(iconPath)) iconPath = "logo.png";
    if (QFile::exists(iconPath)) setWindowIcon(QIcon(iconPath));

    // ── Brand block (logo + title + subtitle) ─────────────────────────────────
    auto* brandCol = new QVBoxLayout;
    brandCol->setSpacing(4);
    brandCol->setAlignment(Qt::AlignHCenter);

    auto* brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    brandRow->setAlignment(Qt::AlignHCenter);

    // Logo image
    QString logoPath = QCoreApplication::applicationDirPath() + "/logo.png";
    if (!QFile::exists(logoPath)) logoPath = QCoreApplication::applicationDirPath() + "/../src/logo.png";
    if (QFile::exists(logoPath)) {
        auto* logoLbl = new QLabel;
        QPixmap px(logoPath);
        logoLbl->setPixmap(px.scaled(52, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLbl->setAlignment(Qt::AlignCenter);
        brandRow->addWidget(logoLbl);
    }

    m_brandLbl = new QLabel("DeliHub");
    m_brandLbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_brandLbl->setStyleSheet("font-size:28px; font-weight:700; background:transparent;");
    brandRow->addWidget(m_brandLbl);

    brandCol->addLayout(brandRow);

    m_subLbl = new QLabel("Delivery Management System");
    m_subLbl->setAlignment(Qt::AlignCenter);
    // Use objectName so ThemeManager sets color via textSecondary (theme-aware, no background)
    m_subLbl->setObjectName("hintLabel");
    m_subLbl->setStyleSheet("font-size:12px; background:transparent;");
    brandCol->addWidget(m_subLbl);

    root->addLayout(brandCol);
    root->addSpacing(20);   // breathing room between brand and first field

    // ── Form ──────────────────────────────────────────────────────────────────
    auto& L = LangManager::instance();
    auto* form = new QFormLayout;
    form->setSpacing(14);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_userEdit = new QLineEdit;
    m_userEdit->setPlaceholderText(L.t("Username") + "...");
    auto* userLabel = new QLabel(L.t("Username"));
    userLabel->setObjectName("hintLabel");
    userLabel->setStyleSheet("background:transparent; padding:0; margin-bottom:2px;");
    form->addRow(userLabel, m_userEdit);

    m_passEdit = new QLineEdit;
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText(L.t("Password") + "...");
    auto* passLabel = new QLabel(L.t("Password"));
    passLabel->setObjectName("hintLabel");
    passLabel->setStyleSheet("background:transparent; padding:0; margin-bottom:2px;");
    form->addRow(passLabel, m_passEdit);

    // ── Branch selector ───────────────────────────────────────────────────────
    auto* branchLabel = new QLabel("Branch / الفرع");
    branchLabel->setObjectName("hintLabel");
    branchLabel->setStyleSheet("background:transparent; padding:0;");
    m_branchCombo = new QComboBox;
    m_branchCombo->setIconSize(QSize(18, 18));
    const auto& branches = BranchManager::instance().branches();
    for (const auto& b : branches) {
        QString iconPath = b.isCloud ? sidebarSvgPath("cloud-svgrepo-com.svg") 
                                     : sidebarSvgPath("branch-svgrepo-com.svg");
        QIcon icon = SvgIconHelper::icon(iconPath, 18);  // Use automatic icon mode
        m_branchCombo->addItem(icon, " " + b.name, b.id);
    }
    form->addRow(branchLabel, m_branchCombo);

    // ── Interface Type selector (Phase 1: Admin/POS) ──────────────────────────
    auto* interfaceLabel = new QLabel("Interface / الواجهة");
    interfaceLabel->setObjectName("hintLabel");
    interfaceLabel->setStyleSheet("background:transparent; padding:0;");
    m_interfaceTypeCombo = new QComboBox;
    m_interfaceTypeCombo->addItem("🖥️  Admin Interface", "admin");
    m_interfaceTypeCombo->addItem("🛒  POS Interface", "pos");
    form->addRow(interfaceLabel, m_interfaceTypeCombo);

    root->addLayout(form);
    root->addSpacing(10);   // breathing room before login button

    // ── Error label ───────────────────────────────────────────────────────────
    m_errorLbl = new QLabel;
    m_errorLbl->setAlignment(Qt::AlignCenter);
    m_errorLbl->setStyleSheet("color:#F87171; font-size:12px;");
    m_errorLbl->setVisible(false);
    root->addWidget(m_errorLbl);

    // ── Forgot Password Link ──────────────────────────────────────────────────
    auto* forgotPasswordBtn = new QPushButton(L.t("Forgot Password?"));
    forgotPasswordBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #2563eb; "
                                     "text-decoration: underline; font-size: 12px; } "
                                     "QPushButton:hover { color: #1d4ed8; }");
    forgotPasswordBtn->setCursor(Qt::PointingHandCursor);
    forgotPasswordBtn->setAutoDefault(false);  // Don't trigger on Enter
    forgotPasswordBtn->setDefault(false);
    root->addWidget(forgotPasswordBtn, 0, Qt::AlignCenter);
    connect(forgotPasswordBtn, &QPushButton::clicked, this, &LoginDialog::onForgotPassword);
    root->addSpacing(10);

    // ── Attendance Barcode Scanner SVG (Animated) ──────────────────────────────
    auto* scannerFrame = new QFrame();
    scannerFrame->setFixedHeight(170);
    scannerFrame->setStyleSheet("QFrame { background: transparent; border: none; }");
    auto* scannerLayout = new QVBoxLayout(scannerFrame);
    scannerLayout->setContentsMargins(0, 0, 0, 0);
    scannerLayout->setAlignment(Qt::AlignCenter);
    scannerLayout->setSpacing(4);
    
    // Container for overlapping SVG and feedback
    auto* svgContainer = new QWidget();
    svgContainer->setFixedSize(200, 120);
    svgContainer->setStyleSheet("background: transparent;");
    auto* svgContainerLayout = new QVBoxLayout(svgContainer);
    svgContainerLayout->setContentsMargins(0, 0, 0, 0);
    
    // Load and display animated SVG with theme-aware coloring
    QString svgPath = sidebarSvgPath("scanner.svg");
    if (!svgPath.isEmpty() && QFile::exists(svgPath)) {
        // Read SVG file and recolor it based on current icon mode
        QFile svgFile(svgPath);
        if (svgFile.open(QIODevice::ReadOnly)) {
            QByteArray svgData = svgFile.readAll();
            svgFile.close();
            
            // Recolor the SVG using the same logic as SvgIconHelper
            QColor iconColor = SvgIconHelper::getIconColor();
            recolorSvgData(svgData, iconColor);
            
            // Create a temporary recolored SVG renderer
            m_barcodeSvgRenderer = new QSvgRenderer(svgData, this);
            
            // Create QSvgWidget with the recolored renderer
            // Note: QSvgWidget doesn't have setRenderer(), so we create a custom widget
            // that uses the renderer directly
            m_barcodeSvgWidget = new QSvgWidget(svgContainer);
            m_barcodeSvgWidget->setFixedSize(200, 120);
            m_barcodeSvgWidget->setStyleSheet("background: transparent; border: none;");
            m_barcodeSvgWidget->move(0, 0);
            
            // Override the widget's renderer by loading from recolored data
            m_barcodeSvgWidget->load(svgData);
        }
        
        // Feedback label for messages (hidden by default, overlays SVG)
        m_feedbackLabel = new QLabel(svgContainer);
        m_feedbackLabel->setFixedSize(200, 120);
        m_feedbackLabel->setAlignment(Qt::AlignCenter);
        m_feedbackLabel->setStyleSheet("background: transparent; border: none;");
        m_feedbackLabel->setVisible(false);
        m_feedbackLabel->setWordWrap(true);
        m_feedbackLabel->move(0, 0);
        m_feedbackLabel->raise();  // Ensure feedback is on top
    }
    
    scannerLayout->addWidget(svgContainer);
    
    // Manual entry text link (simple label, not button)
    auto* manualLinkLabel = new QLabel("<a href='#' style='color: #93C5FD; text-decoration: none; font-size: 11px;'>📝 Manual Entry</a>");
    manualLinkLabel->setAlignment(Qt::AlignCenter);
    manualLinkLabel->setStyleSheet("background: transparent; padding: 4px;");
    manualLinkLabel->setTextFormat(Qt::RichText);
    manualLinkLabel->setOpenExternalLinks(false);
    connect(manualLinkLabel, &QLabel::linkActivated, this, [this]() {
        bool ok = false;
        const QString barcode = QInputDialog::getText(this,
            "Manual Barcode Entry",
            "Type the full code printed under the barcode on the card.\n"
            "Example:  FP-1789423193-7454\n"
            "(the digits alone also work: 17894231937454)",
            QLineEdit::Normal, "", &ok);
        if (ok && !barcode.trimmed().isEmpty()) {
            processBarcode(barcode);
        }
    });
    scannerLayout->addWidget(manualLinkLabel);
    
    root->addWidget(scannerFrame);
    root->addSpacing(10);

    // ── Login button ──────────────────────────────────────────────────────────
    m_loginBtn = new QPushButton("Login");
    m_loginBtn->setObjectName("primaryBtn");
    m_loginBtn->setFixedHeight(38);
    m_loginBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("login-bracket-svgrepo-com.svg"), QColor("#FFFFFF"), 18));
    m_loginBtn->setIconSize(QSize(18, 18));
    m_loginBtn->setDefault(true);  // Make Login the default button (Enter triggers it)
    root->addWidget(m_loginBtn);

    // Failed attempts counter label
    m_attemptsLabel = new QLabel;
    m_attemptsLabel->setAlignment(Qt::AlignCenter);
    m_attemptsLabel->setStyleSheet("color:#F87171; font-size:12px;");
    m_attemptsLabel->setVisible(false);
    root->addWidget(m_attemptsLabel);

    // ── Appearance row ────────────────────────────────────────────────────────
    root->addSpacing(18);   // was 8 — too tight, made this row look glued to the login button
    auto* appearanceRow = new QHBoxLayout;
    auto hint = [](const QString& t){ auto* l = new QLabel(t); l->setObjectName("hintLabel"); l->setStyleSheet("background:transparent; font-size:11px;"); return l; };

    m_themeCombo = new QComboBox;
    m_themeCombo->addItem("🌙 Dark",  "dark");
    m_themeCombo->addItem("☀️ Light", "light");
    int ti = m_themeCombo->findData(ConfigManager::instance().theme());
    if (ti >= 0) m_themeCombo->setCurrentIndex(ti);

    m_langCombo = new QComboBox;
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("عربي",    "ar");
    m_langCombo->blockSignals(true);  // prevent onLangChanged during setup
    int li = m_langCombo->findData(ConfigManager::instance().language());
    if (li >= 0) m_langCombo->setCurrentIndex(li);
    m_langCombo->blockSignals(false);

    appearanceRow->addWidget(hint("Theme:"));
    appearanceRow->addWidget(m_themeCombo, 1);
    appearanceRow->addWidget(hint("Lang:"));
    appearanceRow->addWidget(m_langCombo, 1);
    root->addLayout(appearanceRow);

    // ── Version ───────────────────────────────────────────────────────────────
    auto* verLbl = new QLabel("v2.3.0   |   By Eng-Hosam Hassan");
    verLbl->setAlignment(Qt::AlignCenter);
    verLbl->setObjectName("hintLabel");
    verLbl->setStyleSheet("font-size:10px; background:transparent; margin-top:6px;");
    root->addWidget(verLbl);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(m_userEdit, &QLineEdit::returnPressed, this, [this]{ m_passEdit->setFocus(); });
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoginDialog::onThemeChanged);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoginDialog::onLangChanged);

    // Retranslate login dialog immediately when language changes
    // Use QPointer for all raw label captures to prevent dangling pointer crash
    QPointer<QLabel> safeBrandLbl(m_brandLbl);
    QPointer<QLabel> safeSubLbl(m_subLbl);
    QPointer<QLabel> safeUserLabel(userLabel);
    QPointer<QLabel> safePassLabel(passLabel);
    QPointer<QPushButton> safeLoginBtn(m_loginBtn);
    QPointer<QLineEdit> safeUserEdit(m_userEdit);
    QPointer<QLineEdit> safePassEdit(m_passEdit);

    // Store the connection handle so destructor can disconnect it explicitly
    m_langConn = connect(
        &LangManager::instance(), &LangManager::languageChanged,
        this, [safeBrandLbl, safeSubLbl, safeUserLabel, safePassLabel,
               safeLoginBtn, safeUserEdit, safePassEdit]() {
            auto& L = LangManager::instance();
            if (safeBrandLbl)  safeBrandLbl->setText("DeliHub");
            if (safeSubLbl)    safeSubLbl->setText(L.t("Delivery Management System"));
            if (safeUserLabel) safeUserLabel->setText(L.t("Username"));
            if (safePassLabel) safePassLabel->setText(L.t("Password"));
            if (safeUserEdit)  safeUserEdit->setPlaceholderText(L.t("Username") + "...");
            if (safePassEdit)  safePassEdit->setPlaceholderText(L.t("Password") + "...");
            if (safeLoginBtn)  safeLoginBtn->setText(L.t("Login"));
        });
}

void LoginDialog::onForgotPassword() {
    // Check if branch is selected and database is connected
    if (!m_branchCombo || m_branchCombo->currentIndex() < 0) {
        QMessageBox::warning(this, 
            LangManager::tr("warning"),
            LangManager::tr("please_select_branch_first"));
        return;
    }
    
    // Switch to selected branch and connect database
    int selectedBranchId = m_branchCombo->currentData().toInt();
    BranchManager::instance().setActiveBranch(selectedBranchId);
    
    DatabaseConnectionManager::instance().closeConnection();
    if (!DatabaseConnectionManager::instance().openConnection()) {
        QMessageBox::critical(this,
            LangManager::tr("error"),
            LangManager::tr("failed_to_connect_database"));
        return;
    }
    
    // Create temporary repository for forgot password dialog
    IUserRepository* tempUserRepo = nullptr;
    const bool isSqlite = 
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    if (isSqlite) {
        tempUserRepo = new SQLiteUserRepository;
    } else {
        tempUserRepo = new AccessUserRepository;
    }
    
    auto* dialog = new ForgotPasswordDialog(tempUserRepo, this);
    dialog->exec();
    dialog->deleteLater();
    
    // Clean up temporary repository
    delete tempUserRepo;
}

void LoginDialog::onLogin() {
    m_errorLbl->setVisible(false);
    m_attemptsLabel->setVisible(false);

    QString username = m_userEdit->text().trimmed();
    QString password = m_passEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        m_errorLbl->setText("Please enter username and password.");
        m_errorLbl->setVisible(true);
        return;
    }

    // ── Step 1: Switch branch & reconnect DB ──────────────────────────────────
    int selectedBranchId = m_branchCombo ? m_branchCombo->currentData().toInt() : 1;
    BranchManager::instance().setActiveBranch(selectedBranchId);
    BranchInfo selectedBranch = BranchManager::instance().activeBranch();

    DatabaseConnectionManager::instance().closeConnection();
    if (!DatabaseConnectionManager::instance().openConnection()) {
        m_errorLbl->setText("❌  Failed to connect to branch database.");
        m_errorLbl->setVisible(true);
        return;
    }

    // ── Step 2: Re-create repos pointing to the NEW branch connection ─────────
    // Must happen BEFORE any user check — old repos still reference the
    // previous branch's DB file.
    if (m_ownedRepos) {
        delete m_userRepo;  m_userRepo = nullptr;
        delete m_roleRepo;  m_roleRepo = nullptr;
    }
    // Pick the right repo type based on the actual connection opened
    const bool branchIsSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    if (branchIsSqlite) {
        m_userRepo = new SQLiteUserRepository;
        m_roleRepo = new SQLiteRoleRepository;
    } else {
        m_userRepo = new AccessUserRepository;
        m_roleRepo = new AccessRoleRepository;
    }
    m_ownedRepos = true;

    // ── Step 3: Branch bootstrap — DB-driven, runs every login attempt ────────
    // Check is purely against the branch DB (now open and schema-initialized).
    // File existence is NOT used — that was the prior bug.
    // This fires every time until at least one user exists in THIS branch.
    if (m_userRepo->getAll().isEmpty()) {
        Logger::instance().info(
            QString("Branch '%1' has no users — triggering admin setup.")
                .arg(selectedBranch.name));

        // Create companion files if they don't exist yet (idempotent)
        if (!selectedBranch.isCloud && !selectedBranch.dbPath.isEmpty()) {
            QFileInfo dbFi(selectedBranch.dbPath);
            QString branchDir = dbFi.absolutePath();

            QString logPath = branchDir + "/app.log";
            if (!QFile::exists(logPath)) {
                QFile lf(logPath);
                if (lf.open(QIODevice::WriteOnly)) lf.close();
            }
            QString cfgPath = branchDir + "/config.ini";
            if (!QFile::exists(cfgPath)) {
                QSettings branchCfg(cfgPath, QSettings::IniFormat);
                branchCfg.setValue("Database/Type",      "SQLITE");
                branchCfg.setValue("Database/SQLitePath", selectedBranch.dbPath);
                branchCfg.setValue("App/BranchId",        selectedBranch.id);
                branchCfg.setValue("App/BranchName",      selectedBranch.name);
                branchCfg.sync();
            }
        }

        // Show setup dialog. If the user cancels without creating an account,
        // showFirstRunSetup() returns false and we abort login — next attempt
        // will hit this same check again (DB still empty → re-prompt).
        bool setupCompleted = showFirstRunSetup();
        if (!setupCompleted) {
            m_errorLbl->setText("⚠️  An admin account is required before you can log in.");
            m_errorLbl->setVisible(true);
            return;
        }

        // Setup succeeded — pre-fill the username the user just created
        // so they don't have to retype it
        if (!m_lastCreatedUsername.isEmpty())
            m_userEdit->setText(m_lastCreatedUsername);
        return;  // force re-login with the new credentials
    }

    // ── Step 4: Authenticate against the branch DB ────────────────────────────
    AuthService auth(m_userRepo);
    User user = auth.authenticate(username, password);

    if (user.id() == 0) {
        ++m_failedAttempts;
        m_passEdit->clear();
        m_passEdit->setFocus();

        if (m_failedAttempts >= 3) {
            QList<User> allUsers = m_userRepo->getAll();
            QList<Role> allRoles = m_roleRepo->getAll();
            QMap<int,Role> roleMap;
            for (const auto& r : allRoles) roleMap[r.id] = r;

            QString adminInfo;
            for (const auto& u : allUsers) {
                if (roleMap.value(u.roleId()).canManageUsers) {
                    adminInfo = QString("👤  %1").arg(u.name().isEmpty() ? u.username() : u.name());
                    if (!u.phone().isEmpty())
                        adminInfo += QString("\n📞  %1").arg(u.phone());
                    break;
                }
            }

            m_errorLbl->setText(QString("❌  %1 failed attempt(s). Please contact admin.")
                                    .arg(m_failedAttempts));
            m_errorLbl->setVisible(true);

            if (!adminInfo.isEmpty()) {
                m_attemptsLabel->setText(QString("🆘  Contact your admin:\n%1").arg(adminInfo));
                m_attemptsLabel->setVisible(true);
            }

            m_loginBtn->setEnabled(false);
            int* remaining = new int(10);
            auto* timer = new QTimer(this);
            QPointer<QPushButton> safeBtn(m_loginBtn);
            connect(timer, &QTimer::timeout, this, [safeBtn, remaining, timer](){
                --(*remaining);
                if (safeBtn) safeBtn->setText(QString("Wait %1s...").arg(*remaining));
                if (*remaining <= 0) {
                    timer->stop();
                    if (safeBtn) {
                        safeBtn->setEnabled(true);
                        safeBtn->setText("🔐  Login");
                    }
                    delete remaining;
                }
            });
            timer->start(1000);
        } else {
            int left = 3 - m_failedAttempts;
            m_errorLbl->setText(QString("❌  Incorrect credentials.  %1 attempt(s) remaining before lockout.")
                                    .arg(left));
            m_errorLbl->setVisible(true);
        }
        return;
    }

    // ── Step 5: Auth success — set session and open app ───────────────────────
    m_failedAttempts = 0;

    Role role = m_roleRepo->getById(user.roleId());
    SessionManager::instance().login(user, role);

    BranchInfo activeBranch = BranchManager::instance().activeBranch();
    ConfigManager::instance().setBranchId(activeBranch.id);
    ConfigManager::instance().setBranchName(activeBranch.name);

    Logger::instance().info("User logged in: " + username +
                            " → branch: " + activeBranch.name);
    AuditService::instance().logLogin(username);

    // ── Step 6: Check interface type and set flag ─────────────────────────────
    QString interfaceType = m_interfaceTypeCombo->currentData().toString();
    
    if (interfaceType == "pos") {
        // POS Interface: Check permissions and register session
        Logger::instance().info("Launching POS interface for user: " + username);
        
        // Check if user has CanAccessPOS permission
        if (!role.canAccessPOS) {
            m_errorLbl->setText("❌  You don't have permission to access POS interface.");
            m_errorLbl->setVisible(true);
            SessionManager::instance().logout();
            return;
        }
        
        // Check if there's an open register session for this user
        const bool branchIsSqlite =
            DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
            DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
        IRegisterSessionRepository* sessionRepo = branchIsSqlite 
            ? static_cast<IRegisterSessionRepository*>(new SQLiteRegisterSessionRepository)
            : static_cast<IRegisterSessionRepository*>(new AccessRegisterSessionRepository);
        
        RegisterSession openSession = sessionRepo->getOpenSessionByUserId(user.id());
        delete sessionRepo;
        
        if (openSession.id() <= 0) {
            m_errorLbl->setText("❌  No active register session.\n\nPlease open a session from Admin interface first.");
            m_errorLbl->setVisible(true);
            SessionManager::instance().logout();
            return;
        }
        
        // Mark as POS mode
        m_isPosMode = true;
    } else {
        // Admin Interface
        m_isPosMode = false;
    }
    
    QObject::disconnect(m_langConn);
    accept();
}

void LoginDialog::onThemeChanged(int) {
    ThemeManager::instance().applyTheme(m_themeCombo->currentData().toString());
}

void LoginDialog::onLangChanged(int) {
    QString code = m_langCombo->currentData().toString();
    LangManager::instance().setLanguage(code);
    ConfigManager::instance().setLanguage(code);
}

// Returns true if an admin account was successfully created, false if cancelled.
// Returning false means the branch DB still has no users — next login attempt
// will call this again (the DB check in onLogin() is the gate, not a flag).
bool LoginDialog::showFirstRunSetup() {
    QMessageBox::information(this, "Welcome to DeliHub",
        "<b>No admin account found for this branch.</b><br><br>"
        "Please create an Admin account to get started.");

    // ── Username ──────────────────────────────────────────────────────────────
    bool ok = false;
    QString username = QInputDialog::getText(this, "Create Admin Account",
        "Choose an admin username:", QLineEdit::Normal, "admin", &ok);
    if (!ok) return false;   // user pressed Cancel — setup not completed
    username = username.trimmed();
    if (username.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Username cannot be empty.");
        return false;
    }

    // ── Password ──────────────────────────────────────────────────────────────
    QString password = QInputDialog::getText(this, "Create Admin Account",
        "Choose a password (min 4 characters):", QLineEdit::Password, "", &ok);
    if (!ok) return false;   // user pressed Cancel
    if (password.length() < 4) {
        QMessageBox::warning(this, "Validation", "Password must be at least 4 characters.");
        return false;
    }

    // ── Create admin role ─────────────────────────────────────────────────────
    Role adminRole;
    adminRole.name             = "Admin";
    adminRole.canManageUsers   = true;  adminRole.canViewReports   = true;
    adminRole.canAddCustomer   = true;  adminRole.canEditCustomer   = true;  adminRole.canDeleteCustomer = true;
    adminRole.canAddProduct    = true;  adminRole.canEditProduct    = true;  adminRole.canDeleteProduct  = true;
    adminRole.canAddOrder      = true;  adminRole.canEditOrder      = true;  adminRole.canCancelOrder    = true;
    adminRole.canDeleteOrders  = true;  adminRole.canManageRegions = true;  adminRole.canAccessSettings= true;
    // Phase 1: Enable all new permissions for Admin
    adminRole.canManagePurchases = true;
    adminRole.canAccessPOS       = true;
    adminRole.canManageRegister  = true;
    adminRole.canManageExpenses  = true;
    adminRole.canManageInventory = true;
    if (!m_roleRepo->save(adminRole)) {
        QMessageBox::critical(this, "Error",
            "Failed to save admin role to the database.\nPlease check the database and try again.");
        return false;
    }

    // ── Create admin user ─────────────────────────────────────────────────────
    User admin;
    admin.setName("Administrator");
    admin.setUsername(username);
    admin.setRoleId(adminRole.id);
    AuthService auth(m_userRepo);
    auth.setPassword(admin, password);
    if (!m_userRepo->save(admin)) {
        QMessageBox::critical(this, "Error",
            "Failed to save admin user to the database.\nPlease check the database and try again.");
        return false;
    }

    m_lastCreatedUsername = username;   // pre-fill login field after setup
    // C2: clear first-run flag (session-level guard for close button)
    m_isFirstRun = false;

    QMessageBox::information(this, "Admin Created",
        QString("Admin account <b>%1</b> created successfully.<br><br>"
                "Enter your password below and click Login.")
            .arg(username));
    return true;
}

// ── C2: Block X / Escape / Alt-F4 ONLY during first-run setup ────────────────
// The check fires only when m_isFirstRun is true, which is set exclusively when
// the constructor finds zero users in the DB.  Once showFirstRunSetup() creates
// the first account it clears the flag, so this guard never fires again — even
// across subsequent launches that open the same (now populated) DB.
void LoginDialog::closeEvent(QCloseEvent* event) {
    if (m_isFirstRun) {
        event->ignore();
        QMessageBox::warning(this, "Setup Required",
            "<b>You must create an admin account before closing this window.</b><br><br>"
            "The application cannot run without at least one user account.");
        QTimer::singleShot(0, this, &LoginDialog::showFirstRunSetup);
        return;
    }
    QDialog::closeEvent(event);
}

void LoginDialog::keyPressEvent(QKeyEvent* event) {
    QDialog::keyPressEvent(event);
}

bool LoginDialog::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        const QString key = keyEvent->text();

        // ── CRITICAL FIX: Only capture barcode when NO input field has focus ──
        // If user is typing in username/password, let them type normally.
        // Barcode scanner works ONLY when no field is focused (user is idle).
        QWidget* focusedWidget = QApplication::focusWidget();
        bool isTypingInField = (focusedWidget == m_userEdit || focusedWidget == m_passEdit);
        
        // If user is typing in a field, let the event pass through normally
        if (isTypingInField) {
            m_barcodeBuffer.clear();  // Clear any partial barcode data
            m_lastBarcodeKeyMs = 0;
            return QDialog::eventFilter(obj, event);
        }

        // ── Barcode capture logic (only when idle - no field focused) ─────────
        // A hardware scanner types its payload in a few milliseconds and ends
        // with Enter. A human types slowly. If the gap since the last keystroke
        // is long, this is a person using the keyboard normally — do NOT eat
        // the key, or the username/password fields become unusable.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastBarcodeKeyMs > 0 && (now - m_lastBarcodeKeyMs) > kBarcodeKeyGapMs)
            m_barcodeBuffer.clear();

        if (key == "\r" || key == "\n") {
            if (!m_barcodeBuffer.isEmpty()) {
                const QString scanned = m_barcodeBuffer;
                m_barcodeBuffer.clear();
                m_lastBarcodeKeyMs = 0;
                processBarcode(scanned);
                return true;  // Consume the Enter key
            }
            m_lastBarcodeKeyMs = 0;
            // Let Enter pass through to trigger Login button (default button)
            return QDialog::eventFilter(obj, event);
        }

        if (!key.isEmpty() && key.at(0).isPrint()) {
            m_barcodeBuffer += key;
            m_lastBarcodeKeyMs = now;
            return true;  // Consume the keystroke (part of barcode)
        }
    }
    return QDialog::eventFilter(obj, event);
}

// Shows scan feedback. Falls back to the error label / a message box when the
// animated SVG widget was not created (the SVG file is optional and missing it
// used to make every scan fail *silently* — no success, no error, nothing).
void LoginDialog::showBarcodeFeedback(const QString& html, bool success, int holdMs) {
    if (m_feedbackLabel && m_barcodeSvgWidget) {
        m_barcodeSvgWidget->hide();
        m_feedbackLabel->setStyleSheet(
            success ? "background: #D1FAE5; border: 2px solid #10B981; border-radius: 8px; padding: 10px;"
                    : "background: #FEE2E2; border: 2px solid #EF4444; border-radius: 8px; padding: 10px;");
        m_feedbackLabel->setText(html);
        m_feedbackLabel->setVisible(true);

        QTimer::singleShot(holdMs, this, [this]() {
            if (m_feedbackLabel && m_barcodeSvgWidget) {
                m_feedbackLabel->setVisible(false);
                m_barcodeSvgWidget->show();
            }
        });
        return;
    }

    if (m_errorLbl) {
        m_errorLbl->setText(html);
        m_errorLbl->setVisible(true);
        QTimer::singleShot(holdMs, this, [this]() {
            if (m_errorLbl) m_errorLbl->setText(QString());
        });
        return;
    }

    QMessageBox::information(this, "Attendance", html);
}

void LoginDialog::processBarcode(const QString& barcode) {
    const QString entered = barcode.trimmed();
    if (entered.isEmpty()) return;

    // FIX: Must select branch FIRST before attendance can work.
    if (!m_branchCombo) {
        Logger::instance().warn("[ATTENDANCE] No branch selector found");
        showBarcodeFeedback("<div style='text-align:center; font-size:12px; font-weight:bold; color:#991B1B;'>"
                            "⚠<br/>Please restart the application</div>", false, 2500);
        return;
    }

    int selectedBranchId = m_branchCombo->currentData().toInt();
    if (selectedBranchId <= 0) {
        Logger::instance().warn("[ATTENDANCE] No valid branch selected");
        showBarcodeFeedback("<div style='text-align:center; font-size:13px; font-weight:bold; color:#EF4444;'>"
                            "⚠️<br/>Please select a branch first<br/>"
                            "<span style='font-size:11px;'>من فضلك اختر الفرع أولاً</span></div>", false, 3000);
        return;
    }

    // FIX: Reconnect to the selected branch's database BEFORE checking barcode.
    // This is exactly what onLogin() does — we need the same logic here so
    // attendance reads from the SELECTED branch, not whatever was open before.
    BranchManager::instance().setActiveBranch(selectedBranchId);
    BranchInfo selectedBranch = BranchManager::instance().activeBranch();

    Logger::instance().info(QString("[ATTENDANCE] Switching to branch: %1 (ID=%2)")
        .arg(selectedBranch.name).arg(selectedBranchId));

    DatabaseConnectionManager::instance().closeConnection();
    if (!DatabaseConnectionManager::instance().openConnection()) {
        Logger::instance().error("[ATTENDANCE] Failed to connect to branch database");
        showBarcodeFeedback("<div style='text-align:center; font-size:12px; font-weight:bold; color:#991B1B;'>"
                            "⚠<br/>Cannot connect to<br/>branch database</div>", false, 2500);
        return;
    }

    Logger::instance().info(QString("[ATTENDANCE] Checking barcode: '%1' (normalized: '%2') in branch: %3")
        .arg(entered, BarcodeAuth::normalize(entered), selectedBranch.name));

    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    IUserRepository* userRepo = useSqlite
        ? static_cast<IUserRepository*>(new SQLiteUserRepository)
        : static_cast<IUserRepository*>(new AccessUserRepository);
    IAttendanceRepository* attendanceRepo = useSqlite
        ? static_cast<IAttendanceRepository*>(new SQLiteAttendanceRepository)
        : nullptr;

    if (!attendanceRepo) {
        Logger::instance().warn("[ATTENDANCE] No attendance repository for the current DB backend");
        showBarcodeFeedback("<div style='text-align:center; font-size:12px; font-weight:bold; color:#991B1B;'>"
                            "⚠<br/>Attendance is not available<br/>on this database</div>", false, 2500);
        delete userRepo;
        return;
    }

    User foundUser;
    const QList<User> allUsers = userRepo->getAll();
    Logger::instance().info(QString("[ATTENDANCE] Total users in branch '%1': %2")
        .arg(selectedBranch.name).arg(allUsers.size()));

    for (const auto& user : allUsers) {
        if (BarcodeAuth::matches(user.fingerprintBarcode(), entered)) {
            foundUser = user;
            Logger::instance().info(QString("[ATTENDANCE] Match found: %1 in branch: %2")
                .arg(user.name(), selectedBranch.name));
            break;
        }
    }

    if (foundUser.id() == 0) {
        Logger::instance().warn(QString("[ATTENDANCE] No user found with barcode: '%1' in branch: %2")
            .arg(entered, selectedBranch.name));
        showBarcodeFeedback("<div style='text-align:center; font-size:13px; font-weight:bold; color:#991B1B;'>"
                            "❌<br/>Invalid Barcode</div>", false, 1800);
        delete userRepo;
        delete attendanceRepo;
        return;
    }

    const Attendance lastAttendance = attendanceRepo->getLastByUserId(foundUser.id());
    Attendance::Type newType = Attendance::CheckIn;
    if (lastAttendance.id() > 0 && lastAttendance.type() == Attendance::CheckIn)
        newType = Attendance::CheckOut;

    Attendance newAttendance;
    newAttendance.setUserId(foundUser.id());
    newAttendance.setUserName(foundUser.name());
    newAttendance.setTimestamp(QDateTime::currentDateTime());
    newAttendance.setType(newType);

    if (attendanceRepo->save(newAttendance)) {
        QString iconPath;
        QString iconColor;
        QString typeText;
        
        if (newType == Attendance::CheckIn) {
            iconPath = sidebarSvgPath("login-svgrepo-com.svg");
            iconColor = "#10B981";  // green
            typeText = "Check-In";
        } else {
            iconPath = sidebarSvgPath("logout2-svgrepo-com.svg");
            iconColor = "#EF4444";  // red
            typeText = "Check-Out";
        }
        
        // Create icon pixmap and encode as base64 for HTML display
        QString iconHtml;
        if (!iconPath.isEmpty()) {
            QIcon icon = SvgIconHelper::icon(iconPath, QColor(iconColor), 36);
            QPixmap pixmap = icon.pixmap(36, 36);
            
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            pixmap.save(&buffer, "PNG");
            QString base64 = byteArray.toBase64();
            
            iconHtml = QString("<img src='data:image/png;base64,%1' width='36' height='36' style='display:block; margin:0 auto 6px;'/>")
                .arg(base64);
        }
        
        const QString color = (newType == Attendance::CheckIn) ? "#065F46" : "#991B1B";
        showBarcodeFeedback(QString("<div style='text-align:center; font-size:13px; font-weight:bold; color:%1; direction:ltr;'>"
                                    "%2"
                                    "<div style='margin-top:4px;'>%3</div>"
                                    "<div style='margin-top:2px;'>%4</div>"
                                    "<div style='margin-top:2px;'>%5</div>"
                                    "</div>")
                                .arg(color, iconHtml, foundUser.name(), typeText,
                                     newAttendance.timestamp().toString("hh:mm")),
                            true, 3000);
    } else {
        Logger::instance().error(QString("[ATTENDANCE] Failed to save record for: %1").arg(foundUser.name()));
        showBarcodeFeedback("<div style='text-align:center; font-size:12px; font-weight:bold; color:#991B1B;'>"
                            "⚠<br/>Could not save<br/>attendance record</div>", false, 2500);
    }

    delete userRepo;
    delete attendanceRepo;
}

// ── Helper to recolor SVG data ────────────────────────────────────────────────
// This is a simplified version of the logic in SvgIconHelper::recolorSvg
void LoginDialog::recolorSvgData(QByteArray& svg, const QColor& color) {
    const QByteArray hex = color.name(QColor::HexRgb).toUtf8(); // e.g. "#8a8fbf"

    // Replace hex colors in fill and stroke attributes
    auto replaceAttrColor = [&](const QByteArray& attr) {
        QByteArray search = attr + "=\"#";
        int pos = 0;
        while ((pos = svg.indexOf(search, pos)) != -1) {
            int start = pos + search.size();
            int end = svg.indexOf('"', start);
            if (end < 0) break;
            QByteArray oldColor = svg.mid(start - 1, end - start + 2);
            if (oldColor.toLower() != "\"none\"") {
                svg.replace(pos, end - pos + 1, attr + "=\"" + hex + "\"");
                pos += attr.size() + 3 + hex.size();
            } else {
                pos = end + 1;
            }
        }
    };

    replaceAttrColor("fill");
    replaceAttrColor("stroke");

    // Replace named colors
    QList<QByteArray> namedColors = {"black", "white", "#000", "#fff", "#000000", "#ffffff",
                                      "#030819", "#1a1a1a", "#333", "#333333", "#111918"};
    for (const QByteArray& named : namedColors) {
        svg.replace("fill=\"" + named + "\"", "fill=\"" + hex + "\"");
        svg.replace("stroke=\"" + named + "\"", "stroke=\"" + hex + "\"");
        svg.replace("fill:" + named + ";", "fill:" + hex + ";");
        svg.replace("fill:" + named + "}", "fill:" + hex + "}");
        svg.replace("stroke:" + named + ";", "stroke:" + hex + ";");
        svg.replace("stroke:" + named + "}", "stroke:" + hex + "}");
    }
}
