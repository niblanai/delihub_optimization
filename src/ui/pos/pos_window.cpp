#include "pos_window.h"
#include "pos_payment_dialog.h"
#include "numeric_keypad.h"
#include "receipt_preview_dialog.h"
#include "ui/customers/customer_dialog.h"
#include "ui/svg_icon_helper.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "services/receipt_printer.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "services/barcode_auth.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "core/order.h"
#include "core/stock_movement.h"
#include "core/user.h"
#include "core/role.h"
#include "core/promotion.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QPointer>
#include <QLabel>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QStackedWidget>
#include <QFrame>
#include <QPixmap>
#include <QFile>
#include <QMouseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QSqlQuery>
#include <functional>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

PosWindow::PosWindow(QWidget* parent) : QMainWindow(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_productRepo = useSqlite ? static_cast<IProductRepository*>(new SQLiteProductRepository)
                              : static_cast<IProductRepository*>(new AccessProductRepository);
    m_customerRepo = useSqlite ? static_cast<ICustomerRepository*>(new SQLiteCustomerRepository)
                               : static_cast<ICustomerRepository*>(new AccessCustomerRepository);
    m_orderRepo = useSqlite ? static_cast<IOrderRepository*>(new SQLiteOrderRepository)
                            : static_cast<IOrderRepository*>(new AccessOrderRepository);
    m_regionRepo = useSqlite ? static_cast<IRegionRepository*>(new SQLiteRegionRepository)
                             : static_cast<IRegionRepository*>(new AccessRegionRepository);
    m_sessionRepo = useSqlite ? static_cast<IRegisterSessionRepository*>(new SQLiteRegisterSessionRepository)
                              : static_cast<IRegisterSessionRepository*>(new AccessRegisterSessionRepository);
    m_stockMovementRepo = useSqlite ? static_cast<IStockMovementRepository*>(new SQLiteStockMovementRepository)
                                    : static_cast<IStockMovementRepository*>(new AccessStockMovementRepository);
    m_promotionRepo = useSqlite ? static_cast<IPromotionRepository*>(new SQLitePromotionRepository)
                                : static_cast<IPromotionRepository*>(new AccessPromotionRepository);
    
    // Apply POS-specific stylesheet for better text visibility
    setStyleSheet(
        "QWidget { color: #1F2937; }"
        "QLabel { color: #1F2937; }"
        "QPushButton { color: #1F2937; }"
        "QLineEdit { color: #1F2937; background: white; }"
        "QListWidget { color: #1F2937; background: white; }"
    );
    
    setupUi();
    setWindowTitle("DeliHub - Point of Sale");
    resize(1400, 900);
    showMaximized();
}

PosWindow::~PosWindow() {
    delete m_productRepo;
    delete m_customerRepo;
    delete m_orderRepo;
    delete m_regionRepo;
    delete m_sessionRepo;
    delete m_stockMovementRepo;
    delete m_promotionRepo;
}

void PosWindow::setRegisterSession(const RegisterSession& session) {
    m_session = session;
    m_sessionLbl->setText(QString("Session #%1 | Opened: %2").arg(session.id()).arg(session.openedAt().toString("HH:mm")));
    
    if (session.id() <= 0 || session.status() != RegisterSession::Status::Open) {
        QMessageBox::critical(this, "Error", "No active register session.\n\nPlease open a session first from the admin panel.");
        close();
        return;
    }
    
    // Load products asynchronously after window is shown
    QTimer::singleShot(100, this, [this]() {
        loadProducts();
    });
}

void PosWindow::setupUi() {
    // Get theme
    auto& themeMgr = ThemeManager::instance();
    auto tokens = themeMgr.tokens();
    
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    auto* root = new QHBoxLayout(m_centralWidget);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);
    
    // === LEFT PANEL: Products Grid ===
    auto* leftPanel = new QWidget;
    leftPanel->setMinimumWidth(800);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(10);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    
    // Search bar
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search products or scan barcode..."));
    m_searchEdit->setFixedHeight(42);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  padding: 10px 15px;"
        "  font-size: 14px;"
        "  border: 2px solid #E5E7EB;"
        "  border-radius: 8px;"
        "  background: white;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #3B82F6;"
        "  outline: none;"
        "  box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.1);"
        "}"
    );
    m_searchEdit->installEventFilter(this);
    leftLayout->addWidget(m_searchEdit);
    
    // Products grid (scrollable)
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: #F3F4F6;"
        "  width: 8px;"
        "  border-radius: 4px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #9CA3AF;"
        "  min-height: 30px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #6B7280;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );
    
    auto* gridContainer = new QWidget;
    m_productGrid = new QGridLayout(gridContainer);
    m_productGrid->setSpacing(10);
    scrollArea->setWidget(gridContainer);
    leftLayout->addWidget(scrollArea, 1);
    
    root->addWidget(leftPanel, 3);
    
    // === RIGHT PANEL: Stacked Views (Cart → Payment → Success) ===
    m_rightStack = new QStackedWidget;
    m_rightStack->setMinimumWidth(500);
    m_rightStack->setMaximumWidth(600);
    
    // ─── View 0: Cart View ───────────────────────────────────────────────────
    m_cartView = new QWidget;
    auto* cartLayout = new QVBoxLayout(m_cartView);
    cartLayout->setSpacing(10);
    cartLayout->setContentsMargins(20, 20, 20, 20);
    
    // Customer section
    auto* custFrame = new QFrame;
    custFrame->setStyleSheet("QFrame { background: transparent; border: none; padding: 8px 0px; }");
    auto* custLayout = new QHBoxLayout(custFrame);
    custLayout->setSpacing(10);
    custLayout->setContentsMargins(0, 0, 0, 0);
    custLayout->setAlignment(Qt::AlignVCenter);
    
    auto* customerIcon = new QLabel;
    QPixmap userIcon = SvgIconHelper::icon(sidebarSvgPath("user-svgrepo-com.svg"), 
                                            QColor(tokens.textPrimary), 28).pixmap(28, 28);
    customerIcon->setPixmap(userIcon);
    customerIcon->setFixedSize(28, 28);
    customerIcon->setScaledContents(true);
    customerIcon->setAlignment(Qt::AlignCenter);
    
    m_customerLbl = new QLabel("Walk-in Customer");
    m_customerLbl->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_customerLbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    
    m_selectCustomerBtn = new QPushButton("Select Customer");
    m_selectCustomerBtn->setObjectName("secondaryBtn");
    m_selectCustomerBtn->setMinimumHeight(36);
    m_selectCustomerBtn->setMaximumHeight(36);
    
    custLayout->addWidget(customerIcon);
    custLayout->addWidget(m_customerLbl);
    custLayout->addStretch();
    custLayout->addWidget(m_selectCustomerBtn);
    cartLayout->addWidget(custFrame);
    
    // Cart list
    auto* cartLabel = new QLabel("🛒 Cart");
    cartLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    cartLayout->addWidget(cartLabel);
    
    m_cartList = new QListWidget;
    m_cartList->setStyleSheet(QString("QListWidget { background: %1; border: 1px solid %2; border-radius: 8px; padding: 8px; font-size: 13px; }").arg(tokens.cardBg, tokens.border));
    m_cartList->installEventFilter(this); // Install event filter for keyboard controls
    cartLayout->addWidget(m_cartList, 1);
    
    // Delivery order toggle
    auto* deliveryFrame = new QFrame;
    deliveryFrame->setStyleSheet(QString("QFrame { background: transparent; border: none; padding: 0px; }"));
    auto* deliveryLayout = new QHBoxLayout(deliveryFrame);
    deliveryLayout->setSpacing(8);
    deliveryLayout->setContentsMargins(0, 0, 0, 0);
    
    m_deliveryToggleBtn = new QPushButton("Delivery Order");
    m_deliveryToggleBtn->setCheckable(true);
    m_deliveryToggleBtn->setMinimumHeight(50);
    m_deliveryToggleBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delivery-svgrepo-com.svg"), QColor(tokens.textPrimary), 24));
    m_deliveryToggleBtn->setIconSize(QSize(24, 24));
    m_deliveryToggleBtn->setStyleSheet(QString(
        "QPushButton { "
        "  font-size: 16px; font-weight: bold; "
        "  background: %1; color: %2; "
        "  border-radius: 8px; border: 2px solid %3; "
        "  padding-left: 15px; "
        "  text-align: left; "
        "}"
        "QPushButton:hover { "
        "  background: #E5E7EB; "
        "}"
        "QPushButton:checked { "
        "  background: %4; "
        "  color: white; "
        "  border-color: %5; "
        "  box-shadow: 0 0 15px rgba(59, 130, 246, 0.4); "
        "}"
    ).arg(tokens.cardBg, tokens.textPrimary, tokens.border, 
          tokens.primary, tokens.primary));
    
    m_deliveryFeeLabel = new QLabel("Fee:");
    m_deliveryFeeLabel->setVisible(false);
    m_deliveryFeeLabel->setStyleSheet("font-weight: bold;");
    
    m_deliveryFeeSpin = new QDoubleSpinBox;
    m_deliveryFeeSpin->setRange(0.0, 9999.99);
    m_deliveryFeeSpin->setDecimals(2);
    m_deliveryFeeSpin->setPrefix(ConfigManager::instance().currencySymbol() + " ");
    m_deliveryFeeSpin->setValue(0.0);
    m_deliveryFeeSpin->setMinimumHeight(38);
    m_deliveryFeeSpin->setMaximumWidth(150);
    m_deliveryFeeSpin->setVisible(false);
    m_deliveryFeeSpin->setStyleSheet(
        "QDoubleSpinBox {"
        "  padding: 6px;"
        "  border: 1px solid #D1D5DB;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  width: 16px;"
        "  border: none;"
        "}"
        "QDoubleSpinBox::up-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-bottom: 5px solid #6B7280;"
        "  width: 0px;"
        "  height: 0px;"
        "}"
        "QDoubleSpinBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 5px solid #6B7280;"
        "  width: 0px;"
        "  height: 0px;"
        "}"
    );
    
    deliveryLayout->addWidget(m_deliveryToggleBtn, 2);
    deliveryLayout->addWidget(m_deliveryFeeLabel);
    deliveryLayout->addWidget(m_deliveryFeeSpin, 1);
    
    cartLayout->addWidget(deliveryFrame);
    
    // Totals
    auto* totalsFrame = new QFrame;
    totalsFrame->setStyleSheet(QString("QFrame { background: %1; border-radius: 8px; padding: 10px 12px; }").arg(tokens.cardBg));
    auto* totalsLayout = new QVBoxLayout(totalsFrame);
    totalsLayout->setSpacing(4);
    
    m_cartSubtotalLbl = new QLabel("Subtotal: 0.00");
    m_cartSubtotalLbl->setStyleSheet("font-size: 13px;");
    m_cartTaxLbl = new QLabel("Tax (0%): 0.00");
    m_cartTaxLbl->setStyleSheet("font-size: 13px;");
    m_cartTotalLbl = new QLabel("TOTAL: 0.00");
    m_cartTotalLbl->setStyleSheet("font-weight: bold; font-size: 18px; color: #059669;");
    
    totalsLayout->addWidget(m_cartSubtotalLbl);
    totalsLayout->addWidget(m_cartTaxLbl);
    totalsLayout->addWidget(new QLabel("————————————"));
    totalsLayout->addWidget(m_cartTotalLbl);
    cartLayout->addWidget(totalsFrame);
    
    // Buttons
    m_checkoutBtn = new QPushButton("CHECKOUT");
    m_checkoutBtn->setObjectName("primaryBtn");
    m_checkoutBtn->setMinimumHeight(60);
    m_checkoutBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("cash-svgrepo-com.svg"), QColor("#FFFFFF"), 24));
    m_checkoutBtn->setIconSize(QSize(24, 24));
    m_checkoutBtn->setStyleSheet("QPushButton { font-size: 18px; font-weight: bold; }");
    m_checkoutBtn->setEnabled(false);
    
    m_clearBtn = new QPushButton("Clear Cart");
    m_clearBtn->setObjectName("dangerBtn");
    m_clearBtn->setMinimumHeight(45);
    m_clearBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), QColor("#FFFFFF"), 20));
    m_clearBtn->setIconSize(QSize(20, 20));
    
    cartLayout->addWidget(m_checkoutBtn);
    cartLayout->addWidget(m_clearBtn);
    
    m_rightStack->addWidget(m_cartView);
    
    // ─── View 1: Payment View ────────────────────────────────────────────────
    m_paymentView = new QWidget;
    auto* payLayout = new QVBoxLayout(m_paymentView);
    payLayout->setSpacing(16);
    payLayout->setContentsMargins(20, 20, 20, 20);
    
    auto* payTitle = new QLabel("💳 Payment");
    payTitle->setStyleSheet("font-weight: bold; font-size: 20px;");
    payLayout->addWidget(payTitle);
    
    // Total to pay
    m_paymentTotalLbl = new QLabel("Total: 0.00");
    m_paymentTotalLbl->setStyleSheet("font-size: 24px; font-weight: bold; color: #059669;");
    payLayout->addWidget(m_paymentTotalLbl);
    
    // Split payment summary frame
    auto* splitFrame = new QFrame;
    splitFrame->setStyleSheet(QString("QFrame { background: %1; border: 2px solid %2; border-radius: 8px; padding: 12px; }").arg(tokens.cardBg, tokens.border));
    auto* splitLayout = new QVBoxLayout(splitFrame);
    splitLayout->setSpacing(6);
    
    m_cashAmountLbl = new QLabel("💵 Cash: 0.00");
    m_cashAmountLbl->setStyleSheet("font-size: 14px;");
    m_cardAmountLbl = new QLabel("💳 Card: 0.00");
    m_cardAmountLbl->setStyleSheet("font-size: 14px;");
    m_remainingLbl = new QLabel("Remaining: 0.00");
    m_remainingLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #EF4444;");
    
    splitLayout->addWidget(m_cashAmountLbl);
    splitLayout->addWidget(m_cardAmountLbl);
    splitLayout->addWidget(new QLabel("—————————————"));
    splitLayout->addWidget(m_remainingLbl);
    
    payLayout->addWidget(splitFrame);
    
    // Numeric keypad
    m_numericKeypad = new NumericKeypad;
    payLayout->addWidget(m_numericKeypad);
    
    // Payment method buttons with amount display
    m_cashBtn = new QPushButton("Cash");
    m_cashBtn->setMinimumHeight(70);
    m_cashBtn->setCheckable(true);
    m_cashBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("cash-svgrepo-com.svg"), QColor("#FFFFFF"), 32));
    m_cashBtn->setIconSize(QSize(32, 32));
    m_cashBtn->setStyleSheet(
        "QPushButton { "
        "  font-size: 18px; font-weight: bold; "
        "  background: #10B981; color: white; "
        "  border-radius: 8px; border: 3px solid transparent; "
        "  padding-left: 20px; "
        "  text-align: left; "
        "}"
        "QPushButton:hover { background: #059669; }"
        "QPushButton:checked { "
        "  background: #047857; "
        "  border-color: #34D399; "
        "  box-shadow: 0 0 15px rgba(16, 185, 129, 0.5); "
        "}"
    );
    
    m_cardBtn = new QPushButton("Card");
    m_cardBtn->setMinimumHeight(70);
    m_cardBtn->setCheckable(true);
    m_cardBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("credit-card-svgrepo-com.svg"), QColor("#FFFFFF"), 32));
    m_cardBtn->setIconSize(QSize(32, 32));
    m_cardBtn->setStyleSheet(
        "QPushButton { "
        "  font-size: 18px; font-weight: bold; "
        "  background: #3B82F6; color: white; "
        "  border-radius: 8px; border: 3px solid transparent; "
        "  padding-left: 20px; "
        "  text-align: left; "
        "}"
        "QPushButton:hover { background: #2563EB; }"
        "QPushButton:checked { "
        "  background: #1D4ED8; "
        "  border-color: #60A5FA; "
        "  box-shadow: 0 0 15px rgba(59, 130, 246, 0.5); "
        "}"
    );
    
    payLayout->addWidget(m_cashBtn);
    payLayout->addWidget(m_cardBtn);
    
    payLayout->addSpacing(10);
    
    // Validate button (confirm and print)
    m_validateBtn = new QPushButton("VALIDATE");
    m_validateBtn->setObjectName("primaryBtn");
    m_validateBtn->setMinimumHeight(65);
    m_validateBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("validate-svgrepo-com.svg"), QColor("#FFFFFF"), 28));
    m_validateBtn->setIconSize(QSize(28, 28));
    m_validateBtn->setStyleSheet("QPushButton { font-size: 18px; font-weight: bold; padding-left: 20px; }");
    m_validateBtn->setEnabled(false);
    payLayout->addWidget(m_validateBtn);
    
    payLayout->addStretch();
    
    // Back button
    m_backToCartBtn = new QPushButton("← Back to Cart");
    m_backToCartBtn->setObjectName("secondaryBtn");
    m_backToCartBtn->setMinimumHeight(45);
    payLayout->addWidget(m_backToCartBtn);
    
    m_rightStack->addWidget(m_paymentView);
    
    // ─── View 2: Success View ────────────────────────────────────────────────
    m_successView = new QWidget;
    auto* successLayout = new QVBoxLayout(m_successView);
    successLayout->setSpacing(20);
    successLayout->setContentsMargins(20, 20, 20, 20);
    
    successLayout->addStretch();
    
    auto* checkIcon = new QLabel("✅");
    checkIcon->setStyleSheet("font-size: 80px;");
    checkIcon->setAlignment(Qt::AlignCenter);
    successLayout->addWidget(checkIcon);
    
    auto* successTitle = new QLabel("Order Completed!");
    successTitle->setStyleSheet("font-size: 24px; font-weight: bold; color: #059669;");
    successTitle->setAlignment(Qt::AlignCenter);
    successLayout->addWidget(successTitle);
    
    m_successMessageLbl = new QLabel("Order #0");
    m_successMessageLbl->setStyleSheet("font-size: 16px;");
    m_successMessageLbl->setAlignment(Qt::AlignCenter);
    successLayout->addWidget(m_successMessageLbl);
    
    successLayout->addStretch();
    
    m_printReceiptBtn = new QPushButton();
    QIcon printIcon = SvgIconHelper::icon(sidebarSvgPath("print-svgrepo-com.svg"), 
                                          QColor("#FFFFFF"), 24);
    m_printReceiptBtn->setIcon(printIcon);
    m_printReceiptBtn->setIconSize(QSize(24, 24));
    m_printReceiptBtn->setText(" " + LangManager::instance().t("طباعة الفاتورة"));
    m_printReceiptBtn->setObjectName("primaryBtn");
    m_printReceiptBtn->setMinimumHeight(60);
    m_printReceiptBtn->setStyleSheet("QPushButton { font-size: 16px; font-weight: bold; }");
    successLayout->addWidget(m_printReceiptBtn);
    
    m_newOrderBtn = new QPushButton("➕ New Order");
    m_newOrderBtn->setObjectName("secondaryBtn");
    m_newOrderBtn->setMinimumHeight(50);
    successLayout->addWidget(m_newOrderBtn);
    
    m_rightStack->addWidget(m_successView);
    
    root->addWidget(m_rightStack, 2);
    
    // === Toolbar ===
    auto* toolbar = new QToolBar;
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QString("QToolBar { background: %1; padding: 8px; spacing: 10px; }").arg(tokens.sidebarBg));
    
    // Session selector
    auto* sessionLbl = new QLabel;
    QString sessionIconPath = sidebarSvgPath("session-manager-svgrepo-com.svg");
    if (!sessionIconPath.isEmpty()) {
        QIcon sessionIcon = SvgIconHelper::icon(sessionIconPath, QColor("#FFFFFF"), 20);
        QPixmap sessionPx = sessionIcon.pixmap(20, 20);
        sessionLbl->setPixmap(sessionPx);
    }
    sessionLbl->setStyleSheet("padding: 4px; background: transparent;");
    
    auto* sessionTextLbl = new QLabel(" Session:");
    sessionTextLbl->setStyleSheet("color: white; font-weight: bold; padding: 4px; background: transparent;");
    
    m_sessionCombo = new QComboBox;
    m_sessionCombo->setMinimumWidth(200);
    m_sessionCombo->setStyleSheet("QComboBox { background: white; padding: 6px; border-radius: 4px; }");
    
    m_sessionLbl = new QLabel("No session");
    m_sessionLbl->setStyleSheet("color: white; font-weight: bold; padding: 4px; margin-left: 8px; background: transparent;");
    
    auto* refreshBtn = new QPushButton("Refresh");
    refreshBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("refresh-svgrepo-com.svg"), QColor("#FFFFFF"), 18));
    refreshBtn->setIconSize(QSize(18, 18));
    refreshBtn->setStyleSheet(QString("QPushButton { background: %1; color: white; padding: 8px 16px; border-radius: 6px; }").arg(tokens.primary));
    
    m_logoutBtn = new QPushButton("Logout");
    m_logoutBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("logout-bracket-svgrepo-com.svg"), QColor("#FFFFFF"), 18));
    m_logoutBtn->setIconSize(QSize(18, 18));
    m_logoutBtn->setStyleSheet("QPushButton { background: #DC2626; color: white; padding: 8px 16px; border-radius: 6px; font-weight: bold; }");
    
    toolbar->addWidget(sessionLbl);
    toolbar->addWidget(sessionTextLbl);
    toolbar->addWidget(m_sessionCombo);
    toolbar->addWidget(m_sessionLbl);
    toolbar->addWidget(refreshBtn);
    
    // Spacer to push logout to far right
    auto* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setStyleSheet("background: transparent;");
    toolbar->addWidget(spacer);
    
    toolbar->addWidget(m_logoutBtn);
    
    addToolBar(toolbar);
    
    statusBar()->showMessage("Ready");
    
    // Load sessions to combo
    loadSessionsToCombo();
    
    // === Connections ===
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PosWindow::refreshProducts);
    connect(m_selectCustomerBtn, &QPushButton::clicked, this, &PosWindow::onCustomerSelect);
    connect(m_checkoutBtn, &QPushButton::clicked, this, &PosWindow::onCheckout);
    connect(m_clearBtn, &QPushButton::clicked, this, &PosWindow::onClearCart);
    connect(refreshBtn, &QPushButton::clicked, this, &PosWindow::refreshProducts);
    connect(m_logoutBtn, &QPushButton::clicked, this, &PosWindow::onLogout);
    connect(m_sessionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PosWindow::onSessionChanged);
    
    // Delivery order toggle button
    connect(m_deliveryToggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_deliveryFeeLabel->setVisible(checked);
        m_deliveryFeeSpin->setVisible(checked);
        if (checked) {
            m_deliveryFeeSpin->setFocus();
        }
        updateCartDisplay();
    });
    
    // Delivery fee changed
    connect(m_deliveryFeeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        updateCartDisplay();
    });
    
    connect(m_cashBtn, &QPushButton::clicked, this, &PosWindow::onCashPaymentClicked);
    connect(m_cardBtn, &QPushButton::clicked, this, &PosWindow::onCardPaymentClicked);
    connect(m_validateBtn, &QPushButton::clicked, this, &PosWindow::onValidateAndPrint);
    connect(m_numericKeypad, &NumericKeypad::valueChanged, this, &PosWindow::updatePaymentDisplay);
    connect(m_backToCartBtn, &QPushButton::clicked, this, &PosWindow::showCartView);
    
    connect(m_printReceiptBtn, &QPushButton::clicked, this, &PosWindow::onPrintReceipt);
    connect(m_newOrderBtn, &QPushButton::clicked, this, &PosWindow::onNewOrder);
}

void PosWindow::loadProducts() {
    m_products = m_productRepo->getAll();
    refreshProducts();
}

void PosWindow::refreshProducts() {
    // Show loading status
    statusBar()->showMessage("Loading products...");
    
    // Clear existing grid
    QLayoutItem* item;
    while ((item = m_productGrid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    QString filter = m_searchEdit->text().trimmed().toLower();
    const QString sym = ConfigManager::instance().currencySymbol();
    
    int row = 0, col = 0;
    const int columns = 3;
    int displayedCount = 0;
    const int maxDisplay = 100; // Limit to prevent UI freeze
    
    // Fetch active promotions ONCE per refresh instead of once per product card -
    // this used to run getActive() (plus a getProductsForPromotion() per promotion)
    // inside the loop below, for every single product, on every refresh.
    QList<Promotion> activePromotions = m_promotionRepo->getActive();
    
    for (const auto& product : m_products) {
        if (product.status() != Product::Status::Active) continue;
        
        if (!filter.isEmpty() && !product.name().toLower().contains(filter) &&
            !product.barcode().toLower().contains(filter)) {
            continue;
        }
        
        if (displayedCount >= maxDisplay) {
            auto* moreLbl = new QLabel(QString("+ %1 more products (use search to filter)").arg(m_products.size() - maxDisplay));
            moreLbl->setStyleSheet("color: #6B7280; font-style: italic; padding: 20px;");
            moreLbl->setAlignment(Qt::AlignCenter);
            m_productGrid->addWidget(moreLbl, row, 0, 1, columns);
            break;
        }
        
        // Create product card widget
        auto* card = new QWidget;
        card->setMinimumHeight(140);
        card->setMaximumWidth(250);
        
        // Same lookup used by onProductClicked() when the item is actually sold,
        // so the price shown on the card and the price charged can never diverge.
        double promotionPrice = getActivePromotionPrice(product, activePromotions);
        bool hasPromotion = promotionPrice > 0.0;
        
        card->setStyleSheet(
            "QWidget {"
            "  background: white;"
            "  border: 2px solid #E5E7EB;"
            "  border-radius: 8px;"
            "}"
            "QWidget:hover {"
            "  background: #F3F4F6;"
            "  border-color: #3B82F6;"
            "}"
        );
        card->setCursor(Qt::PointingHandCursor);
        
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(4);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        
        // Promotion ribbon (top-right corner) - must be positioned AFTER card is laid out
        QLabel* ribbon = nullptr;
        if (hasPromotion) {
            ribbon = new QLabel("🏷️ OFFER", card);
            ribbon->setStyleSheet(
                "QLabel {"
                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #DC2626, stop:1 #EF4444);"
                "  color: white;"
                "  font-weight: bold;"
                "  font-size: 9px;"
                "  padding: 3px 8px;"
                "  border-radius: 0px 6px 0px 6px;"
                "}"
            );
            ribbon->setAlignment(Qt::AlignCenter);
            ribbon->setFixedSize(60, 20);
            // Position will be set after card layout
        }
        
        // Product image (top)
        auto* imageLabel = new QLabel;
        imageLabel->setFixedSize(100, 100);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(false);
        
        if (!product.imagePath().isEmpty() && QFile::exists(product.imagePath())) {
            QPixmap img(product.imagePath());
            if (!img.isNull()) {
                imageLabel->setPixmap(img.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                imageLabel->setText("📦");
                imageLabel->setStyleSheet("font-size: 48px;");
            }
        } else {
            // No image - show emoji
            imageLabel->setText("📦");
            imageLabel->setStyleSheet("font-size: 48px;");
        }
        cardLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
        
        // Product name (below image)
        auto* nameLabel = new QLabel(product.name());
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #1F2937;");
        nameLabel->setMaximumHeight(36);
        cardLayout->addWidget(nameLabel);
        
        // Price (with tax if applicable) - use promotion price if available
        double taxRate = ConfigManager::instance().taxRate();
        double basePrice = hasPromotion ? promotionPrice : product.price();
        
        // Only add tax if product has tax flag enabled
        double displayPrice = basePrice;
        if (product.hasTax()) {
            displayPrice = basePrice * (1.0 + taxRate / 100.0);
        }
        
        QString priceText;
        if (hasPromotion) {
            // Show old price crossed out + new price
            double oldDisplayPrice = product.hasTax() ? product.priceWithTax(taxRate) : product.price();
            priceText = QString("<span style='text-decoration: line-through; color: #9CA3AF; font-size: 11px;'>%1</span> <span style='color: #DC2626; font-weight: bold;'>%2 %3</span>")
                .arg(oldDisplayPrice, 0, 'f', 2)
                .arg(displayPrice, 0, 'f', 2)
                .arg(sym);
        } else {
            priceText = QString("%1 %2").arg(displayPrice, 0, 'f', 2).arg(sym);
        }
        
        auto* priceLabel = new QLabel(priceText);
        priceLabel->setAlignment(Qt::AlignCenter);
        priceLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #059669;");
        priceLabel->setTextFormat(Qt::RichText);
        cardLayout->addWidget(priceLabel);
        
        // Stock info
        auto* stockLabel = new QLabel(QString("Stock: %1 %2").arg(product.stockQty()).arg(product.unitLabel()));
        stockLabel->setAlignment(Qt::AlignCenter);
        stockLabel->setStyleSheet("font-size: 10px; color: #6B7280;");
        cardLayout->addWidget(stockLabel);
        
        // Make entire card clickable
        card->setProperty("productId", product.id());
        card->setProperty("hasPromotion", hasPromotion);
        card->setProperty("promotionPrice", promotionPrice);
        card->installEventFilter(this);
        
        m_productGrid->addWidget(card, row, col);
        
        // Position promotion ribbon after card is added to layout
        if (ribbon) {
            card->updateGeometry();
            // Use QPointer to safely handle widget deletion during rapid search
            QPointer<QWidget> cardPtr(card);
            QPointer<QLabel> ribbonPtr(ribbon);
            QTimer::singleShot(0, [cardPtr, ribbonPtr]() {
                if (cardPtr && ribbonPtr) {
                    ribbonPtr->move(cardPtr->width() - 60, 0);
                    ribbonPtr->raise();
                }
            });
        }
        
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
        displayedCount++;
    }
    
    // Add spacer at the end
    m_productGrid->setRowStretch(row + 1, 1);
    
    statusBar()->showMessage(QString("Ready - %1 product(s) displayed").arg(displayedCount), 3000);
}

double PosWindow::getActivePromotionPrice(const Product& product, const QList<Promotion>& activePromotions) const {
    double bestPrice = 0.0;
    bool found = false;
    QDateTime now = QDateTime::currentDateTime();

    for (const Promotion& promo : activePromotions) {
        if (promo.status != Promotion::Status::Active) continue;
        if (now < promo.startDate || now > promo.endDate) continue;

        QList<int> productIds = m_promotionRepo->getProductsForPromotion(promo.id);
        if (!productIds.contains(product.id())) continue;

        double magPrice = m_promotionRepo->getProductMagazinePrice(promo.id, product.id());
        if (magPrice <= 0) continue;

        if (!found || magPrice < bestPrice) {
            bestPrice = magPrice;
            found = true;
        }
    }

    return found ? bestPrice : 0.0;
}

void PosWindow::onProductClicked(const Product& product) {
    if (product.stockQty() <= 0) {
        statusBar()->showMessage(QString("⚠ Product \"%1\" is out of stock").arg(product.name()), 3000);
        return;
    }
    
    // Check if already in cart
    for (auto& item : m_cart) {
        if (item.productId == product.id()) {
            if (item.quantity >= product.stockQty()) {
                statusBar()->showMessage(QString("⚠ Cannot add more. Available: %1").arg(product.stockQty()), 3000);
                return;
            }
            item.quantity++;
            updateCartDisplay();
            return;
        }
    }
    
    // IMPORTANT: this used to always use product.price(), completely ignoring any
    // active magazine/promotion - the card could show a discounted price while the
    // cart silently charged full price. Now it applies the exact same promotion
    // lookup used to render the card, so what's shown is what's actually sold.
    QList<Promotion> activePromotions = m_promotionRepo->getActive();
    double promoPrice = getActivePromotionPrice(product, activePromotions);

    // Add new item (store base price, tax calculated later)
    CartItem item;
    item.productId = product.id();
    item.name = product.name();
    item.unitPrice = promoPrice > 0.0 ? promoPrice : product.price(); // Use magazine price if one applies
    item.hasTax = product.hasTax();   // Store tax flag
    item.quantity = 1;
    m_cart.append(item);
    
    updateCartDisplay();
}

void PosWindow::updateCartDisplay() {
    m_cartList->clear();
    
    const QString sym = ConfigManager::instance().currencySymbol();
    double taxRate = ConfigManager::instance().taxRate();
    
    for (int i = 0; i < m_cart.size(); i++) {
        const auto& item = m_cart[i];
        
        // Calculate display price (with tax if applicable)
        double itemTotal = item.total();
        if (item.hasTax && taxRate > 0.0) {
            itemTotal *= (1.0 + taxRate / 100.0);
        }
        
        QString text = QString("%1 x %2\n%3 %4")
                           .arg(item.quantity)
                           .arg(item.name)
                           .arg(itemTotal, 0, 'f', 2)
                           .arg(sym);
        
        auto* listItem = new QListWidgetItem(text);
        listItem->setData(Qt::UserRole, i);
        m_cartList->addItem(listItem);
    }
    
    updateTotals();
}

CartTotals PosWindow::calculateCartTotals() const {
    CartTotals t;
    double taxRate = ConfigManager::instance().taxRate();

    for (const auto& item : m_cart) {
        double itemSubtotal = item.total();
        t.subtotal += itemSubtotal;

        // Calculate tax for items with tax enabled
        if (item.hasTax && taxRate > 0.0) {
            t.tax += itemSubtotal * (taxRate / 100.0);
        }
    }

    // Add delivery fee if enabled
    if (m_deliveryToggleBtn && m_deliveryToggleBtn->isChecked()) {
        t.deliveryFee = m_deliveryFeeSpin->value();
    }

    return t;
}

void PosWindow::updateTotals() {
    CartTotals t = calculateCartTotals();
    double taxRate = ConfigManager::instance().taxRate();

    const QString sym = ConfigManager::instance().currencySymbol();
    m_cartSubtotalLbl->setText(QString("Subtotal: %1 %2").arg(t.subtotal, 0, 'f', 2).arg(sym));
    
    QString taxText = QString("Tax (%1%): %2 %3").arg(taxRate, 0, 'f', 2).arg(t.tax, 0, 'f', 2).arg(sym);
    if (t.deliveryFee > 0.0) {
        taxText += QString("\nDelivery Fee: %1 %2").arg(t.deliveryFee, 0, 'f', 2).arg(sym);
    }
    m_cartTaxLbl->setText(taxText);
    
    m_cartTotalLbl->setText(QString("TOTAL: %1 %2").arg(t.total(), 0, 'f', 2).arg(sym));
    m_paymentTotalLbl->setText(QString("Total: %1 %2").arg(t.total(), 0, 'f', 2).arg(sym));
    
    m_checkoutBtn->setEnabled(!m_cart.isEmpty());
}

void PosWindow::onCustomerSelect() {
    // Create customer selection dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Select Customer");
    dialog.setMinimumSize(700, 500);
    
    auto* layout = new QVBoxLayout(&dialog);
    
    // Search box
    auto* searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Search by name, phone, or address...");
    searchEdit->setMinimumHeight(40);
    layout->addWidget(searchEdit);
    
    // Customer table
    auto* table = new QTableWidget;
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"ID", "Name", "Phone", "Address"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    layout->addWidget(table);
    
    // Load customers function
    auto loadCustomers = [&](const QString& searchTerm = QString()) {
        table->setRowCount(0);
        
        QList<Customer> customers;
        if (searchTerm.isEmpty()) {
            customers = m_customerRepo->getAll();
        } else {
            // Search in name, phone, address
            for (const auto& customer : m_customerRepo->getAll()) {
                bool matchName = customer.name().contains(searchTerm, Qt::CaseInsensitive);
                bool matchPhone = false;
                bool matchAddress = false;
                
                for (const auto& phone : customer.phones()) {
                    if (phone.number.contains(searchTerm, Qt::CaseInsensitive)) {
                        matchPhone = true;
                        break;
                    }
                }
                
                for (const auto& addr : customer.addresses()) {
                    if (addr.text.contains(searchTerm, Qt::CaseInsensitive)) {
                        matchAddress = true;
                        break;
                    }
                }
                
                if (matchName || matchPhone || matchAddress) {
                    customers.append(customer);
                }
            }
        }
        
        for (const auto& customer : customers) {
            int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(QString::number(customer.id())));
            table->setItem(row, 1, new QTableWidgetItem(customer.name()));
            
            // Get first phone
            QString phoneStr = customer.phones().isEmpty() ? "" : customer.phones().first().number;
            table->setItem(row, 2, new QTableWidgetItem(phoneStr));
            
            // Get first address
            QString addrStr = customer.addresses().isEmpty() ? "" : customer.addresses().first().text;
            table->setItem(row, 3, new QTableWidgetItem(addrStr));
        }
    };
    
    // Connect search
    connect(searchEdit, &QLineEdit::textChanged, [&](const QString& text) {
        loadCustomers(text);
    });
    
    // Load initial customers
    loadCustomers();
    
    // Buttons
    auto* btnLayout = new QHBoxLayout;
    auto* addNewBtn = new QPushButton("➕ Add New Customer");
    auto* selectBtn = new QPushButton("✓ Select");
    auto* cancelBtn = new QPushButton("✗ Cancel");
    
    selectBtn->setObjectName("primaryBtn");
    addNewBtn->setObjectName("secondaryBtn");
    
    btnLayout->addWidget(addNewBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(selectBtn);
    layout->addLayout(btnLayout);
    
    // Add new customer using full CustomerDialog
    connect(addNewBtn, &QPushButton::clicked, [&]() {
        // Get regions and products for the dialog
        QList<Region> regions = m_regionRepo->getAll();
        QList<Product> products = m_productRepo->getAll();
        
        CustomerDialog customerDialog(regions, products, &dialog);
        
        if (customerDialog.exec() == QDialog::Accepted) {
            Customer savedCustomer = customerDialog.getCustomer();
            if (m_customerRepo->save(savedCustomer)) {
                QMessageBox::information(&dialog, "Success", "Customer added successfully!");
                loadCustomers(searchEdit->text()); // Refresh list
            } else {
                QMessageBox::critical(&dialog, "Error", "Failed to save customer!");
            }
        }
    });
    
    // Select customer
    connect(selectBtn, &QPushButton::clicked, [&]() {
        if (table->currentRow() < 0) {
            QMessageBox::warning(&dialog, "Warning", "Please select a customer!");
            return;
        }
        
        int customerId = table->item(table->currentRow(), 0)->text().toInt();
        auto customer = m_customerRepo->getById(customerId);
        if (customer.id() > 0) {
            m_selectedCustomer = customer;
            m_customerLbl->setText(customer.name());
            statusBar()->showMessage(QString("Customer selected: %1").arg(customer.name()), 2000);
            dialog.accept();
        }
    });
    
    // Double-click to select
    connect(table, &QTableWidget::cellDoubleClicked, [&]() {
        selectBtn->click();
    });
    
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    dialog.exec();
}

void PosWindow::onCheckout() {
    if (m_cart.isEmpty()) return;
    
    // This used to be "m_totalAmount = subtotal;" with a "// + tax if applicable"
    // comment that was never actually implemented - so the payment screen asked
    // for (and VALIDATE accepted) only the pre-tax, pre-delivery-fee subtotal,
    // while the cart screen correctly showed subtotal + tax + delivery. Now both
    // read from the exact same calculateCartTotals() used by updateTotals().
    m_totalAmount = calculateCartTotals().total();
    
    // Reset payment state
    m_cashAmount = 0.0;
    m_cardAmount = 0.0;
    m_numericKeypad->clear();
    m_cashBtn->setChecked(false);
    m_cardBtn->setChecked(false);
    
    showPaymentView();
}

void PosWindow::showPaymentView() {
    const QString sym = ConfigManager::instance().currencySymbol();
    m_paymentTotalLbl->setText(QString("Total: %1 %2").arg(m_totalAmount, 0, 'f', 2).arg(sym));
    
    updatePaymentDisplay();
    m_rightStack->setCurrentWidget(m_paymentView);
    m_numericKeypad->setFocus();
}

void PosWindow::updatePaymentDisplay() {
    const QString sym = ConfigManager::instance().currencySymbol();
    
    // Update split amounts
    m_cashAmountLbl->setText(QString("💵 Cash: %1 %2").arg(m_cashAmount, 0, 'f', 2).arg(sym));
    m_cardAmountLbl->setText(QString("💳 Card: %1 %2").arg(m_cardAmount, 0, 'f', 2).arg(sym));
    
    double paid = m_cashAmount + m_cardAmount;
    double remaining = m_totalAmount - paid;
    
    if (remaining > 0.01) {
        m_remainingLbl->setText(QString("Remaining: %1 %2").arg(remaining, 0, 'f', 2).arg(sym));
        m_remainingLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #EF4444;");
        m_validateBtn->setEnabled(false);
    } else {
        double change = paid - m_totalAmount;
        if (change > 0.01) {
            m_remainingLbl->setText(QString("Change: %1 %2").arg(change, 0, 'f', 2).arg(sym));
            m_remainingLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #3B82F6;");
        } else {
            m_remainingLbl->setText(QString("✓ Paid in full"));
            m_remainingLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #10B981;");
        }
        m_validateBtn->setEnabled(true);
    }
}

void PosWindow::onCashPaymentClicked() {
    QString input = m_numericKeypad->value().trimmed();
    
    if (m_cashBtn->isChecked()) {
        // Button is now checked - apply amount
        if (input.isEmpty() || input == "0") {
            // No input: pay remaining with cash
            double remaining = m_totalAmount - m_cashAmount - m_cardAmount;
            m_cashAmount += remaining;
        } else {
            // Has input: use typed amount
            bool ok = false;
            double amount = input.toDouble(&ok);
            if (ok && amount > 0) {
                m_cashAmount += amount;
                m_numericKeypad->clear();
            }
        }
    } else {
        // Button unchecked - reset cash amount
        m_cashAmount = 0.0;
    }
    
    updatePaymentDisplay();
}

void PosWindow::onCardPaymentClicked() {
    QString input = m_numericKeypad->value().trimmed();
    
    if (m_cardBtn->isChecked()) {
        // Button is now checked - apply amount
        if (input.isEmpty() || input == "0") {
            // No input: pay remaining with card
            double remaining = m_totalAmount - m_cashAmount - m_cardAmount;
            m_cardAmount += remaining;
        } else {
            // Has input: use typed amount
            bool ok = false;
            double amount = input.toDouble(&ok);
            if (ok && amount > 0) {
                m_cardAmount += amount;
                m_numericKeypad->clear();
            }
        }
    } else {
        // Button unchecked - reset card amount
        m_cardAmount = 0.0;
    }
    
    updatePaymentDisplay();
}

void PosWindow::onValidateAndPrint() {
    double paid = m_cashAmount + m_cardAmount;
    if (paid < m_totalAmount - 0.01) {
        statusBar()->showMessage("⚠ Payment not complete!", 3000);
        return;
    }
    
    // Determine payment method string
    QString paymentMethod;
    if (m_cashAmount > 0 && m_cardAmount > 0) {
        paymentMethod = QString("Split (Cash: %1, Card: %2)")
            .arg(m_cashAmount, 0, 'f', 2)
            .arg(m_cardAmount, 0, 'f', 2);
    } else if (m_cashAmount > 0) {
        paymentMethod = "Cash";
    } else {
        paymentMethod = "Card";
    }
    
    if (!saveOrder(paymentMethod)) {
        QMessageBox::critical(this, "Error", "Failed to save order. Check database connection.");
        return;
    }
    
    showSuccessView();
}

bool PosWindow::saveOrder(const QString& paymentMethod) {
    // Check if delivery order
    bool isDeliveryOrder = m_deliveryToggleBtn && m_deliveryToggleBtn->isChecked();
    
    if (!isDeliveryOrder) {
        // POS order: requires active session
        if (m_session.id() == 0) {
            QMessageBox::critical(this, "Error", "No active register session!");
            return false;
        }
    } else {
        // Delivery order: requires customer
        if (m_selectedCustomer.id() == 0) {
            QMessageBox::critical(this, "Error", "Delivery orders require a customer! Please select a customer.");
            return false;
        }
    }
    
    // Create order
    Order order;
    order.setCustomerId(m_selectedCustomer.id() > 0 ? m_selectedCustomer.id() : 0);
    order.setCustomerName(m_selectedCustomer.id() > 0 ? m_selectedCustomer.name() : "Walk-in Customer");
    
    if (isDeliveryOrder) {
        // Delivery order: save to BOTH Orders page AND current session
        // This allows it to appear in Orders page for delivery tracking
        // AND be counted in the cashier's session sales
        order.setRegisterSessionId(m_session.id()); // Save with session ID so it counts in session
        order.setStatus("Pending"); // Delivery orders start as Pending
        order.setDeliveryFee(m_deliveryFeeSpin->value());
    } else {
        // POS order: save to session only
        order.setRegisterSessionId(m_session.id());
        order.setStatus("Delivered"); // POS orders are instantly delivered
        order.setDeliveryFee(0.0);
    }
    
    order.setPaymentMethod(paymentMethod);
    order.setDateTime(QDateTime::currentDateTime());
    order.setDiscountAmount(0.0);
    
    // Add items
    QList<OrderItem> orderItems;
    double taxRate = ConfigManager::instance().taxRate();
    
    for (const auto& cartItem : m_cart) {
        OrderItem item;
        item.productId = cartItem.productId;
        item.productName = cartItem.name;
        item.quantity = cartItem.quantity;
        
        // IMPORTANT: Store the FINAL price (with tax if applicable)
        // so Order::calculateTotals() gets the correct total
        double finalUnitPrice = cartItem.unitPrice;
        if (cartItem.hasTax && taxRate > 0) {
            finalUnitPrice = cartItem.unitPrice * (1.0 + taxRate / 100.0);
        }
        item.unitPrice = finalUnitPrice;
        
        orderItems.append(item);
    }
    order.setItems(orderItems);
    order.calculateTotals();
    
    // Generate unique invoice barcode (format: INV-{timestamp}-{random})
    QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());
    QString random = QString::number(QRandomGenerator::global()->bounded(1000, 9999));
    QString invoiceBarcode = QString("INV-%1-%2").arg(timestamp).arg(random);
    order.setInvoiceBarcode(invoiceBarcode);
    
    // Save order
    if (!m_orderRepo->save(order)) {
        Logger::instance().error("Failed to save POS order");
        return false;
    }
    
    // Create stock movements and update stock
    int currentUserId = SessionManager::instance().currentUser().id();
    QDateTime now = QDateTime::currentDateTime();
    
    for (const auto& item : orderItems) {
        Product product = m_productRepo->getById(item.productId);
        if (product.id() == 0) continue;
        
        // Create stock movement
        StockMovement movement;
        movement.productId = item.productId;
        movement.movementType = "Sale";
        movement.quantity = static_cast<int>(item.quantity);
        movement.dateTime = now;
        movement.referenceType = "Order";
        movement.referenceId = order.id();
        movement.notes = QString("POS Sale - Order #%1").arg(order.id());
        movement.userId = currentUserId;
        
        if (!m_stockMovementRepo->save(movement)) {
            Logger::instance().error(QString("Failed to create stock movement for product %1").arg(item.productId));
        }
        
        // Decrease stock
        int newStock = product.stockQty() - static_cast<int>(item.quantity);
        if (newStock < 0) newStock = 0;
        product.setStockQty(newStock);
        
        if (!m_productRepo->save(product)) {
            Logger::instance().error(QString("Failed to update stock for product %1").arg(product.id()));
        }
    }
    
    m_lastOrder = order;
    statusBar()->showMessage(QString("✅ Order #%1 completed").arg(order.id()), 5000);
    return true;
}

void PosWindow::onPrintReceipt() {
    if (m_lastOrder.id() == 0) return;
    
    // Show receipt preview dialog
    ReceiptPreviewDialog dlg(m_lastOrder, m_session, m_lastOrder.paymentMethod(), this);
    dlg.exec();
}

void PosWindow::onNewOrder() {
    m_cart.clear();
    m_selectedCustomer = Customer();
    m_customerLbl->setText("👤 Walk-in Customer");
    
    // Reset delivery order toggle
    if (m_deliveryToggleBtn) {
        m_deliveryToggleBtn->setChecked(false);
        m_deliveryFeeSpin->setValue(0.0);
    }
    
    updateCartDisplay();
    showCartView();
    loadProducts(); // Refresh products to show updated stock
}


void PosWindow::onClearCart() {
    if (m_cart.isEmpty()) return;
    
    m_cart.clear();
    m_selectedCustomer = Customer(); // Reset customer
    m_customerLbl->setText("Walk-in Customer");
    
    // Reset delivery order toggle
    if (m_deliveryToggleBtn) {
        m_deliveryToggleBtn->setChecked(false);
        m_deliveryFeeSpin->setValue(0.0);
    }
    
    updateCartDisplay();
    statusBar()->showMessage("🗑 Cart cleared", 2000);
}

void PosWindow::onLogout() {
    close();
}

void PosWindow::onQuantityChanged() {
    // TODO: Implement quantity editing
}

bool PosWindow::eventFilter(QObject* obj, QEvent* event) {
    // Handle cart list keyboard controls
    if (obj == m_cartList && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Get selected item
        QListWidgetItem* currentItem = m_cartList->currentItem();
        if (!currentItem) {
            return QMainWindow::eventFilter(obj, event);
        }
        
        int cartIndex = currentItem->data(Qt::UserRole).toInt();
        if (cartIndex < 0 || cartIndex >= m_cart.size()) {
            return QMainWindow::eventFilter(obj, event);
        }
        
        // Delete key: first press sets qty=0, second press removes item
        if (keyEvent->key() == Qt::Key_Delete) {
            m_quantityBuffer.clear(); // Clear buffer on delete
            
            if (m_cart[cartIndex].quantity > 0) {
                // First press: clear quantity
                m_cart[cartIndex].quantity = 0;
                updateCartDisplay();
                // Re-select same position
                if (cartIndex < m_cartList->count()) {
                    m_cartList->setCurrentRow(cartIndex);
                }
                statusBar()->showMessage("Quantity cleared. Press Delete again to remove item.", 2000);
            } else {
                // Second press: remove item - check authorization if enabled
                if (ConfigManager::instance().requireAuthForPOSDeletion()) {
                    if (!requestDeleteAuthorization()) {
                        statusBar()->showMessage("❌ Delete authorization denied", 2000);
                        return true;
                    }
                }
                
                m_cart.removeAt(cartIndex);
                updateCartDisplay();
                statusBar()->showMessage("Item removed from cart", 2000);
            }
            return true;
        }
        
        // Find product to get stock limit
        int productId = m_cart[cartIndex].productId;
        int maxStock = 9999;
        
        for (const auto& product : m_products) {
            if (product.id() == productId) {
                maxStock = product.stockQty();
                break;
            }
        }
        
        // Enter/Return: apply buffered quantity
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!m_quantityBuffer.isEmpty()) {
                int newQty = m_quantityBuffer.toInt();
                
                // Cap to available stock
                if (newQty > maxStock) {
                    newQty = maxStock;
                    statusBar()->showMessage(QString("⚠ Quantity capped to available stock: %1").arg(maxStock), 3000);
                }
                
                if (newQty == 0) {
                    m_cart.removeAt(cartIndex);
                    updateCartDisplay();
                    statusBar()->showMessage("Item removed from cart", 2000);
                } else {
                    m_cart[cartIndex].quantity = newQty;
                    updateCartDisplay();
                    // Re-select same position
                    if (cartIndex < m_cartList->count()) {
                        m_cartList->setCurrentRow(cartIndex);
                    }
                    statusBar()->showMessage(QString("Quantity set to %1").arg(newQty), 2000);
                }
                
                m_quantityBuffer.clear();
                return true;
            }
        }
        
        // Number keys 0-9: add to buffer
        if (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_9) {
            int digit = keyEvent->key() - Qt::Key_0;
            m_quantityBuffer += QString::number(digit);
            
            // Show current buffer in status bar
            statusBar()->showMessage(QString("Enter quantity: %1 (press Enter to confirm, Delete to cancel)").arg(m_quantityBuffer), 5000);
            return true;
        }
        
        // Escape: cancel buffer
        if (keyEvent->key() == Qt::Key_Escape) {
            if (!m_quantityBuffer.isEmpty()) {
                m_quantityBuffer.clear();
                statusBar()->showMessage("Quantity input cancelled", 2000);
                return true;
            }
        }
    }
    
    // Handle product card clicks
    if (obj->isWidgetType() && event->type() == QEvent::MouseButtonPress) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && widget->property("productId").isValid()) {
            int productId = widget->property("productId").toInt();
            bool hasPromotion = widget->property("hasPromotion").toBool();
            double promotionPrice = widget->property("promotionPrice").toDouble();
            
            // Find product
            for (const auto& product : m_products) {
                if (product.id() == productId) {
                    // Create modified product with promotion price if applicable
                    Product productToAdd = product;
                    
                    qDebug() << "🛒 Product clicked:" << product.name();
                    qDebug() << "   Original price:" << product.price();
                    qDebug() << "   Has promotion:" << hasPromotion;
                    qDebug() << "   Promotion price:" << promotionPrice;
                    
                    if (hasPromotion && promotionPrice > 0) {
                        productToAdd.setPrice(promotionPrice);
                        qDebug() << "   ✅ Applied promotion price:" << promotionPrice;
                    } else {
                        qDebug() << "   ❌ Using original price:" << product.price();
                    }
                    
                    onProductClicked(productToAdd);
                    return true;
                }
            }
        }
    }
    
    // Barcode scanner sends Enter after scanning
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            QString barcode = m_searchEdit->text().trimmed();
            
            if (!barcode.isEmpty()) {
                // Find product by barcode
                for (const auto& product : m_products) {
                    if (product.barcode() == barcode && product.status() == Product::Status::Active) {
                        // Auto-add to cart
                        onProductClicked(product);
                        m_searchEdit->clear();
                        return true; // Event handled
                    }
                }
                
                // Not found
                statusBar()->showMessage(QString("⚠ No product found with barcode: %1").arg(barcode), 3000);
                m_searchEdit->clear();
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}


void PosWindow::loadSessionsToCombo() {
    if (!m_sessionRepo) return;
    
    m_sessionCombo->blockSignals(true);
    m_sessionCombo->clear();
    
    // Get all open sessions
    QList<RegisterSession> sessions = m_sessionRepo->getAll();
    QList<RegisterSession> openSessions;
    
    for (const auto& session : sessions) {
        if (session.isOpen()) {
            openSessions.append(session);
        }
    }
    
    if (openSessions.isEmpty()) {
        m_sessionCombo->addItem("No open sessions", 0);
        m_sessionCombo->setEnabled(false);
        m_sessionLbl->setText("⚠ No session");
        m_sessionLbl->setStyleSheet("color: #FCA5A5; font-weight: bold; padding: 8px;");
    } else {
        m_sessionCombo->setEnabled(true);
        for (const auto& session : openSessions) {
            QString text = QString("Session #%1 - %2")
                .arg(session.id())
                .arg(session.openedAt().toString("dd/MM hh:mm"));
            m_sessionCombo->addItem(text, session.id());
            
            // Select current session if it matches
            if (session.id() == m_session.id()) {
                m_sessionCombo->setCurrentIndex(m_sessionCombo->count() - 1);
            }
        }
    }
    
    m_sessionCombo->blockSignals(false);
}

void PosWindow::onSessionChanged(int index) {
    if (index < 0) return;
    
    int sessionId = m_sessionCombo->currentData().toInt();
    if (sessionId == 0) return;
    
    // Same session, no change needed
    if (sessionId == m_session.id()) return;
    
    // Verify admin password
    if (!verifyAdminPassword()) {
        // Revert selection
        m_sessionCombo->blockSignals(true);
        for (int i = 0; i < m_sessionCombo->count(); ++i) {
            if (m_sessionCombo->itemData(i).toInt() == m_session.id()) {
                m_sessionCombo->setCurrentIndex(i);
                break;
            }
        }
        m_sessionCombo->blockSignals(false);
        statusBar()->showMessage("⚠ Admin password required to switch sessions", 3000);
        return;
    }
    
    // Load new session
    RegisterSession newSession = m_sessionRepo->getById(sessionId);
    if (newSession.id() == 0) {
        statusBar()->showMessage("⚠ Failed to load session", 3000);
        return;
    }
    
    setRegisterSession(newSession);
    
    // Clear cart when switching sessions
    m_cart.clear();
    updateCartDisplay();
    
    statusBar()->showMessage(QString("✅ Switched to Session #%1").arg(newSession.id()), 3000);
}

bool PosWindow::verifyAdminPassword() {
    bool ok;
    QString password = QInputDialog::getText(this, "Admin Authentication",
        "Enter admin password to switch sessions:",
        QLineEdit::Password, QString(), &ok);
    
    if (!ok || password.isEmpty()) {
        return false;
    }
    
    // Check if current user is admin
    User currentUser = SessionManager::instance().currentUser();
    
    // Simple check: roleId 1 is typically Admin/SuperAdmin
    // In production, verify against actual role table or use proper role check
    if (currentUser.roleId() != 1) {
        QMessageBox::warning(this, "Access Denied", "Only administrators can switch sessions.");
        return false;
    }
    
    // Password verification: In production, verify password hash
    // For now, we trust the session authentication
    // TODO: Add SessionManager::verifyCurrentUserPassword(password)
    
    return true;
}

void PosWindow::showSuccessView() {
    m_successMessageLbl->setText(QString("Order #%1\nTotal: %2 %3")
        .arg(m_lastOrder.id())
        .arg(m_lastOrder.grandTotal(), 0, 'f', 2)
        .arg(ConfigManager::instance().currencySymbol()));
    m_rightStack->setCurrentWidget(m_successView);
}

void PosWindow::showCartView() {
    m_rightStack->setCurrentWidget(m_cartView);
}

bool PosWindow::requestDeleteAuthorization() {
    QDialog authDialog(this);
    authDialog.setWindowTitle("Authorization Required");
    authDialog.setModal(true);
    authDialog.setMinimumWidth(420);

    auto* layout = new QVBoxLayout(&authDialog);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("<h3>🔒 Manager Authorization Required</h3>");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* messageLabel = new QLabel(
        "Scan the manager card, or type the code printed under the barcode:");
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignCenter);
    messageLabel->setObjectName("hintLabel");
    layout->addWidget(messageLabel);

    auto* barcodeEdit = new QLineEdit();
    barcodeEdit->setPlaceholderText("FP-0000000000-0000");
    // FIX: was QLineEdit::Password — typing blind made manual entry unusable
    // and typos impossible to spot. The card code is not a secret password.
    barcodeEdit->setEchoMode(QLineEdit::Normal);
    barcodeEdit->setClearButtonEnabled(true);
    barcodeEdit->setMinimumHeight(34);
    layout->addWidget(barcodeEdit);

    auto* statusLabel = new QLabel();
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);
    layout->addWidget(statusLabel);

    // FIX: there was no confirm button at all — the only way to submit was
    // QLineEdit::returnPressed, which a scanner triggers but a person typing
    // could easily miss (and Cancel was auto-default, so Enter could reject
    // the dialog instead).
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* okBtn = btnBox->button(QDialogButtonBox::Ok);
    okBtn->setText("Authorize");
    okBtn->setObjectName("primaryBtn");
    okBtn->setAutoDefault(false);
    okBtn->setDefault(false);
    btnBox->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
    btnBox->button(QDialogButtonBox::Cancel)->setDefault(false);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::rejected, &authDialog, &QDialog::reject);

    bool authorized = false;

    auto verify = [&]() {
        const QString entered = barcodeEdit->text();
        if (entered.trimmed().isEmpty()) {
            statusLabel->setText("<span style='color:#EF4444;'>Enter or scan a card code first</span>");
            statusLabel->setVisible(true);
            return;
        }

        Logger::instance().info(QString("[POS AUTH] Checking barcode: '%1' (normalized: '%2')")
            .arg(entered, BarcodeAuth::normalize(entered)));

        const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                               || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
        IUserRepository* userRepo = useSqlite
            ? static_cast<IUserRepository*>(new SQLiteUserRepository)
            : static_cast<IUserRepository*>(new AccessUserRepository);
        IRoleRepository* roleRepo = useSqlite
            ? static_cast<IRoleRepository*>(new SQLiteRoleRepository)
            : static_cast<IRoleRepository*>(new AccessRoleRepository);

        User foundUser;
        const QList<User> allUsers = userRepo->getAll();
        Logger::instance().info(QString("[POS AUTH] Total users in DB: %1").arg(allUsers.size()));

        for (const auto& user : allUsers) {
            // FIX: was an exact, case-sensitive `==` against the raw text.
            // Now normalized on both sides, so a typed code matches a scanned one.
            if (BarcodeAuth::matches(user.fingerprintBarcode(), entered)) {
                foundUser = user;
                Logger::instance().info(QString("[POS AUTH] Match found: %1").arg(user.name()));
                break;
            }
        }

        if (foundUser.id() == 0) {
            Logger::instance().warn(QString("[POS AUTH] No user found with barcode: '%1'").arg(entered));
            statusLabel->setText(
                BarcodeAuth::looksLikeCardCode(entered)
                    ? "<span style='color:#EF4444;'>❌ No user has this card code</span>"
                    : "<span style='color:#EF4444;'>❌ That does not look like a card code.<br/>"
                      "Type the full code printed under the barcode, e.g. FP-1789423193-7454</span>");
            statusLabel->setVisible(true);
            barcodeEdit->selectAll();
            barcodeEdit->setFocus();
            delete userRepo;
            delete roleRepo;
            return;
        }

        const Role userRole = roleRepo->getById(foundUser.roleId());
        Logger::instance().info(QString("[POS AUTH] User role: %1, canDeleteFromPOS: %2")
            .arg(userRole.name, userRole.canDeleteFromPOS ? "YES" : "NO"));

        if (!userRole.canDeleteFromPOS) {
            Logger::instance().warn(QString("[POS AUTH] Permission denied for: %1").arg(foundUser.name()));
            statusLabel->setText(QString("<span style='color:#EF4444;'>❌ %1 does not have permission</span>")
                                 .arg(foundUser.name()));
            statusLabel->setVisible(true);
            barcodeEdit->selectAll();
            barcodeEdit->setFocus();
            delete userRepo;
            delete roleRepo;
            return;
        }

        Logger::instance().info(QString("[POS AUTH] Authorized by: %1").arg(foundUser.name()));
        statusLabel->setText(QString("<span style='color:#10B981;'>✅ Authorized by %1</span>")
                             .arg(foundUser.name()));
        statusLabel->setVisible(true);
        authorized = true;

        delete userRepo;
        delete roleRepo;

        QTimer::singleShot(400, &authDialog, &QDialog::accept);
    };

    // Scanner path (sends Enter as a suffix) and manual path (button click)
    // both land on the same verification routine.
    connect(barcodeEdit, &QLineEdit::returnPressed, &authDialog, verify);
    connect(okBtn, &QPushButton::clicked, &authDialog, verify);

    // Any edit clears a stale error so the operator is not staring at an old
    // "invalid" message while retyping.
    connect(barcodeEdit, &QLineEdit::textEdited, &authDialog, [statusLabel](const QString&) {
        statusLabel->setVisible(false);
    });

    barcodeEdit->setFocus();
    authDialog.exec();

    return authorized;
}
