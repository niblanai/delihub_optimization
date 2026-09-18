#include "main_window.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "services/session_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QMessageBox>
#include "services/lang_manager.h"
#include "services/branch_manager.h"
#include "ui/translatable_page.h"
#include "ui/tr_helper.h"
#include "ui/dashboard/dashboard_page.h"
#include "ui/customers/customers_page.h"
#include "ui/products/products_page.h"
#include "ui/orders/orders_page.h"
#include "ui/scheduled_orders/scheduled_orders_page.h"
#include "ui/reports/reports_page.h"
#include "ui/users/users_page.h"
#include "ui/settings/settings_page.h"
#include "ui/coupons/coupons_page.h"
#include "ui/returns/returns_page.h"
#include "ui/audit_log/audit_log_page.h"
#include "ui/reports/reports_page.h"
#include "ui/notes/notes_page.h"
// Phase 1 pages
#include "ui/suppliers/suppliers_page.h"
#include "ui/purchases/purchases_page.h"
#include "ui/register/register_page.h"
#include "ui/expenses/expenses_page.h"
#include "ui/inventory/inventory_page.h"
#include "services/audit_service.h"
#include "services/notification_sound_player.h"
#include <QPointer>
#include <QApplication>
#include <QIcon>
#include <QComboBox>
#include <QPixmap>
#include <QCoreApplication>
#include <QFile>
#include <QShortcut>
#include <QTime>
#include <QTimer>
#include <QSettings>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QScrollArea>
#include <QMouseEvent>
#include <QGraphicsBlurEffect>
#include <QPainter>

// ─────────────────────────────────────────────────────────────────────────────
// Resolve path to a sidebar SVG icon.
// Search order:
//   1. dist/sidebar/<file>              (installed app & dist run)
//   2. dist/../src/sidebar/<file>       (development run from build/)
// ─────────────────────────────────────────────────────────────────────────────
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    // Installed / dist run
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    // Development run (build dir → src/sidebar)
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("DeliHub — Delivery Management System");
    setMinimumSize(1100, 700);
    resize(1280, 780);
    applyStylesheet();
    setupUi();

    // Task 7: start notification sound player (lives for the lifetime of the window)
    new NotificationSoundPlayer(this);
    
    // Load icons with correct colors on startup
    QTimer::singleShot(0, this, [this]() {
        refreshNavIcons();
    });
}

void MainWindow::setupUi() {
    m_central = new QWidget;
    setCentralWidget(m_central);
    // Central uses a horizontal layout:
    // [HamburgerStrip (54px fixed)] | [Pages (remaining width)]
    auto* centralLayout = new QHBoxLayout(m_central);
    centralLayout->setSpacing(0);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    // ── Left strip: hamburger button (fixed 54px wide) ────────────────────────
    auto* hbStrip = new QWidget(m_central);
    hbStrip->setFixedWidth(54);
    hbStrip->setObjectName("hbStrip");
    // Transparent — inherits page background
    hbStrip->setAttribute(Qt::WA_TranslucentBackground);
    centralLayout->addWidget(hbStrip);

    // ── Pages container ────────────────────────────────────────────────────────
    auto* pagesWrapper = new QWidget(m_central);
    pagesWrapper->setObjectName("pagesWrapper");
    centralLayout->addWidget(pagesWrapper, 1);

    // ── Banner ────────────────────────────────────────────────────────────────
    setupFallbackBanner();

    m_pages = new QWidget(pagesWrapper);
    m_pages->setObjectName("pagesContainer");
    auto* pagesRoot = new QVBoxLayout(m_pages);
    pagesRoot->setSpacing(0);
    pagesRoot->setContentsMargins(0, 0, 0, 0);
    pagesRoot->addWidget(m_banner);
    m_banner->setVisible(DatabaseConnectionManager::instance().isSqliteFallbackActive());

    m_pageStack = new QStackedWidget(m_pages);
    m_pageStack->setObjectName("pagesStack");
    pagesRoot->addWidget(m_pageStack, 1);

    // Give pagesWrapper a simple layout that fills pagesWrapper
    auto* wrapLayout = new QVBoxLayout(pagesWrapper);
    wrapLayout->setSpacing(0);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->addWidget(m_pages);

    // Keep a handle on pagesWrapper for openSidebar()/closeSidebar()
    m_pagesWrapper = pagesWrapper;

    // Real blur effect on the page content — off by default, only
    // enabled while the sidebar is open (see openSidebar/closeSidebar)
    m_pagesBlur = new QGraphicsBlurEffect(m_pagesWrapper);
    m_pagesBlur->setBlurRadius(0);
    m_pagesBlur->setBlurHints(QGraphicsBlurEffect::PerformanceHint);
    m_pagesBlur->setEnabled(false);
    m_pagesWrapper->setGraphicsEffect(m_pagesBlur);

    m_blurAnim = new QPropertyAnimation(m_pagesBlur, "blurRadius", this);
    m_blurAnim->setDuration(260);
    m_blurAnim->setEasingCurve(QEasingCurve::OutCubic);

    // Pages: 0=Dashboard, 1=Customers, 2=Products, 3=Orders, 4=Scheduled,
    //        5=Reports, 6=Returns, 7=Coupons, 8=Users, 9=Settings, 10=AuditLog, 11=Notes
    //        12=Suppliers, 13=Purchases, 14=Register, 15=Expenses, 16=Inventory
    m_pageStack->addWidget(new DashboardPage);
    m_pageStack->addWidget(new CustomersPage);
    m_pageStack->addWidget(new ProductsPage);
    m_pageStack->addWidget(new OrdersPage);
    auto* schedPage = new ScheduledOrdersPage;
    m_pageStack->addWidget(schedPage);
    m_pageStack->addWidget(new ReportsPage);
    m_pageStack->addWidget(new ReturnsPage);
    m_pageStack->addWidget(new CouponsPage);
    m_pageStack->addWidget(new UsersPage);
    m_pageStack->addWidget(new SettingsPage);
    m_pageStack->addWidget(new AuditLogPage);   // index 10
    m_pageStack->addWidget(new NotesPage);      // index 11
    // Phase 1 pages
    m_pageStack->addWidget(new SuppliersPage);  // index 12
    m_pageStack->addWidget(new PurchasesPage);  // index 13
    m_pageStack->addWidget(new RegisterPage);   // index 14
    m_pageStack->addWidget(new ExpensesPage);   // index 15
    m_pageStack->addWidget(new InventoryPage);  // index 16

    // Wire AuditLogPage undo → refresh affected data pages
    if (auto* alp = qobject_cast<AuditLogPage*>(m_pageStack->widget(10))) {
        connect(alp, &AuditLogPage::dataRestored, this, [this](const QString& entityType) {
            if (entityType == "Product" || entityType == "Category") {
                if (auto* pp = qobject_cast<ProductsPage*>(m_pageStack->widget(2)))
                    pp->refresh();
            }
            if (entityType == "Customer") {
                if (auto* cp = qobject_cast<CustomersPage*>(m_pageStack->widget(1)))
                    cp->refresh();
            }
            if (entityType == "Order") {
                if (auto* op = qobject_cast<OrdersPage*>(m_pageStack->widget(3)))
                    op->refresh();
            }
        });
    }

    // Wire ScheduledOrders → switch to Orders tab on convert
    connect(schedPage, &ScheduledOrdersPage::orderCreated, this, [this](int orderId) {
        navigateTo(3);
        QTimer::singleShot(50, this, [this, orderId]() {
            if (auto* op = qobject_cast<OrdersPage*>(m_pageStack->widget(3)))
                op->openOrderForEdit(orderId);
        });
    });

    // Overlay backdrop
    // NOTE: the dim colour is painted by hand in eventFilter() (QEvent::Paint).
    // QPalette/setAutoFillBackground() are NOT reliable here: the app-wide theme
    // stylesheet matches plain QWidgets and repaints them with an opaque
    // background, silently overriding whatever colour is set on the palette —
    // that's exactly why the overlay was showing up as a solid theme-coloured
    // block instead of a translucent dim. Painting manually skips the style
    // system entirely, so it can't be turned solid again.
    m_overlay = new QWidget(m_central);
    m_overlay->setObjectName("sidebarOverlay");
    m_overlay->setAttribute(Qt::WA_NoSystemBackground, true);
    m_overlay->hide();
    m_overlay->installEventFilter(this);

    // ── Sidebar drawer ────────────────────────────────────────────────────────
    setupSidebar();
    m_sidebar->setFixedWidth(SIDEBAR_W);
    m_sidebar->move(-SIDEBAR_W, 0);
    m_sidebar->raise();

    // ── Hamburger button inside the left strip ────────────────────────────────
    m_hamburgerBtn = new QPushButton(hbStrip);
    m_hamburgerBtn->setObjectName("hamburgerBtn");
    m_hamburgerBtn->setFixedSize(44, 44);
    m_hamburgerBtn->move(5, 10);   // fixed position inside strip
    m_hamburgerBtn->show();
    {
        QString hbSvg = sidebarSvgPath("side bar.svg");
        QColor iconColor(ThemeManager::instance().tokens().textPrimary);
        QIcon ico = SvgIconHelper::icon(hbSvg, iconColor, 34);
        if (!ico.isNull()) { m_hamburgerBtn->setIcon(ico); m_hamburgerBtn->setIconSize(QSize(34,34)); }
        else                { m_hamburgerBtn->setText("☰"); }
    }
    m_hamburgerBtn->setToolTip("Menu");
    m_hamburgerBtn->raise();
    connect(m_hamburgerBtn, &QPushButton::clicked, this, [this](){
        m_sidebarOpen ? closeSidebar() : openSidebar();
    });
    // Recolor hamburger icon on theme change
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](ThemeType){
        QString hbSvg = sidebarSvgPath("side bar.svg");
        QColor iconColor(ThemeManager::instance().tokens().textPrimary);
        QIcon ico = SvgIconHelper::icon(hbSvg, iconColor, 34);
        if (!ico.isNull()) { m_hamburgerBtn->setIcon(ico); m_hamburgerBtn->setIconSize(QSize(34,34)); m_hamburgerBtn->setText(""); }
    });

    // ── Animations ────────────────────────────────────────────────────────────
    m_slideAnim = new QPropertyAnimation(m_sidebar, "pos", this);
    m_slideAnim->setDuration(260);
    m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    new QShortcut(QKeySequence("Ctrl+1"), this, [this](){ navigateTo(0); });
    new QShortcut(QKeySequence("Ctrl+2"), this, [this](){ navigateTo(1); });
    new QShortcut(QKeySequence("Ctrl+3"), this, [this](){ navigateTo(2); });
    new QShortcut(QKeySequence("Ctrl+4"), this, [this](){ navigateTo(3); });
    new QShortcut(QKeySequence("Ctrl+5"), this, [this](){ navigateTo(4); });
    new QShortcut(QKeySequence("Ctrl+6"), this, [this](){ navigateTo(5); });
    new QShortcut(QKeySequence("Escape"), this, [this](){ if (m_sidebarOpen) closeSidebar(); });

    // ── Language changes ──────────────────────────────────────────────────────
    connect(&LangManager::instance(), &LangManager::languageChanged, this, [this]() {
        auto& L = LangManager::instance();
        for (auto& e : m_navEntries)
            e.btn->setText("  " + L.t(e.label));
        for (int i = 0; i < m_pageStack->count(); ++i)
            retranslateWidget(m_pageStack->widget(i));
        retranslateWidget(m_sidebar);
        for (int i = 0; i < m_pageStack->count(); ++i)
            if (auto* tp = dynamic_cast<TranslatablePage*>(m_pageStack->widget(i)))
                tp->retranslateUi();
    });

    navigateTo(0);

    // Trigger initial layout now that all children exist
    QTimer::singleShot(0, this, [this](){
        const int h = m_central->height();
        if (m_overlay) m_overlay->setGeometry(0, 0, m_central->width(), h);
    });
}

void MainWindow::setupSidebar() {
    auto& L  = LangManager::instance();
    const auto& tk = ThemeManager::instance().tokens();

    // Sidebar is a child of m_central — positioned as overlay drawer
    m_sidebar = new QWidget(m_central);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(SIDEBAR_W);
    m_sidebar->hide();   // starts hidden

    auto* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setSpacing(0);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);

    // ── HEADER ────────────────────────────────────────────────────────────────
    auto* headerWidget = new QWidget;
    headerWidget->setObjectName("sidebarHeader");
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setSpacing(6);
    headerLayout->setContentsMargins(16, 16, 16, 12);

    // Logo + brand row
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(10);
    topRow->setContentsMargins(0, 0, 0, 0);

    QString logoPath = QCoreApplication::applicationDirPath() + "/logo.png";
    if (!QFile::exists(logoPath))
        logoPath = QCoreApplication::applicationDirPath() + "/../src/logo.png";
    if (QFile::exists(logoPath)) {
        auto* logoLbl = new QLabel;
        QPixmap px(logoPath);
        logoLbl->setPixmap(px.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLbl->setAlignment(Qt::AlignCenter);
        logoLbl->setFixedSize(32, 32);
        // Transparent background — no colored box behind the logo
        logoLbl->setStyleSheet("background:transparent;");
        topRow->addWidget(logoLbl);
    }
    auto* brand = new QLabel("DeliHub");
    brand->setObjectName("brandLabel");
    brand->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    topRow->addWidget(brand, 1);

    // Close button (X icon)
    auto* closeBtn = new QPushButton;
    closeBtn->setObjectName("iconBtn");
    closeBtn->setFixedSize(28, 28);
    {
        QString closeSvg = sidebarSvgPath("close-svgrepo-com.svg");
        QColor iconColor(tk.navText);
        QIcon ico = SvgIconHelper::icon(closeSvg, iconColor, 16);
        if (!ico.isNull()) { closeBtn->setIcon(ico); closeBtn->setIconSize(QSize(16, 16)); }
        else closeBtn->setText("✕");
    }
    closeBtn->setToolTip("Close");
    closeBtn->setStyleSheet("background:transparent; border:none;");
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::closeSidebar);
    topRow->addWidget(closeBtn);
    headerLayout->addLayout(topRow);

    // Metallic separator
    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("sidebarMetalSep");
    headerLayout->addWidget(sep1);

    // User info — centered
    const User& currentUser = SessionManager::instance().currentUser();
    const Role& currentRole = SessionManager::instance().currentRole();
    QString displayName = currentUser.name().isEmpty()
        ? currentUser.username() : currentUser.name();

    auto* userLbl = new QLabel("👤  " + displayName);
    userLbl->setObjectName("currentUserLabel");
    userLbl->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(userLbl);

    if (!currentRole.name.isEmpty()) {
        auto* roleLbl = new QLabel(currentRole.name);
        roleLbl->setObjectName("sidebarRoleLabel");
        roleLbl->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(roleLbl);
    }
    QString branchName = BranchManager::instance().activeBranch().name;
    if (branchName.isEmpty()) branchName = ConfigManager::instance().branchName();
    if (!branchName.isEmpty()) {
        auto* branchLbl = new QLabel("🏢  " + branchName);
        branchLbl->setObjectName("sidebarBranchLabel");
        branchLbl->setAlignment(Qt::AlignCenter);
        branchLbl->setWordWrap(true);
        headerLayout->addWidget(branchLbl);
    }

    sidebarLayout->addWidget(headerWidget);

    // Metallic separator
    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("sidebarMetalSep");
    sidebarLayout->addWidget(sep2);

    // ── NAV ───────────────────────────────────────────────────────────────────
    auto* navContainer = new QWidget;
    navContainer->setObjectName("navContainer");
    auto* navLayout = new QVBoxLayout(navContainer);
    navLayout->setSpacing(2);
    navLayout->setContentsMargins(8, 8, 8, 8);
    navLayout->setAlignment(Qt::AlignTop);

    auto svgPath = [&](const QString& file) -> QString {
        QString p = sidebarSvgPath(file);
        return QFile::exists(p) ? p : QString();
    };

    struct NavDef { QString svgFile; QString key; int idx; QPushButton** ptr; bool adminOnly; };
    QList<NavDef> defs = {
        {"dashboard-svgrepo-com.svg",                 "Dashboard",        0,  &m_btnDashboard, false},
        {"users-svgrepo-com.svg",                     "Customers",        1,  &m_btnCustomers, false},
        {"product-svgrepo-com.svg",                   "Products",         2,  &m_btnProducts,  false},
        {"cart-shopping-fast-svgrepo-com.svg",         "Orders",           3,  &m_btnOrders,    false},
        {"schedule-svgrepo-com.svg",                  "Scheduled Orders", 4,  &m_btnScheduled, false},
        {"report-svgrepo-com.svg",                "Reports",          5,  &m_btnReports,   false},
        {"return-svgrepo-com.svg",                    "Returns",          6,  &m_btnReturns,   false},
        {"coupon-pass-ticket-voucher-svgrepo-com.svg", "Coupons",          7,  &m_btnCoupons,   false},
        {"user-svgrepo-com.svg",                      "Users",            8,  &m_btnUsers,     true},
        {"setting-5-svgrepo-com.svg",                 "Settings",         9,  &m_btnSettings,  true},
        {"audit-svgrepo-com.svg",                     "Audit Log",        10, &m_btnAuditLog,  true},
        {"note-dark-svgrepo-com.svg",                 "Notes",            11, &m_btnNotes,     false},
        // Phase 1 navigation
        {"truck-svgrepo-com.svg",                     "Suppliers",        12, &m_btnSuppliers, false},
        {"invoice-receipt-svgrepo-com.svg",           "Purchases",        13, &m_btnPurchases, false},
        {"cash-register-svgrepo-com.svg",             "Register/Till",    14, &m_btnRegister,  false},
        {"miscellaneous-expenses-svgrepo-com.svg",    "Expenses",         15, &m_btnExpenses,  false},
        {"corp-purchase-svgrepo-com.svg",             "Inventory",        16, &m_btnInventory, false},
    };
    const bool isAdmin = SessionManager::instance().isAdmin();
    QColor iconColor(tk.navText);

    m_navEntries.clear();
    for (auto& d : defs) {
        auto* btn = new QPushButton;
        btn->setObjectName("navBtn");
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QString sp = svgPath(d.svgFile);
        if (!sp.isEmpty()) {
            QIcon ico = SvgIconHelper::icon(sp, iconColor, 18);
            if (!ico.isNull()) { btn->setIcon(ico); btn->setIconSize(QSize(18, 18)); }
        }
        btn->setText("  " + L.t(d.key));
        if (d.adminOnly) btn->setVisible(isAdmin);
        *d.ptr = btn;
        navLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, idx = d.idx]{ navigateTo(idx); });
        m_navEntries.append({btn, sp, d.svgFile, d.key});
    }

    auto* navScroll = new QScrollArea;
    navScroll->setObjectName("navScrollArea");
    navScroll->setWidget(navContainer);
    navScroll->setWidgetResizable(true);
    navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navScroll->setFrameShape(QFrame::NoFrame);
    sidebarLayout->addWidget(navScroll, 1);

    // Metallic separator
    auto* sep3 = new QFrame;
    sep3->setFrameShape(QFrame::HLine);
    sep3->setObjectName("sidebarMetalSep");
    sidebarLayout->addWidget(sep3);

    // ── FOOTER ────────────────────────────────────────────────────────────────
    auto* footerWidget = new QWidget;
    footerWidget->setObjectName("sidebarFooter");
    auto* footerLayout = new QVBoxLayout(footerWidget);
    footerLayout->setSpacing(4);
    footerLayout->setContentsMargins(10, 6, 10, 8);

    auto* clockLbl = new QLabel(QTime::currentTime().toString("hh:mm:ss"));
    clockLbl->setObjectName("clockLabel");
    clockLbl->setAlignment(Qt::AlignCenter);
    auto* clockTimer = new QTimer(clockLbl);
    QPointer<QLabel> safeClk(clockLbl);
    connect(clockTimer, &QTimer::timeout, [safeClk](){
        if (safeClk) safeClk->setText(QTime::currentTime().toString("hh:mm:ss"));
    });
    clockTimer->start(1000);
    footerLayout->addWidget(clockLbl);

    // Theme selector removed - use Settings page for theme customization
    
    auto* langLabel = new QLabel(L.t("Language"));
    langLabel->setObjectName("sidebarHint");
    footerLayout->addWidget(langLabel);
    auto* langCombo = new QComboBox;
    langCombo->addItem("English","en"); langCombo->addItem("عربي","ar");
    langCombo->blockSignals(true);
    int li = langCombo->findData(ConfigManager::instance().language());
    if (li >= 0) langCombo->setCurrentIndex(li);
    langCombo->blockSignals(false);
    footerLayout->addWidget(langCombo);
    connect(langCombo, &QComboBox::currentTextChanged, this, [this, langCombo](const QString&){
        QString code = langCombo->currentData().toString();
        LangManager::instance().setLanguage(code);
        ConfigManager::instance().setLanguage(code);
        updateNavButtonLabels();
    });

    // Logout
    auto* logoutBtn = new QPushButton;
    logoutBtn->setObjectName("dangerBtn");
    logoutBtn->setFixedHeight(36);
    {
        QIcon logIco = SvgIconHelper::icon(sidebarSvgPath("logout-bracket-svgrepo-com.svg"),
                                            QColor("#FFFFFF"), 16);
        if (!logIco.isNull()) { logoutBtn->setIcon(logIco); logoutBtn->setIconSize(QSize(16,16)); }
        logoutBtn->setText("  " + L.t("Logout"));
    }
    connect(logoutBtn, &QPushButton::clicked, this, [this](){
        AuditService::instance().logLogout(SessionManager::instance().currentUser().username());
        SessionManager::instance().logout();
        emit logoutRequested();
    });
    footerLayout->addWidget(logoutBtn);

    auto* verLbl = new QLabel("v2.3.0");
    verLbl->setObjectName("versionLabel");
    verLbl->setAlignment(Qt::AlignCenter);
    footerLayout->addWidget(verLbl);

    sidebarLayout->addWidget(footerWidget);

    // Refresh icons on theme change
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeType){ refreshNavIcons(); });
}

void MainWindow::setupFallbackBanner() {
    m_banner = new QFrame;
    m_banner->setObjectName("fallbackBanner");
    m_banner->setFixedHeight(42);

    auto* layout = new QHBoxLayout(m_banner);
    layout->setContentsMargins(16, 0, 16, 0);

    auto* icon = new QLabel("⚠️");
    icon->setFixedWidth(24);

    auto* text = new QLabel(
        "<b>Warning:</b> Running on local SQLite database — "
        "not connected to the shared branch database. "
        "Data entered here will not be shared with other branches."
    );
    text->setWordWrap(false);
    text->setObjectName("bannerText");

    layout->addWidget(icon);
    layout->addWidget(text, 1);
}

void MainWindow::navigateTo(int pageIndex) {
    // ── Update sidebar button visibility based on role permissions ──────────
    // (runs on every navigation so it stays current if role changes mid-session)
    const Role& role = SessionManager::instance().currentRole();
    if (m_btnDashboard)
        m_btnDashboard->setVisible(role.canAccessDashboard);

    // ── Permission check — silently redirect instead of showing a warning ───
    if (pageIndex == 0 && !role.canAccessDashboard) {
        // Navigate to the first page the user can access
        navigateTo(1);   // Customers is always visible
        return;
    }

    m_currentPage = pageIndex;
    m_pageStack->setCurrentIndex(pageIndex);

    // Update checked state on all nav buttons
    QList<QPushButton*> btns = {
        m_btnDashboard, m_btnCustomers, m_btnProducts, m_btnOrders,
        m_btnScheduled, m_btnReports, m_btnReturns, m_btnCoupons,
        m_btnUsers, m_btnSettings, m_btnAuditLog, m_btnNotes,
        m_btnSuppliers, m_btnPurchases, m_btnRegister, m_btnExpenses, m_btnInventory
    };
    for (int i = 0; i < btns.size(); ++i)
        if (btns[i]) btns[i]->setChecked(i == pageIndex);

    // Close the overlay sidebar after navigation
    if (m_sidebarOpen) closeSidebar();

    if (pageIndex == 0) {
        if (auto* dp = qobject_cast<DashboardPage*>(m_pageStack->widget(0)))
            dp->refresh();
    } else if (pageIndex == 1) {
        if (auto* cp = qobject_cast<CustomersPage*>(m_pageStack->widget(1)))
            cp->refresh();
    } else if (pageIndex == 2) {
        if (auto* pp = qobject_cast<ProductsPage*>(m_pageStack->widget(2)))
            pp->refresh();
    } else if (pageIndex == 3) {
        if (auto* op = qobject_cast<OrdersPage*>(m_pageStack->widget(3)))
            op->refresh();
    } else if (pageIndex == 4) {
        if (auto* sp = qobject_cast<ScheduledOrdersPage*>(m_pageStack->widget(4)))
            sp->refresh();
    } else if (pageIndex == 5) {
        if (auto* rp = qobject_cast<ReportsPage*>(m_pageStack->widget(5)))
            rp->refresh();
    } else if (pageIndex == 6) {
        if (auto* rtp = qobject_cast<ReturnsPage*>(m_pageStack->widget(6)))
            rtp->refresh();
    } else if (pageIndex == 7) {
        if (auto* cop = qobject_cast<CouponsPage*>(m_pageStack->widget(7)))
            cop->refresh();
    } else if (pageIndex == 8) {
        if (auto* up = qobject_cast<UsersPage*>(m_pageStack->widget(8)))
            up->refresh();
    } else if (pageIndex == 9) {
        if (auto* stp = qobject_cast<SettingsPage*>(m_pageStack->widget(9)))
            stp->refresh();
    } else if (pageIndex == 10) {
        if (auto* alp = qobject_cast<AuditLogPage*>(m_pageStack->widget(10)))
            alp->refresh();
    } else if (pageIndex == 11) {
        if (auto* np = qobject_cast<NotesPage*>(m_pageStack->widget(11)))
            np->refresh();
    }
}

void MainWindow::applyStylesheet() {
    // Use ThemeManager — respects user's saved preference
    ThemeManager::instance().applyTheme(ConfigManager::instance().theme());
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlay drawer — open / close with slide + fade animation
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openSidebar() {
    if (m_sidebarOpen || !m_sidebar || !m_overlay) return;
    m_sidebarOpen = true;

    const int h = m_central->height();
    m_sidebar->setFixedHeight(h);

    // Overlay covers the FULL central area behind the sidebar
    // (including the strip on the left so clicks anywhere close the sidebar)
    m_overlay->setGeometry(0, 0, m_central->width(), h);
    m_overlay->show();
    m_overlay->raise();
    m_sidebar->move(-SIDEBAR_W, 0);
    m_sidebar->show();
    m_sidebar->raise();
    if (m_hamburgerBtn) m_hamburgerBtn->raise();

    // Slide in
    m_slideAnim->stop();
    m_slideAnim->setStartValue(QPoint(-SIDEBAR_W, 0));
    m_slideAnim->setEndValue(QPoint(0, 0));
    m_slideAnim->start();

    // Blur the page content behind the sidebar
    if (m_pagesBlur && m_blurAnim) {
        m_pagesBlur->setEnabled(true);
        m_blurAnim->stop();
        m_blurAnim->setStartValue(m_pagesBlur->blurRadius());
        m_blurAnim->setEndValue(18.0);   // tweak to taste (12–24 looks good)
        m_blurAnim->start();
    }
}

void MainWindow::closeSidebar() {
    if (!m_sidebarOpen || !m_sidebar || !m_overlay) return;
    m_sidebarOpen = false;

    // Slide out to left
    m_slideAnim->stop();
    m_slideAnim->setStartValue(m_sidebar->pos());
    m_slideAnim->setEndValue(QPoint(-SIDEBAR_W, 0));
    connect(m_slideAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_sidebarOpen && m_sidebar) m_sidebar->hide();
    }, Qt::SingleShotConnection);
    m_slideAnim->start();

    // Hide overlay when slide-out animation finishes
    connect(m_slideAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_sidebarOpen && m_overlay) m_overlay->hide();
    }, Qt::SingleShotConnection);

    // Un-blur the page content
    if (m_pagesBlur && m_blurAnim) {
        m_blurAnim->stop();
        m_blurAnim->setStartValue(m_pagesBlur->blurRadius());
        m_blurAnim->setEndValue(0.0);
        connect(m_blurAnim, &QPropertyAnimation::finished, this, [this]() {
            if (!m_sidebarOpen && m_pagesBlur) m_pagesBlur->setEnabled(false);
        }, Qt::SingleShotConnection);
        m_blurAnim->start();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (!m_central) return;
    const int h = m_central->height();

    // Overlay fills entire central widget (covers strip + pages)
    if (m_overlay) m_overlay->setGeometry(0, 0, m_central->width(), h);

    // Sidebar height tracks window height
    if (m_sidebar && m_sidebarOpen) m_sidebar->setFixedHeight(h);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_overlay) {
        // Paint the dim backdrop ourselves — see the comment where m_overlay
        // is created in setupUi() for why this can't go through QSS/palette.
        if (event->type() == QEvent::Paint) {
            QPainter p(m_overlay);
            p.fillRect(m_overlay->rect(), QColor(0, 0, 0, 110));
            return true;
        }
        // Click on overlay → close sidebar
        if (event->type() == QEvent::MouseButtonPress) {
            closeSidebar();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateNavButtonLabels() {
    auto& L = LangManager::instance();
    for (auto& e : m_navEntries) {
        e.btn->setText("  " + L.t(e.label));
        e.btn->setToolTip(QString());
        e.btn->setStyleSheet(QString());
    }
}

void MainWindow::refreshNavIcons() {
    // Use automatic icon coloring based on svgIconMode setting
    for (auto& e : m_navEntries) {
        if (!e.svgPath.isEmpty()) {
            QIcon ico = SvgIconHelper::icon(e.svgPath, 18);  // Auto mode
            if (!ico.isNull()) {
                e.btn->setIcon(ico);
                e.btn->setIconSize(QSize(18, 18));
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Ensure all timers, threads and background objects are fully torn down
    // so the process doesn't linger after the user clicks the X button.
    QApplication::quit();
    QMainWindow::closeEvent(event);
}


