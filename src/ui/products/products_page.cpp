#include "products_page.h"
#include "product_dialog.h"
#include "product_quick_edit_dialog.h"
#include "missing_product_dialog.h"
#include "barcode_print_dialog.h"
#include "product_suppliers_dialog.h"
#include "magazine_dialog.h"
#include "services/archive_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "services/session_manager.h"
#include "services/audit_service.h"
#include "ui/tr_helper.h"
#include "ui/arabic_sort_proxy.h"
#include "ui/svg_icon_helper.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "core/product_supplier.h"
#include "core/promotion.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QFrame>
#include <QTimer>
#include <QDialog>
#include <QFormLayout>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileDialog>
#include <QCoreApplication>
#include <QFile>
#include "xlsxdocument.h"
#include "xlsxformat.h"

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

ProductsPage::ProductsPage(QWidget* parent)
    : QWidget(parent)
{
    if (DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE")
    {
        m_productRepo  = new SQLiteProductRepository;
        m_categoryRepo = new SQLiteCategoryRepository;
        m_missingRepo  = new SQLiteMissingProductRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_subUnitRepo  = new SQLiteProductSubUnitRepository;
        m_productSupplierRepo = new SQLiteProductSupplierRepository;
        m_supplierRepo = new SQLiteSupplierRepository;
        m_promotionRepo = new SQLitePromotionRepository;
    } else {
        m_productRepo  = new AccessProductRepository;
        m_categoryRepo = new AccessCategoryRepository;
        m_missingRepo  = new AccessMissingProductRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_subUnitRepo  = new AccessProductSubUnitRepository;
        m_productSupplierRepo = new AccessProductSupplierRepository;
        m_supplierRepo = new AccessSupplierRepository;
        m_promotionRepo = new AccessPromotionRepository;
    }

    setupUi();
    // Defer data loading until after window is shown
    // Data loaded by MainWindow::navigateTo — no singleShot needed
}

void ProductsPage::setupUi() {
    auto& L = LangManager::instance();
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto* tabs = new QTabWidget;

    // ── TAB 1: PRODUCTS DIRECTORY ─────────────────────────────────────────
    auto* prodTabWidget = new QWidget;
    auto* prodTabLayout = new QVBoxLayout(prodTabWidget);
    prodTabLayout->setSpacing(0);
    prodTabLayout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 10, 16, 10);
    toolbarLayout->setSpacing(10);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search products or categories...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMinimumWidth(240);
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);

    m_catFilterCombo = new QComboBox;
    m_catFilterCombo->setMinimumWidth(150);

    m_manageCatBtn = new QPushButton(L.t("Categories"));
    m_manageCatBtn->setObjectName("secondaryBtn");
    m_manageCatBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("category-svgrepo-com.svg"), 
                                                  QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_manageCatBtn->setIconSize(QSize(18, 18));

    m_addProdBtn = new QPushButton(L.t("＋ Add Product"));
    m_addProdBtn->setObjectName("primaryBtn");

    m_editProdBtn = new QPushButton(L.t("Edit"));
    m_editProdBtn->setObjectName("secondaryBtn");
    m_editProdBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                 QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editProdBtn->setIconSize(QSize(18, 18));
    m_editProdBtn->setEnabled(false);

    m_deleteProdBtn = new QPushButton(L.t("Delete"));
    m_deleteProdBtn->setObjectName("dangerBtn");
    m_deleteProdBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                   QColor("#FFFFFF"), 18));
    m_deleteProdBtn->setIconSize(QSize(18, 18));
    m_deleteProdBtn->setEnabled(false);

    m_importProdBtn = new QPushButton(L.t("Import"));
    m_importProdBtn->setObjectName("secondaryBtn");
    m_importProdBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("import-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_importProdBtn->setIconSize(QSize(18, 18));
    m_importProdBtn->setToolTip(L.t("Import products from Excel file"));

    toolbarLayout->addWidget(m_searchEdit);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(m_catFilterCombo);
    toolbarLayout->addWidget(m_manageCatBtn);
    toolbarLayout->addWidget(m_importProdBtn);
    toolbarLayout->addWidget(m_addProdBtn);
    toolbarLayout->addWidget(m_editProdBtn);
    toolbarLayout->addWidget(m_deleteProdBtn);

    prodTabLayout->addWidget(toolbar);

    // Table
    m_prodModel = new ProductTableModel(this);
    auto* prodProxy = new ArabicSortProxy(this);
    prodProxy->setSourceModel(m_prodModel);

    m_prodTableView = new QTableView;
    m_prodTableView->setObjectName("dataTable");
    m_prodTableView->setModel(prodProxy);
    m_prodTableView->setSortingEnabled(true);
    m_prodTableView->horizontalHeader()->setSortIndicatorShown(true);
    m_prodTableView->horizontalHeader()->setStretchLastSection(false);
    m_prodTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_prodTableView->horizontalHeader()->setSectionResizeMode(ProductTableModel::ColName,     QHeaderView::Stretch);
    m_prodTableView->horizontalHeader()->setSectionResizeMode(ProductTableModel::ColBarcode,  QHeaderView::Fixed);
    m_prodTableView->horizontalHeader()->setSectionResizeMode(ProductTableModel::ColManufacture, QHeaderView::Fixed);
    m_prodTableView->horizontalHeader()->setSectionResizeMode(ProductTableModel::ColExpiry,   QHeaderView::Fixed);
    m_prodTableView->setColumnWidth(ProductTableModel::ColBarcode,  130);
    m_prodTableView->setColumnWidth(ProductTableModel::ColManufacture, 120);
    m_prodTableView->setColumnWidth(ProductTableModel::ColExpiry,   120);
    m_prodTableView->horizontalHeader()->setMinimumSectionSize(60);
    m_prodTableView->verticalHeader()->setVisible(false);
    m_prodTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_prodTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_prodTableView->setAlternatingRowColors(true);
    m_prodTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_prodTableView->setColumnHidden(ProductTableModel::ColId, true);
    m_prodTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_prodTableView, &QTableView::customContextMenuRequested,
            this, &ProductsPage::showProductContextMenu);

    prodTabLayout->addWidget(m_prodTableView, 1);

    // Status bar
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 4, 16, 4);
    m_prodStatusLabel = new QLabel("0 products");
    m_prodStatusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(m_prodStatusLabel);
    prodTabLayout->addWidget(statusBar);

    tabs->addTab(prodTabWidget, "");
    tabs->setTabIcon(0, SvgIconHelper::icon(sidebarSvgPath("product-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(0, L.t("Products Directory"));

    // ── TAB 2: MISSING PRODUCTS LOG ───────────────────────────────────────
    auto* missingWidget = new QWidget;
    auto* missingLayout = new QVBoxLayout(missingWidget);
    missingLayout->setSpacing(0);
    missingLayout->setContentsMargins(0, 0, 0, 0);

    auto* missingToolbar = new QFrame;
    missingToolbar->setObjectName("pageToolbar");
    auto* missingToolbarLayout = new QHBoxLayout(missingToolbar);
    missingToolbarLayout->setContentsMargins(16, 10, 16, 10);
    missingToolbarLayout->setSpacing(10);

    // #10: search bar replaces the static title label
    m_missingSearchEdit = new QLineEdit;
    m_missingSearchEdit->setPlaceholderText(L.t("Search by name or barcode..."));
    m_missingSearchEdit->setObjectName("searchEdit");
    m_missingSearchEdit->setMinimumWidth(220);
    
    // Add search icon
    QAction* missingSearchAction = new QAction(m_missingSearchEdit);
    missingSearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                      QColor("#9CA3AF"), 16));
    m_missingSearchEdit->addAction(missingSearchAction, QLineEdit::LeadingPosition);

    m_addMissingBtn = new QPushButton(L.t("＋ Report Missing Product"));
    m_addMissingBtn->setObjectName("primaryBtn");

    m_toggleResolveBtn = new QPushButton(L.t("Toggle Resolved"));
    m_toggleResolveBtn->setObjectName("secondaryBtn");
    m_toggleResolveBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("validate-svgrepo-com.svg"), 
                                                      QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_toggleResolveBtn->setIconSize(QSize(18, 18));
    m_toggleResolveBtn->setEnabled(false);

    m_deleteMissingBtn = new QPushButton(L.t("Delete"));
    m_deleteMissingBtn->setObjectName("dangerBtn");
    m_deleteMissingBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                      QColor("#FFFFFF"), 18));
    m_deleteMissingBtn->setIconSize(QSize(18, 18));
    m_deleteMissingBtn->setEnabled(false);

    m_importMissingBtn = new QPushButton(L.t("Import Data"));
    m_importMissingBtn->setObjectName("secondaryBtn");
    m_importMissingBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("import-svgrepo-com.svg"), 
                                                      QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_importMissingBtn->setIconSize(QSize(18, 18));

    missingToolbarLayout->addWidget(m_missingSearchEdit);
    missingToolbarLayout->addStretch();
    missingToolbarLayout->addWidget(m_importMissingBtn);
    missingToolbarLayout->addWidget(m_addMissingBtn);
    missingToolbarLayout->addWidget(m_toggleResolveBtn);
    missingToolbarLayout->addWidget(m_deleteMissingBtn);

    missingLayout->addWidget(missingToolbar);

    m_missingTable = new QTableWidget;
    m_missingTable->setObjectName("dataTable");
    m_missingTable->setColumnCount(6);
    m_missingTable->setHorizontalHeaderLabels({
        L.t("Product Requested"), L.t("Barcode"),
        L.t("Qty Required"), L.t("Current Stock"),
        L.t("Date Added"), L.t("Status")
    });
    m_missingTable->horizontalHeader()->setStretchLastSection(true);
    m_missingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_missingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_missingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_missingTable->verticalHeader()->setVisible(false);
    m_missingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_missingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_missingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_missingTable->setSortingEnabled(true);

    missingLayout->addWidget(m_missingTable, 1);

    tabs->addTab(missingWidget, "");
    tabs->setTabIcon(1, SvgIconHelper::icon(sidebarSvgPath("lost-items-missing-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(1, L.t("Missing Products"));

    // ── TAB 3: EXPIRED PRODUCTS ───────────────────────────────────────────
    auto* expiredWidget = new QWidget;
    auto* expiredLayout = new QVBoxLayout(expiredWidget);
    expiredLayout->setSpacing(0);
    expiredLayout->setContentsMargins(0, 0, 0, 0);

    auto* expiredToolbar = new QFrame;
    expiredToolbar->setObjectName("pageToolbar");
    auto* expiredTbLayout = new QHBoxLayout(expiredToolbar);
    expiredTbLayout->setContentsMargins(16, 10, 16, 10);
    expiredTbLayout->setSpacing(10);

    auto* expiredTitle = new QLabel("Expired Products");
    expiredTitle->setObjectName("pageTitle");

    m_restoreExpiredBtn = new QPushButton(L.t("Restore to Active"));
    m_restoreExpiredBtn->setObjectName("secondaryBtn");
    m_restoreExpiredBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("restore-svgrepo-com.svg"), 
                                                       QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_restoreExpiredBtn->setIconSize(QSize(18, 18));
    m_restoreExpiredBtn->setEnabled(false);
    m_restoreExpiredBtn->setToolTip(L.t("Mark selected product as Active again"));

    m_deleteExpiredBtn = new QPushButton("Delete");
    m_deleteExpiredBtn->setObjectName("dangerBtn");
    m_deleteExpiredBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                      QColor("#FFFFFF"), 18));
    m_deleteExpiredBtn->setIconSize(QSize(18, 18));
    m_deleteExpiredBtn->setEnabled(false);

    m_refreshExpiredBtn = new QPushButton(L.t("Refresh"));
    m_refreshExpiredBtn->setObjectName("secondaryBtn");
    m_refreshExpiredBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("refresh-svgrepo-com.svg"), 
                                                       QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_refreshExpiredBtn->setIconSize(QSize(18, 18));
    m_refreshExpiredBtn->setToolTip(L.t("Reload expired products list"));

    expiredTbLayout->addWidget(expiredTitle);
    expiredTbLayout->addStretch();
    expiredTbLayout->addWidget(m_refreshExpiredBtn);
    expiredTbLayout->addWidget(m_restoreExpiredBtn);
    expiredTbLayout->addWidget(m_deleteExpiredBtn);
    expiredLayout->addWidget(expiredToolbar);

    m_expiredTable = new QTableWidget;
    m_expiredTable->setObjectName("dataTable");
    m_expiredTable->setColumnCount(6);
    m_expiredTable->setHorizontalHeaderLabels({
        L.t("ID"), L.t("Name"), L.t("Barcode"),
        L.t("Category"), L.t("Price"), L.t("Expired On")
    });
    m_expiredTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_expiredTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_expiredTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_expiredTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_expiredTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_expiredTable->verticalHeader()->setVisible(false);
    m_expiredTable->setColumnHidden(0, true); // hide ID col
    m_expiredTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_expiredTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_expiredTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_expiredTable->setAlternatingRowColors(true);
    m_expiredTable->setSortingEnabled(true);
    expiredLayout->addWidget(m_expiredTable, 1);

    // status bar for expired tab
    auto* expiredStatusBar = new QFrame;
    expiredStatusBar->setObjectName("statusBar");
    auto* expiredStatusLayout = new QHBoxLayout(expiredStatusBar);
    expiredStatusLayout->setContentsMargins(16, 4, 16, 4);
    auto* expiredStatusLbl = new QLabel;
    expiredStatusLbl->setObjectName("statusLabel");
    expiredStatusLbl->setObjectName("expiredStatusLbl");
    expiredStatusLayout->addWidget(expiredStatusLbl);
    expiredLayout->addWidget(expiredStatusBar);

    tabs->addTab(expiredWidget, "");
    tabs->setTabIcon(2, SvgIconHelper::icon(sidebarSvgPath("hourglass-expired-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(2, L.t("Expired Products"));

    // ── TAB 4: PROMOTIONS (مجلة العروض) ───────────────────────────────────
    auto* promotionsWidget = new QWidget;
    auto* promotionsLayout = new QVBoxLayout(promotionsWidget);
    promotionsLayout->setSpacing(0);
    promotionsLayout->setContentsMargins(0, 0, 0, 0);

    // Toolbar for promotions
    auto* promoToolbar = new QFrame;
    promoToolbar->setObjectName("pageToolbar");
    auto* promoTbLayout = new QHBoxLayout(promoToolbar);
    promoTbLayout->setContentsMargins(16, 10, 16, 10);
    promoTbLayout->setSpacing(10);

    auto* promoTitle = new QLabel;
    promoTitle->setObjectName("pageTitle");
    promoTitle->setText("📰 " + L.t("Promotions Magazine"));

    // Search box for promotions
    m_promotionSearchEdit = new QLineEdit;
    m_promotionSearchEdit->setPlaceholderText(L.t("search_promotions"));
    m_promotionSearchEdit->setMaximumWidth(300);
    connect(m_promotionSearchEdit, &QLineEdit::textChanged, this, &ProductsPage::onPromotionSearchChanged);

    // Buttons (فوق!) - بالترتيب: Add, Edit, Delete, Refresh
    m_addPromotionBtn = new QPushButton(L.t("magazine_new"));
    m_addPromotionBtn->setObjectName("primaryBtn");
    m_addPromotionBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("add-plus-svgrepo-com.svg"),
                                                    QColor("#FFFFFF"), 18));
    m_addPromotionBtn->setIconSize(QSize(18, 18));
    connect(m_addPromotionBtn, &QPushButton::clicked, this, &ProductsPage::onAddPromotion);

    m_editPromotionBtn = new QPushButton(L.t("magazine_edit"));
    m_editPromotionBtn->setObjectName("secondaryBtn");
    m_editPromotionBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"),
                                                     QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editPromotionBtn->setIconSize(QSize(18, 18));
    connect(m_editPromotionBtn, &QPushButton::clicked, this, &ProductsPage::onEditPromotion);

    m_deletePromotionBtn = new QPushButton(L.t("magazine_delete"));
    m_deletePromotionBtn->setObjectName("dangerBtn");
    m_deletePromotionBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"),
                                                       QColor("#FFFFFF"), 18));
    m_deletePromotionBtn->setIconSize(QSize(18, 18));
    connect(m_deletePromotionBtn, &QPushButton::clicked, this, &ProductsPage::onDeletePromotion);

    m_refreshPromotionsBtn = new QPushButton;
    m_refreshPromotionsBtn->setObjectName("secondaryBtn");
    m_refreshPromotionsBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("refresh-svgrepo-com.svg"),
                                                         QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_refreshPromotionsBtn->setIconSize(QSize(18, 18));
    m_refreshPromotionsBtn->setToolTip(L.t("Refresh"));
    connect(m_refreshPromotionsBtn, &QPushButton::clicked, this, &ProductsPage::onRefreshPromotions);

    promoTbLayout->addWidget(promoTitle);
    promoTbLayout->addWidget(m_promotionSearchEdit);
    promoTbLayout->addStretch();
    promoTbLayout->addWidget(m_addPromotionBtn);
    promoTbLayout->addWidget(m_editPromotionBtn);
    promoTbLayout->addWidget(m_deletePromotionBtn);
    promoTbLayout->addWidget(m_refreshPromotionsBtn);
    promotionsLayout->addWidget(promoToolbar);

    // Table for promotions
    m_promotionsTable = new QTableWidget;
    m_promotionsTable->setObjectName("dataTable");
    m_promotionsTable->setColumnCount(6);
    m_promotionsTable->setHorizontalHeaderLabels({
        L.t("ID"), L.t("promotion_title"), L.t("promotion_type"),
        L.t("promotion_start_date"), L.t("promotion_end_date"), L.t("promotion_status")
    });
    m_promotionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_promotionsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_promotionsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_promotionsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_promotionsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_promotionsTable->verticalHeader()->setVisible(false);
    m_promotionsTable->setColumnHidden(0, true); // hide ID
    m_promotionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_promotionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_promotionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_promotionsTable->setAlternatingRowColors(true);
    m_promotionsTable->setSortingEnabled(true);
    promotionsLayout->addWidget(m_promotionsTable, 1);

    tabs->addTab(promotionsWidget, "");
    tabs->setTabIcon(3, SvgIconHelper::icon(sidebarSvgPath("offer-promotion-fire-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(3, L.t("Promotions Magazine"));

    root->addWidget(tabs);

    // ── Connections ─────────────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProductsPage::onSearchProducts);
    connect(m_catFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProductsPage::onCategoryFilterChanged);
    connect(m_addProdBtn,    &QPushButton::clicked, this, &ProductsPage::onAddProduct);
    connect(m_editProdBtn,   &QPushButton::clicked, this, &ProductsPage::onEditProduct);
    connect(m_deleteProdBtn, &QPushButton::clicked, this, &ProductsPage::onDeleteProduct);
    connect(m_manageCatBtn,  &QPushButton::clicked, this, &ProductsPage::onManageCategories);
    connect(m_importProdBtn, &QPushButton::clicked, this, &ProductsPage::onImportProductsDirectory);

    connect(m_prodTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ProductsPage::onProductSelectionChanged);
    // C1a: Restore double-click on Products Directory to the full ProductDialog
    // (same as clicking the Edit button — all product fields, not the quick-edit 3-field dialog)
    connect(m_prodTableView, &QTableView::doubleClicked,
            this, [this](const QModelIndex&) { onEditProduct(); });

    connect(m_addMissingBtn,    &QPushButton::clicked, this, &ProductsPage::onAddMissingProduct);
    connect(m_toggleResolveBtn, &QPushButton::clicked, this, &ProductsPage::onToggleMissingResolved);
    connect(m_importMissingBtn, &QPushButton::clicked, this, &ProductsPage::onImportMissingProducts);
    connect(m_deleteMissingBtn, &QPushButton::clicked, this, &ProductsPage::onDeleteMissingProduct);
    // #10: live search in missing products tab
    if (m_missingSearchEdit) {
        connect(m_missingSearchEdit, &QLineEdit::textChanged, this,
            [this](const QString&){ loadMissingProducts(); });
    }
    connect(m_missingTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool has = !m_missingTable->selectedItems().isEmpty();
        m_toggleResolveBtn->setEnabled(has);
        m_deleteMissingBtn->setEnabled(has);
    });
    // Issue 5: double-click opens the unified edit dialog (not toggle-resolved)
    connect(m_missingTable, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) { onMissingProductDoubleClicked(row); });

    // ── Expired tab connections ────────────────────────────────────────────
    connect(m_refreshExpiredBtn, &QPushButton::clicked, this, [this]() {
        loadProducts();
        loadExpiredProducts();
    });
    connect(m_expiredTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool has = !m_expiredTable->selectedItems().isEmpty();
        m_restoreExpiredBtn->setEnabled(has);
        m_deleteExpiredBtn->setEnabled(has);
    });
    connect(m_restoreExpiredBtn, &QPushButton::clicked, this, [this]() {
        int row = m_expiredTable->currentRow();
        if (row < 0) return;
        int pid = m_expiredTable->item(row, 0)->text().toInt();
        Product p = m_productRepo->getById(pid);
        if (p.id() == 0) return;

        // ── Popup: edit manufacture + expiry dates before restoring ─────────
        QDialog dlg(this);
        dlg.setWindowTitle("Restore Product — Update Dates");
        dlg.setMinimumWidth(380);
        dlg.setModal(true);

        auto* layout = new QVBoxLayout(&dlg);
        layout->setSpacing(12);
        layout->setContentsMargins(20, 16, 20, 16);

        auto* info = new QLabel(QString("<b>%1</b><br><span style='color:#94A3B8;font-size:11px;'>"
            "Set new dates then click Save to restore this product to Active.</span>")
            .arg(p.name()));
        info->setWordWrap(true);
        layout->addWidget(info);

        auto* form = new QFormLayout;
        form->setSpacing(10);

        auto* mfgEdit = new QDateEdit(p.manufactureDate().isValid()
            ? p.manufactureDate() : QDate::currentDate());
        mfgEdit->setDisplayFormat("yyyy-MM-dd");
        mfgEdit->setCalendarPopup(true);
        form->addRow("Manufacture Date:", mfgEdit);

        auto* expEdit = new QDateEdit(p.expiryDate().isValid()
            ? p.expiryDate() : QDate::currentDate().addMonths(6));
        expEdit->setDisplayFormat("yyyy-MM-dd");
        expEdit->setCalendarPopup(true);
        form->addRow("New Expiry Date:", expEdit);
        layout->addLayout(form);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
        layout->addWidget(btns);

        QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&]() {
            if (expEdit->date() <= QDate::currentDate()) {
                QMessageBox::warning(&dlg, "Invalid Date",
                    "New expiry date must be in the future to restore the product as Active.");
                return;
            }
            dlg.accept();
        });
        QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return;

        p.setManufactureDate(mfgEdit->date());
        p.setExpiryDate(expEdit->date());
        p.setStatus(Product::Status::Active);

        if (m_productRepo->save(p)) {
            Logger::instance().info("Restored expired product with new dates: " + p.name());
            // Remove from table immediately
            m_expiredTable->removeRow(row);
            loadProducts(); // refresh main products tab too
        } else {
            QMessageBox::critical(this, "Error", "Failed to restore product.");
        }
    });
    connect(m_deleteExpiredBtn, &QPushButton::clicked, this, [this]() {
        int row = m_expiredTable->currentRow();
        if (row < 0) return;
        int pid    = m_expiredTable->item(row, 0)->text().toInt();
        QString nm = m_expiredTable->item(row, 1)->text();
        auto reply = QMessageBox::question(this, "Confirm Delete",
            QString("Permanently delete expired product \"%1\"?").arg(nm),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        // Expired product soft-delete (no manual snapshot needed — archive stores it)
        QString archErr;
        int aid = ArchiveManager::instance().softDelete("Products", "Id", pid, &archErr);
        if (aid < 0) {
            Logger::instance().warn("Failed to archive expired product: " + archErr);
        } else {
            AuditService::instance().logDelete("Product", pid,
                QString("{\"reason\":\"expired\",\"archiveId\":%1,\"name\":\"%2\"}")
                    .arg(aid).arg(nm));
        }
        Logger::instance().info("Deleted expired product: " + nm);
        refresh();
    });
}

void ProductsPage::refresh() {
    loadCategories();
    loadProducts();
    loadMissingProducts();
    loadExpiredProducts();
    loadPromotions();
}

void ProductsPage::loadCategories() {
    m_categories = m_categoryRepo->getAll();
    m_categoryMap.clear();

    m_catFilterCombo->blockSignals(true);
    m_catFilterCombo->clear();
    m_catFilterCombo->addItem("All Categories", 0);

    for (const auto& c : m_categories) {
        m_categoryMap[c.id] = c.name;
        m_catFilterCombo->addItem(c.name, c.id);
    }
    m_catFilterCombo->blockSignals(false);

    m_prodModel->setCategoryMap(m_categoryMap);
}

void ProductsPage::loadProducts() {
    QString keyword = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    int categoryId = m_catFilterCombo ? m_catFilterCombo->currentData().toInt() : 0;

    QList<Product> list;
    if (!keyword.isEmpty()) {
        // SEARCH: load ALL matching products (search must not be affected by pagination)
        list = m_productRepo->search(keyword);
    } else {
        // LAZY LOADING: load first 200 products only for initial performance
        // User can search to access any product beyond first 200
        list = m_productRepo->getPage(0, 200);
    }

    // Apply category filter if selected
    if (categoryId > 0) {
        QList<Product> filtered;
        for (const auto& p : list) {
            if (p.categoryId() == categoryId)
                filtered.append(p);
        }
        list = filtered;
    }

    // ── Auto-expire: products past expiry date → set status to Hidden ────────
    // (no longer auto-added to Missing Products — expired tab handles them)
    QDate today = QDate::currentDate();
    for (auto& p : list) {
        if (p.expiryDate().isValid() && p.expiryDate() < today
            && p.status() == Product::Status::Active) {
            p.setStatus(Product::Status::Hidden);
            m_productRepo->save(p);
            Logger::instance().warn("Product expired (hidden): " + p.name());
        }
    }

    m_prodModel->setProducts(list);
    
    // Show total count to indicate there are more products available via search
    int totalCount = m_productRepo->getTotalCount();
    if (keyword.isEmpty() && totalCount > 200) {
        m_prodStatusLabel->setText(QString::number(list.size()) + " of " + QString::number(totalCount) + " product(s) shown. Use search to find more.");
    } else {
        m_prodStatusLabel->setText(QString::number(list.size()) + " product(s)");
    }
}

void ProductsPage::loadMissingProducts() {
    m_missingProducts = m_missingRepo->getAll();

    // #10: apply search filter from m_missingSearchEdit
    QString kw = m_missingSearchEdit ? m_missingSearchEdit->text().trimmed().toLower() : QString();
    QList<MissingProduct> filtered;
    if (kw.isEmpty()) {
        filtered = m_missingProducts;
    } else {
        for (const auto& mp : m_missingProducts) {
            if (mp.productName.toLower().contains(kw) || mp.barcode.toLower().contains(kw))
                filtered.append(mp);
        }
    }

    // FIX: disable sorting during populate to prevent Qt from scrambling
    // rows mid-insertion (causes data to appear in wrong cells or disappear).
    m_missingTable->setSortingEnabled(false);
    m_missingTable->setRowCount(0);

    for (const auto& item : filtered) {
        int row = m_missingTable->rowCount();
        m_missingTable->insertRow(row);

        auto mkC = [](const QString& txt, Qt::Alignment al = Qt::AlignVCenter | Qt::AlignLeft) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(al);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        QString statusText = item.purchased ? "✓ Resolved" : "⏳ Pending";
        // BUG 1 FIX: show currentQty for all sources where it's set (> 0), not just auto_low_stock.
        // For manual requests this is the live-stock snapshot captured at creation time.
        QString currentQtyText = (item.currentQty > 0)
            ? QString::number(item.currentQty) : "—";
        // Issue 5: show "12 كرتونة" style
        QString qtyText = item.unitLabel.isEmpty()
            ? QString::number(item.quantityNeeded)
            : QString("%1 %2").arg(item.quantityNeeded).arg(item.unitLabel);

        auto* itemProd   = mkC(item.productName);
        auto* itemBarc   = mkC(item.barcode.isEmpty() ? "—" : item.barcode, Qt::AlignCenter);
        auto* itemQty    = mkC(qtyText, Qt::AlignCenter);
        auto* itemCurQty = mkC(currentQtyText, Qt::AlignCenter);
        auto* itemDate   = mkC(item.dateAdded.toString("yyyy-MM-dd"), Qt::AlignCenter);
        auto* itemStatus = mkC(statusText, Qt::AlignCenter);

        // UserRole sort keys: numeric/date values for correct column sorting
        itemProd->setData(Qt::UserRole + 1, item.productName.toLower());
        itemBarc->setData(Qt::UserRole + 1, item.barcode.toLower());
        itemQty->setData(Qt::UserRole + 1,  item.quantityNeeded);
        itemCurQty->setData(Qt::UserRole + 1, item.currentQty);
        itemDate->setData(Qt::UserRole + 1, item.dateAdded.isValid()
            ? (double)item.dateAdded.toJulianDay() : 0.0);
        itemStatus->setData(Qt::UserRole + 1, item.purchased ? 1 : 0);

        if (item.purchased) {
            itemStatus->setForeground(QBrush(QColor("#4ADE80")));
        } else {
            itemStatus->setForeground(QBrush(QColor("#F87171")));
        }

        // Auto-flagged low-stock rows: tint current qty red if at/below threshold
        if (item.source == "auto_low_stock" && !item.purchased) {
            itemCurQty->setForeground(QBrush(QColor("#F87171")));
        }

        itemProd->setData(Qt::UserRole, item.id);

        m_missingTable->setItem(row, 0, itemProd);
        m_missingTable->setItem(row, 1, itemBarc);
        m_missingTable->setItem(row, 2, itemQty);
        m_missingTable->setItem(row, 3, itemCurQty);
        m_missingTable->setItem(row, 4, itemDate);
        m_missingTable->setItem(row, 5, itemStatus);
    }
    // Re-enable sorting now that all rows are fully populated
    m_missingTable->setSortingEnabled(true);
}

void ProductsPage::loadExpiredProducts() {
    // A product is "expired" if its ExpiryDate is valid AND in the past
    QDate today = QDate::currentDate();
    QList<Product> all = m_productRepo->getAll();

    m_expiredTable->setSortingEnabled(false);  // prevent row scrambling
    m_expiredTable->setRowCount(0);
    int count = 0;    const QString sym = ConfigManager::instance().currencySymbol();

    for (const auto& p : all) {
        bool isExpired = p.expiryDate().isValid() && p.expiryDate() < today;
        // Also include status == Hidden that were set by auto-expire
        bool isHiddenExpired = (p.status() == Product::Status::Hidden)
                               && p.expiryDate().isValid()
                               && p.expiryDate() < today;
        if (!isExpired && !isHiddenExpired) continue;

        int row = m_expiredTable->rowCount();
        m_expiredTable->insertRow(row);

        auto cell = [&](const QString& txt, Qt::Alignment al = Qt::AlignVCenter | Qt::AlignLeft) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(al);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            it->setForeground(QBrush(QColor("#F87171"))); // red tint for expired
            return it;
        };

        auto* c0 = cell(QString::number(p.id()), Qt::AlignCenter);
        auto* c1 = cell(p.name());
        auto* c2 = cell(p.barcode().isEmpty() ? "—" : p.barcode(), Qt::AlignCenter);
        auto* c3 = cell(m_categoryMap.value(p.categoryId(), "—"), Qt::AlignCenter);
        auto* c4 = cell(QString("%1 %2").arg(QString::number(p.price(), 'f', 2), sym), Qt::AlignCenter);
        auto* c5 = cell(p.expiryDate().toString("yyyy-MM-dd"), Qt::AlignCenter);

        // UserRole+1 sort keys
        c0->setData(Qt::UserRole + 1, p.id());
        c1->setData(Qt::UserRole + 1, p.name().toLower());
        c2->setData(Qt::UserRole + 1, p.barcode().toLower());
        c3->setData(Qt::UserRole + 1, m_categoryMap.value(p.categoryId(), "").toLower());
        c4->setData(Qt::UserRole + 1, p.price());
        c5->setData(Qt::UserRole + 1, p.expiryDate().isValid()
            ? (double)p.expiryDate().toJulianDay() : 0.0);

        m_expiredTable->setItem(row, 0, c0);
        m_expiredTable->setItem(row, 1, c1);
        m_expiredTable->setItem(row, 2, c2);
        m_expiredTable->setItem(row, 3, c3);
        m_expiredTable->setItem(row, 4, c4);
        m_expiredTable->setItem(row, 5, c5);

        ++count;
    }

    // Update the status label in the expired tab's status bar
    if (auto* lbl = m_expiredTable->parentWidget()
                    ? m_expiredTable->parentWidget()->findChild<QLabel*>("expiredStatusLbl")
                    : nullptr) {
        lbl->setText(QString("%1 expired product(s)").arg(count));
    }

    m_restoreExpiredBtn->setEnabled(false);
    m_deleteExpiredBtn->setEnabled(false);
    m_expiredTable->setSortingEnabled(true);
}

void ProductsPage::onCategoryFilterChanged(int /*index*/) {
    loadProducts();
}

void ProductsPage::onAddProduct() {
    if (!SessionManager::instance().currentRole().canAddProduct) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to add products.");
        return;
    }
    ProductDialog dlg(m_categories, m_subUnitRepo, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Product p = dlg.getProduct();
    if (!m_productRepo->save(p)) {
        QMessageBox::critical(this, "Error", "Failed to save product.");
        return;
    }
    
    // Save sub-units
    dlg.saveSubUnits(p.id());
    
    // Check low-stock flag after saving
    if (auto* sqRepo = dynamic_cast<SQLiteProductRepository*>(m_productRepo))
        sqRepo->checkAndUpdateLowStockFlag(p, m_missingRepo);

    loadProducts();
    loadMissingProducts();
    Logger::instance().info("Product added: " + p.name());
    AuditService::instance().logCreate("Product", p.id(), p.name());
}

void ProductsPage::onEditProduct() {
    if (!SessionManager::instance().currentRole().canEditProduct) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to edit products.");
        return;
    }
    QModelIndexList sel = m_prodTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    // Map proxy index → source index
    auto* proxy = qobject_cast<ArabicSortProxy*>(m_prodTableView->model());
    int row = proxy ? proxy->mapToSource(sel.first()).row() : sel.first().row();
    Product p = m_prodModel->productAt(row);

    QList<ProductPriceHistory> history = m_productRepo->getPriceHistory(p.id());

    ProductDialog dlg(m_categories, m_subUnitRepo, this);
    dlg.setProduct(p, history);
    if (dlg.exec() != QDialog::Accepted) return;

    Product updated = dlg.getProduct();
    if (!m_productRepo->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update product.");
        return;
    }
    
    // Save sub-units
    dlg.saveSubUnits(updated.id());
    
    // Check low-stock flag after every product edit
    if (auto* sqRepo = dynamic_cast<SQLiteProductRepository*>(m_productRepo))
        sqRepo->checkAndUpdateLowStockFlag(updated, m_missingRepo);

    loadProducts();
    loadMissingProducts();
    Logger::instance().info("Product updated: " + updated.name());

    // Snapshot of BEFORE state (p = original before dialog opened)
    auto makeProductSnap = [](const Product& prod) -> QJsonObject {
        QJsonObject s;
        s["id"]               = prod.id();
        s["name"]             = prod.name();
        s["barcode"]          = prod.barcode();
        s["price"]            = prod.price();
        s["categoryId"]       = prod.categoryId();
        s["status"]           = Product::statusToString(prod.status());
        s["mfgDate"]          = prod.manufactureDate().isValid()
                                    ? prod.manufactureDate().toString("yyyy-MM-dd") : "";
        s["expDate"]          = prod.expiryDate().isValid()
                                    ? prod.expiryDate().toString("yyyy-MM-dd") : "";
        s["stockQty"]         = prod.stockQty();
        s["lowStockThreshold"]= prod.lowStockThreshold();
        return s;
    };
    QString details = QJsonDocument(QJsonObject{{"before", makeProductSnap(p)}})
                          .toJson(QJsonDocument::Compact);
    AuditService::instance().logUpdate("Product", updated.id(), details);
}

void ProductsPage::onDeleteProduct() {
    if (!SessionManager::instance().currentRole().canDeleteProduct) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to delete products.");
        return;
    }
    QModelIndexList sel = m_prodTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    // Map proxy index → source index
    auto* proxy = qobject_cast<ArabicSortProxy*>(m_prodTableView->model());
    int row = proxy ? proxy->mapToSource(sel.first()).row() : sel.first().row();
    // Copy by value — avoid dangling ref after model mutation
    Product p = m_prodModel->productAt(row);
    int pid = p.id(); QString pname = p.name();
    if (pid <= 0) return;

    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete product \"%1\"?").arg(pname),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Task 11: soft-delete
    QString archiveErr;
    int archiveId = ArchiveManager::instance().softDelete("Products", "Id", pid, &archiveErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive product before deletion:\n" + archiveErr);
        return;
    }
    m_prodModel->removeProduct(row);
    m_prodStatusLabel->setText(QString::number(m_prodModel->rowCount()) + " product(s)");
    m_prodTableView->selectionModel()->clearSelection();
    Logger::instance().info("Product deleted: " + pname);

    // Build JSON snapshot for undo restore
    QJsonObject snap;
    snap["id"]          = pid;
    snap["name"]        = p.name();
    snap["barcode"]     = p.barcode();
    snap["price"]       = p.price();
    snap["categoryId"]  = p.categoryId();
    snap["status"]      = Product::statusToString(p.status());
    snap["mfgDate"]     = p.manufactureDate().isValid() ? p.manufactureDate().toString("yyyy-MM-dd") : "";
    snap["expDate"]     = p.expiryDate().isValid()      ? p.expiryDate().toString("yyyy-MM-dd")      : "";
    snap["stockQty"]          = p.stockQty();
    snap["lowStockThreshold"] = p.lowStockThreshold();
    QString details = QJsonDocument(QJsonObject{
        {"before", snap},
        {"archiveId", archiveId}
    }).toJson(QJsonDocument::Compact);
    AuditService::instance().logDelete("Product", pid, details);
}

void ProductsPage::onProductSelectionChanged() {
    bool hasSelection = !m_prodTableView->selectionModel()->selectedRows().isEmpty();
    m_editProdBtn->setEnabled(hasSelection);
    m_deleteProdBtn->setEnabled(hasSelection);
}

void ProductsPage::showProductContextMenu(const QPoint& pos) {
    QModelIndex index = m_prodTableView->indexAt(pos);
    if (!index.isValid()) return;

    auto& L = LangManager::instance();
    QMenu menu(this);
    
    QAction* editAction = menu.addAction(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), QColor("#000000")), 
                                         L.t("Edit Product"));
    QAction* barcodeAction = menu.addAction(SvgIconHelper::icon(sidebarSvgPath("barcode scan.svg"), QColor("#000000")), 
                                            L.t("Print Barcode"));
    QAction* suppliersAction = menu.addAction(SvgIconHelper::icon(sidebarSvgPath("suppliers.svg"), QColor("#000000")), 
                                              L.t("Manage Suppliers"));
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), QColor("#dc2626")), 
                                           L.t("Delete Product"));
    deleteAction->setObjectName("dangerAction");

    QAction* selected = menu.exec(m_prodTableView->viewport()->mapToGlobal(pos));
    if (selected == editAction) {
        onEditProduct();
    } else if (selected == barcodeAction) {
        onPrintBarcode();
    } else if (selected == suppliersAction) {
        onManageSuppliers();
    } else if (selected == deleteAction) {
        onDeleteProduct();
    }
}

void ProductsPage::onPrintBarcode() {
    auto selection = m_prodTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    QModelIndex proxyIdx = selection.first();
    QModelIndex sourceIdx = qobject_cast<QSortFilterProxyModel*>(m_prodTableView->model())->mapToSource(proxyIdx);
    int row = sourceIdx.row();
    const Product& product = m_prodModel->productAt(row);

    // Open barcode print dialog
    auto* dialog = new BarcodePrintDialog(product, this);
    dialog->exec();
    dialog->deleteLater();
}

void ProductsPage::onManageCategories() {
    // ── Inline Categories CRUD dialog (same UX as Manage Regions) ────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Categories");
    dlg.setMinimumWidth(360);
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* list = new QListWidget;
    auto reload = [&]() {
        list->clear();
        m_categories = m_categoryRepo->getAll();
        for (const auto& c : m_categories) {
            auto* item = new QListWidgetItem(c.name);
            item->setData(Qt::UserRole, c.id);
            list->addItem(item);
        }
    };
    reload();
    layout->addWidget(list);

    auto* inputRow = new QHBoxLayout;
    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("Category name...");
    auto* addBtn  = new QPushButton("＋ Add");
    addBtn->setObjectName("addBtn");
    auto* delBtn  = new QPushButton("Delete");
    delBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                        QColor(ThemeManager::instance().tokens().textPrimary), 18));
    delBtn->setObjectName("removeBtn");
    inputRow->addWidget(nameEdit, 1);
    inputRow->addWidget(addBtn);
    inputRow->addWidget(delBtn);
    layout->addLayout(inputRow);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("secondaryBtn");
    layout->addWidget(closeBtn);

    QObject::connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) return;
        Category c; c.name = name;
        if (m_categoryRepo->save(c)) { nameEdit->clear(); reload(); }
        else QMessageBox::warning(&dlg, "Error", "Failed to add category.");
    });
    QObject::connect(nameEdit, &QLineEdit::returnPressed, addBtn, &QPushButton::click);
    QObject::connect(delBtn, &QPushButton::clicked, &dlg, [&]() {
        auto* item = list->currentItem();
        if (!item) { QMessageBox::information(&dlg, "Select", "Please select a category first."); return; }
        int id = item->data(Qt::UserRole).toInt();
        auto reply = QMessageBox::question(&dlg, "Confirm", "Delete category \"" + item->text() + "\"?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) { m_categoryRepo->remove(id); reload(); }
    });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
    loadCategories();
    loadProducts();
}

void ProductsPage::onAddMissingProduct() {
    // Issue 4: new dialog with product catalog search + unit label
    QList<Product> products = m_productRepo->getAll();
    // Pass current missing products for duplicate detection
    QList<MissingProduct> currentMissing = m_missingRepo->getAll();
    MissingProductDialog dlg(products, currentMissing, this);
    if (dlg.exec() != QDialog::Accepted) return;

    MissingProduct item = dlg.getMissingProduct();
    item.dateAdded = QDate::currentDate();
    item.purchased = false;
    item.source    = "manual";

    if (dlg.conflictAction() == MissingProductDialog::ConflictAction::Replace
        && dlg.conflictExistingId() > 0) {
        // Replace: update the existing record's qty+unit instead of inserting new
        item.id = dlg.conflictExistingId();
    }

    if (!m_missingRepo->save(item)) {
        QMessageBox::critical(this, "Error", "Failed to record missing product request.");
        return;
    }
    loadMissingProducts();
    Logger::instance().info("Missing product request logged: " + item.productName);
}

void ProductsPage::onToggleMissingResolved() {
    int row = m_missingTable->currentRow();
    if (row < 0) return;

    // BUG FIX: use the DB id stored in column-0 UserRole, NOT the visual row index.
    // When the user sorts the table, currentRow() is the visual position which
    // no longer matches the m_missingProducts list order.
    auto* idItem = m_missingTable->item(row, 0);
    if (!idItem) return;
    int dbId = idItem->data(Qt::UserRole).toInt();

    // Find the matching record by DB id
    int dataIdx = -1;
    for (int i = 0; i < m_missingProducts.size(); ++i) {
        if (m_missingProducts.at(i).id == dbId) { dataIdx = i; break; }
    }
    if (dataIdx < 0) return;

    MissingProduct item = m_missingProducts.at(dataIdx);
    item.purchased = !item.purchased;

    if (m_missingRepo->save(item)) {
        loadMissingProducts();
        Logger::instance().info("Toggled missing product purchased status for ID: " + QString::number(item.id));
    } else {
        QMessageBox::critical(this, "Error", "Failed to update status.");
    }
}

void ProductsPage::onDeleteMissingProduct() {
    int row = m_missingTable->currentRow();
    if (row < 0) return;

    // BUG FIX: same as onToggleMissingResolved — use DB id, not visual row index
    auto* idItem = m_missingTable->item(row, 0);
    if (!idItem) return;
    int dbId = idItem->data(Qt::UserRole).toInt();

    int dataIdx = -1;
    for (int i = 0; i < m_missingProducts.size(); ++i) {
        if (m_missingProducts.at(i).id == dbId) { dataIdx = i; break; }
    }
    if (dataIdx < 0) return;

    const MissingProduct& mp = m_missingProducts.at(dataIdx);
    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete missing product request for \"%1\"?").arg(mp.productName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    if (m_missingRepo->remove(mp.id)) {
        loadMissingProducts();
        Logger::instance().info("Deleted missing product request ID=" + QString::number(mp.id));
    } else {
        QMessageBox::critical(this, "Error", "Failed to delete request.");
    }
}

void ProductsPage::retranslateUi() { retranslateWidget(this); }

// ── Issue 5: double-click on Missing Products → unified edit dialog ───────────
void ProductsPage::onMissingProductDoubleClicked(int row) {
    if (row < 0) return;

    // BUG FIX: use DB id from UserRole, not visual row index
    auto* idItem = m_missingTable->item(row, 0);
    if (!idItem) return;
    int dbId = idItem->data(Qt::UserRole).toInt();

    int dataIdx = -1;
    for (int i = 0; i < m_missingProducts.size(); ++i) {
        if (m_missingProducts.at(i).id == dbId) { dataIdx = i; break; }
    }
    if (dataIdx < 0) return;

    MissingProduct mp = m_missingProducts.at(dataIdx);

    QList<Product> products = m_productRepo->getAll();
    MissingProductDialog dlg(products, {}, this);   // no duplicate check on edit
    dlg.setWindowTitle("Edit Missing Product Request");
    dlg.setMissingProduct(mp);

    if (dlg.exec() != QDialog::Accepted) return;

    MissingProduct updated = dlg.getMissingProduct();
    updated.id        = mp.id;          // preserve DB id
    updated.purchased = mp.purchased;   // preserve resolved state
    updated.source    = mp.source;
    updated.dateAdded = mp.dateAdded;
    updated.branchId  = mp.branchId;
    updated.currentQty= mp.currentQty;

    if (!m_missingRepo->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update missing product request.");
        return;
    }
    loadMissingProducts();
    Logger::instance().info("Missing product request updated: " + updated.productName);
}



// ── Task C: Import Missing Products from Excel ────────────────────────────────
void ProductsPage::onImportMissingProducts() {
    // ── Import dialog with instructions and template download ─────────────────
    QDialog importDlg(this);
    importDlg.setWindowTitle("Import Missing Products from Excel");
    importDlg.setMinimumWidth(520);
    importDlg.setModal(true);

    auto* mainLayout = new QVBoxLayout(&importDlg);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 16, 20, 16);

    auto* instrLabel = new QLabel(
        "<b>Import format (one product per row):</b><br>"
        "Columns: <code>Product Name | Barcode | Qty Needed | Date Added (yyyy-MM-dd) | Unit</code><br>"
        "<span style='color:#94A3B8;font-size:11px;'>"
        "Each imported row creates a <b>Pending</b> request and — if a product with "
        "matching Name or Barcode does not already exist — also adds it to the "
        "Products Directory.<br>"
        "Incomplete rows are imported with the available fields; missing fields are left blank."
        "</span>");
    instrLabel->setWordWrap(true);
    mainLayout->addWidget(instrLabel);

    auto* btnRow = new QHBoxLayout;
    auto* templateBtn = new QPushButton("⬇ Download Template");
    templateBtn->setObjectName("secondaryBtn");
    btnRow->addWidget(templateBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Open)->setText("📂 Choose File…");
    btns->button(QDialogButtonBox::Open)->setObjectName("primaryBtn");
    mainLayout->addWidget(btns);

    // Template download
    QObject::connect(templateBtn, &QPushButton::clicked, &importDlg, [&]() {
        QString savePath = QFileDialog::getSaveFileName(
            &importDlg, "Save Template", "missing_products_template.xlsx",
            "Excel Files (*.xlsx)");
        if (savePath.isEmpty()) return;
        QXlsx::Document tmpl;
        QXlsx::Format hdr;
        hdr.setFontBold(true);
        hdr.setPatternBackgroundColor(QColor("#1E293B"));
        hdr.setFontColor(QColor("#FFFFFF"));
        tmpl.write(1, 1, "Product Name",    hdr);
        tmpl.write(1, 2, "Barcode",         hdr);
        tmpl.write(1, 3, "Quantity Needed", hdr);
        tmpl.write(1, 4, "Date Added",      hdr);
        tmpl.write(1, 5, "Unit",            hdr);
        // Example row
        tmpl.write(2, 1, "Example Product");
        tmpl.write(2, 2, "1234567890");
        tmpl.write(2, 3, 5);
        tmpl.write(2, 4, QDate::currentDate().toString("yyyy-MM-dd"));
        tmpl.write(2, 5, QString::fromUtf8("قطعة"));
        if (tmpl.saveAs(savePath))
            QMessageBox::information(&importDlg, "Template Saved",
                "Template saved to:\n" + savePath);
        else
            QMessageBox::warning(&importDlg, "Error", "Failed to save template.");
    });

    QObject::connect(btns, &QDialogButtonBox::rejected, &importDlg, &QDialog::reject);
    QObject::connect(btns, &QDialogButtonBox::accepted, &importDlg, &QDialog::accept);

    if (importDlg.exec() != QDialog::Accepted) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Excel File", QString(), "Excel Files (*.xlsx)");
    if (filePath.isEmpty()) return;

    QXlsx::Document doc(filePath);
    if (!doc.load()) {
        QMessageBox::critical(this, "Error", "Failed to open Excel file:\n" + filePath);
        return;
    }

    // Build existing product lookups — separate maps for name and barcode
    // so that a match on EITHER field is sufficient (avoids false "not found"
    // when the Excel row has a blank barcode but the DB record has one, or vice-versa)
    QList<Product> allProducts = m_productRepo->getAll();
    QMap<QString, int> productByName;    // name (lower) → id
    QMap<QString, int> productByBarcode; // barcode (lower, non-empty) → id
    for (const auto& p : allProducts) {
        productByName[p.name().toLower()] = p.id();
        if (!p.barcode().isEmpty())
            productByBarcode[p.barcode().toLower()] = p.id();
    }

    // Helper: find existing product id by name OR barcode (returns 0 if not found)
    auto findExistingId = [&](const QString& name, const QString& barcode) -> int {
        QString nameLow = name.toLower();
        QString barcLow = barcode.toLower();
        if (!nameLow.isEmpty() && productByName.contains(nameLow))
            return productByName[nameLow];
        if (!barcLow.isEmpty() && productByBarcode.contains(barcLow))
            return productByBarcode[barcLow];
        return 0;
    };

    // Build existing missing-product lookup for duplicate detection
    // key: productName (lower) OR barcode (lower) → existing MissingProduct
    QList<MissingProduct> existingMissing = m_missingRepo->getAll();
    QMap<QString, MissingProduct> missingByName;
    QMap<QString, MissingProduct> missingByBarcode;
    for (const auto& mp2 : existingMissing) {
        if (!mp2.productName.isEmpty())
            missingByName[mp2.productName.toLower()] = mp2;
        if (!mp2.barcode.isEmpty())
            missingByBarcode[mp2.barcode.toLower()] = mp2;
    }

    // Helper: find existing MissingProduct by name OR barcode
    auto findExistingMissing = [&](const QString& n, const QString& bc) -> MissingProduct {
        if (!n.isEmpty() && missingByName.contains(n.toLower()))
            return missingByName[n.toLower()];
        if (!bc.isEmpty() && missingByBarcode.contains(bc.toLower()))
            return missingByBarcode[bc.toLower()];
        return {};
    };

    int totalRows = doc.dimension().lastRow();
    int imported = 0, skippedHeader = 0;
    bool abortImport = false;
    bool skipAllMissing = false;   // "Skip All" flag for missing-product duplicates
    bool replaceAllMissing = false; // "Replace All" flag
    QStringList newlyAddedToDirectory;

    for (int row = 1; row <= totalRows && !abortImport; ++row) {
        QString name       = doc.read(row, 1).toString().trimmed();
        QString barcode    = doc.read(row, 2).toString().trimmed();
        QString qtyStr     = doc.read(row, 3).toString().trimmed();
        QString dateStr    = doc.read(row, 4).toString().trimmed();
        QString unitStr    = doc.read(row, 5).toString().trimmed();
        QString curQtyStr;  // no longer in template — will be auto-filled from product directory

        // Skip header row
        if (row == 1 && (name.compare("product name", Qt::CaseInsensitive) == 0 ||
                          name.compare("product", Qt::CaseInsensitive) == 0)) {
            ++skippedHeader;
            continue;
        }
        // Skip completely empty rows
        if (name.isEmpty() && barcode.isEmpty() && qtyStr.isEmpty()) continue;

        // ── Check: already in Missing Products? ───────────────────────────────
        MissingProduct existingMp = findExistingMissing(name, barcode);
        bool hasMissingConflict = (existingMp.id > 0);

        if (hasMissingConflict && !skipAllMissing && !replaceAllMissing) {
            // Build conflict dialog identical in style to Products Directory import
            QDialog conflictDlg(this);
            conflictDlg.setWindowTitle("Duplicate in Missing Products List");
            conflictDlg.setMinimumWidth(520);
            conflictDlg.setModal(true);

            auto* cl = new QVBoxLayout(&conflictDlg);
            cl->setSpacing(12);
            cl->setContentsMargins(20, 16, 20, 16);

            auto* titleLbl = new QLabel(
                QString("<b style='color:#F87171;'>⚠ Duplicate on row %1</b>").arg(row));
            cl->addWidget(titleLbl);

            auto* reasonLbl = new QLabel(
                QString("<span style='color:#94A3B8;'>This product already exists in the "
                        "Missing Products list.</span>"));
            reasonLbl->setWordWrap(true);
            cl->addWidget(reasonLbl);

            // Comparison grid: Excel vs existing request
            auto* grid = new QGridLayout;
            grid->setSpacing(6);
            auto addHdr = [&](int col, const QString& t) {
                auto* l = new QLabel(t);
                l->setStyleSheet("font-weight:bold; color:#94A3B8;");
                grid->addWidget(l, 0, col);
            };
            auto addCell = [&](int r2, int col, const QString& t, bool hi = false) {
                auto* l = new QLabel(t.isEmpty() ? "—" : t);
                l->setStyleSheet(hi ? "color:#F87171;" : "color:#E2E8F0;");
                grid->addWidget(l, r2, col);
            };
            addHdr(0, "Field");
            addHdr(1, "In Excel (new)");
            addHdr(2, "Existing Request");

            QStringList fields   = {"Product Name", "Barcode", "Qty Needed", "Unit", "Date Added"};
            QStringList excelV   = {name, barcode, qtyStr, unitStr, dateStr};            QStringList existV   = {
                existingMp.productName,
                existingMp.barcode,
                QString::number(existingMp.quantityNeeded),
                existingMp.unitLabel,
                existingMp.dateAdded.toString("yyyy-MM-dd")
            };
            for (int i = 0; i < fields.size(); ++i) {
                auto* fl = new QLabel(fields[i]);
                fl->setStyleSheet("color:#64748B;");
                grid->addWidget(fl, i + 1, 0);
                bool diff = (excelV[i] != existV[i]) && !excelV[i].isEmpty();
                addCell(i + 1, 1, excelV[i], diff);
                addCell(i + 1, 2, existV[i], false);
            }
            cl->addLayout(grid);

            auto* sep = new QFrame;
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color:#334155;");
            cl->addWidget(sep);

            auto* btnRow = new QHBoxLayout;
            auto* skipBtn       = new QPushButton("⏭ Skip");
            auto* skipAllBtn    = new QPushButton("⏭ Skip All");
            auto* replaceBtn    = new QPushButton("🔄 Replace");
            auto* replaceAllBtn = new QPushButton("🔄 Replace All");
            auto* stopBtn       = new QPushButton("⛔ Stop Import");
            skipBtn->setObjectName("secondaryBtn");
            skipAllBtn->setObjectName("secondaryBtn");
            replaceBtn->setObjectName("primaryBtn");
            replaceAllBtn->setObjectName("primaryBtn");
            stopBtn->setObjectName("dangerBtn");
            btnRow->addWidget(skipBtn);
            btnRow->addWidget(skipAllBtn);
            btnRow->addStretch();
            btnRow->addWidget(replaceBtn);
            btnRow->addWidget(replaceAllBtn);
            btnRow->addWidget(stopBtn);
            cl->addLayout(btnRow);

            enum class MConflict { Skip, SkipAll, Replace, ReplaceAll, Stop };
            MConflict result = MConflict::Skip;

            QObject::connect(skipBtn,       &QPushButton::clicked, &conflictDlg, [&](){ result = MConflict::Skip;       conflictDlg.accept(); });
            QObject::connect(skipAllBtn,    &QPushButton::clicked, &conflictDlg, [&](){ result = MConflict::SkipAll;    conflictDlg.accept(); });
            QObject::connect(replaceBtn,    &QPushButton::clicked, &conflictDlg, [&](){ result = MConflict::Replace;    conflictDlg.accept(); });
            QObject::connect(replaceAllBtn, &QPushButton::clicked, &conflictDlg, [&](){ result = MConflict::ReplaceAll; conflictDlg.accept(); });
            QObject::connect(stopBtn,       &QPushButton::clicked, &conflictDlg, [&](){ result = MConflict::Stop;       conflictDlg.accept(); });

            conflictDlg.exec();

            if (result == MConflict::Stop)       { abortImport = true; break; }
            if (result == MConflict::SkipAll)    { skipAllMissing = true; continue; }
            if (result == MConflict::Skip)       { continue; }
            if (result == MConflict::ReplaceAll) { replaceAllMissing = true; }
            // Replace / ReplaceAll: update existing request and continue below
            if (result == MConflict::Replace || result == MConflict::ReplaceAll) {
                existingMp.quantityNeeded = qtyStr.isEmpty() ? existingMp.quantityNeeded : qtyStr.toInt();
                existingMp.currentQty     = curQtyStr.isEmpty() ? existingMp.currentQty : curQtyStr.toInt();
                existingMp.unitLabel      = unitStr.isEmpty() ? existingMp.unitLabel : unitStr;
                if (!dateStr.isEmpty()) {
                    QDate d = QDate::fromString(dateStr, "yyyy-MM-dd");
                    if (d.isValid()) existingMp.dateAdded = d;
                }
                m_missingRepo->save(existingMp);
                // Update lookup so subsequent rows see the refreshed state
                if (!existingMp.productName.isEmpty())
                    missingByName[existingMp.productName.toLower()] = existingMp;
                if (!existingMp.barcode.isEmpty())
                    missingByBarcode[existingMp.barcode.toLower()] = existingMp;
                ++imported;
                continue;
            }
        } else if (hasMissingConflict && skipAllMissing) {
            continue;
        } else if (hasMissingConflict && replaceAllMissing) {
            existingMp.quantityNeeded = qtyStr.isEmpty() ? existingMp.quantityNeeded : qtyStr.toInt();
            existingMp.currentQty     = curQtyStr.isEmpty() ? existingMp.currentQty : curQtyStr.toInt();
            existingMp.unitLabel      = unitStr.isEmpty() ? existingMp.unitLabel : unitStr;
            if (!dateStr.isEmpty()) {
                QDate d = QDate::fromString(dateStr, "yyyy-MM-dd");
                if (d.isValid()) existingMp.dateAdded = d;
            }
            m_missingRepo->save(existingMp);
            if (!existingMp.productName.isEmpty())
                missingByName[existingMp.productName.toLower()] = existingMp;
            if (!existingMp.barcode.isEmpty())
                missingByBarcode[existingMp.barcode.toLower()] = existingMp;
            ++imported;
            continue;
        }

        // ── a) Create a new Pending missing-product request ──────────────────
        MissingProduct mp;
        mp.productName    = name;
        mp.barcode        = barcode;
        mp.quantityNeeded = qtyStr.isEmpty() ? 1 : qtyStr.toInt();
        if (mp.quantityNeeded <= 0) mp.quantityNeeded = 1;
        mp.unitLabel  = unitStr;
        mp.dateAdded  = dateStr.isEmpty()
            ? QDate::currentDate()
            : QDate::fromString(dateStr, "yyyy-MM-dd");
        if (!mp.dateAdded.isValid()) mp.dateAdded = QDate::currentDate();
        mp.purchased = false;
        mp.source    = "manual";

        // Link to existing product if found by name OR barcode.
        // Also fill in the missing field from the product directory:
        //   • Excel has name but no barcode  → look up by name and fill barcode
        //   • Excel has barcode but no name  → look up by barcode and fill name
        int existingId = findExistingId(name, barcode);
        if (existingId > 0) {
            mp.productId = existingId;
            for (const auto& p : allProducts) {
                if (p.id() == existingId) {
                    // Fill missing name from directory
                    if (mp.productName.isEmpty() && !p.name().isEmpty())
                        mp.productName = p.name();
                    // Fill missing barcode from directory
                    if (mp.barcode.isEmpty() && !p.barcode().isEmpty())
                        mp.barcode = p.barcode();
                    mp.currentQty = p.stockQty();
                    break;
                }
            }
        }

        m_missingRepo->save(mp);
        ++imported;

        // ── b) Create in Products Directory ONLY if not found by name or barcode
        if (existingId == 0 && !name.isEmpty()) {
            Product newProd;
            newProd.setName(name);
            newProd.setBarcode(barcode);
            newProd.setStockQty(0);   // unknown stock — leave at 0, not quantityNeeded
            newProd.setStatus(Product::Status::Active);
            if (m_productRepo->save(newProd)) {
                // Update both lookup maps so later rows don't re-create the same product
                productByName[name.toLower()] = newProd.id();
                if (!barcode.isEmpty())
                    productByBarcode[barcode.toLower()] = newProd.id();
                allProducts.append(newProd);
                // Link the missing request to the new product
                mp.productId = newProd.id();
                m_missingRepo->save(mp);
                newlyAddedToDirectory.append(name);
            }
        }
    }

    loadMissingProducts();
    loadProducts();
    Logger::instance().info(QString("Missing Products import: %1 row(s) imported").arg(imported));

    // ── Summary message ────────────────────────────────────────────────────
    QString summary = QString("Import complete.\n%1 request(s) created.").arg(imported);
    if (skippedHeader > 0)
        summary += "\n(Header row skipped.)";
    if (!newlyAddedToDirectory.isEmpty()) {
        summary += QString("\n\n%1 product(s) were not in the Products Directory and have been added automatically:\n")
                       .arg(newlyAddedToDirectory.size());
        for (const QString& n : newlyAddedToDirectory)
            summary += "  • " + n + "\n";
    }
    QMessageBox::information(this, "Import Complete", summary);
}

// ── Import Products Directory from Excel ─────────────────────────────────────
void ProductsPage::onImportProductsDirectory()
{
    // ── Step 1: Intro dialog with instructions + template download ────────────
    QDialog importDlg(this);
    importDlg.setWindowTitle("Import Products from Excel");
    importDlg.setMinimumWidth(560);
    importDlg.setModal(true);

    auto* mainLayout = new QVBoxLayout(&importDlg);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 16, 20, 16);

    auto* instrLabel = new QLabel(
        "<b>Import Products Directory — format (one product per row):</b><br>"
        "Columns: <code>Name | Barcode | Price | Category | Stock Qty | "
        "Low Stock Threshold | Manufacture Date (yyyy-MM-dd) | Expiry Date (yyyy-MM-dd) | Status</code><br>"
        "<span style='color:#94A3B8;font-size:11px;'>"
        "• <b>Category</b>: name must match an existing category, or leave blank.<br>"
        "• <b>Status</b>: Active or Hidden (default: Active).<br>"
        "• <b>Dates</b>: use yyyy-MM-dd format or leave blank.<br>"
        "• If a product with the same <b>Name</b> or <b>Barcode</b> already exists "
        "you will be asked how to handle it (Skip / Replace / Stop)."
        "</span>");
    instrLabel->setWordWrap(true);
    mainLayout->addWidget(instrLabel);

    auto* btnRow = new QHBoxLayout;
    auto* templateBtn = new QPushButton("⬇ Download Template");
    templateBtn->setObjectName("secondaryBtn");
    btnRow->addWidget(templateBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Open)->setText("📂 Choose File…");
    btns->button(QDialogButtonBox::Open)->setObjectName("primaryBtn");
    mainLayout->addWidget(btns);

    // ── Template download ─────────────────────────────────────────────────────
    QObject::connect(templateBtn, &QPushButton::clicked, &importDlg, [&]() {
        QString savePath = QFileDialog::getSaveFileName(
            &importDlg, "Save Template", "products_directory_template.xlsx",
            "Excel Files (*.xlsx)");
        if (savePath.isEmpty()) return;
        QXlsx::Document tmpl;
        QXlsx::Format hdr;
        hdr.setFontBold(true);
        hdr.setPatternBackgroundColor(QColor("#1E293B"));
        hdr.setFontColor(QColor("#FFFFFF"));
        tmpl.write(1, 1, "Name",               hdr);
        tmpl.write(1, 2, "Barcode",             hdr);
        tmpl.write(1, 3, "Price",               hdr);
        tmpl.write(1, 4, "Category",            hdr);
        tmpl.write(1, 5, "Stock Qty",           hdr);
        tmpl.write(1, 6, "Low Stock Threshold", hdr);
        tmpl.write(1, 7, "Manufacture Date",    hdr);
        tmpl.write(1, 8, "Expiry Date",         hdr);
        tmpl.write(1, 9, "Status",              hdr);
        // Example row
        tmpl.write(2, 1, "Example Product");
        tmpl.write(2, 2, "1234567890123");
        tmpl.write(2, 3, 9.99);
        tmpl.write(2, 4, "General");
        tmpl.write(2, 5, 50);
        tmpl.write(2, 6, 5);
        tmpl.write(2, 7, QDate::currentDate().toString("yyyy-MM-dd"));
        tmpl.write(2, 8, QDate::currentDate().addYears(1).toString("yyyy-MM-dd"));
        tmpl.write(2, 9, "Active");
        if (tmpl.saveAs(savePath))
            QMessageBox::information(&importDlg, "Template Saved",
                "Template saved to:\n" + savePath);
        else
            QMessageBox::warning(&importDlg, "Error", "Failed to save template.");
    });

    QObject::connect(btns, &QDialogButtonBox::rejected, &importDlg, &QDialog::reject);
    QObject::connect(btns, &QDialogButtonBox::accepted, &importDlg, &QDialog::accept);

    if (importDlg.exec() != QDialog::Accepted) return;

    // ── Step 2: Choose file ───────────────────────────────────────────────────
    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Excel File", QString(), "Excel Files (*.xlsx)");
    if (filePath.isEmpty()) return;

    QXlsx::Document doc(filePath);
    if (!doc.load()) {
        QMessageBox::critical(this, "Error", "Failed to open Excel file:\n" + filePath);
        return;
    }

    // ── Step 3: Build lookup maps ─────────────────────────────────────────────
    QList<Product> allProducts = m_productRepo->getAll();

    // name (lower) → Product
    QMap<QString, Product> byName;
    // barcode (lower, non-empty) → Product
    QMap<QString, Product> byBarcode;
    for (const auto& p : allProducts) {
        byName[p.name().toLower()] = p;
        if (!p.barcode().isEmpty())
            byBarcode[p.barcode().toLower()] = p;
    }

    // category name (lower) → id
    QMap<QString, int> catByName;
    for (const auto& c : m_categories)
        catByName[c.name.toLower()] = c.id;

    // ── Step 4: Process rows ──────────────────────────────────────────────────
    int totalRows    = doc.dimension().lastRow();
    int imported     = 0;
    int skipped      = 0;
    int replaced     = 0;
    bool abortImport = false;
    bool replaceAll  = false;   // "Replace All" flag set if user clicks that button

    for (int row = 1; row <= totalRows && !abortImport; ++row)
    {
        QString nameVal  = doc.read(row, 1).toString().trimmed();
        QString barcode  = doc.read(row, 2).toString().trimmed();
        QString priceStr = doc.read(row, 3).toString().trimmed();
        QString catName  = doc.read(row, 4).toString().trimmed();
        QString stockStr = doc.read(row, 5).toString().trimmed();
        QString threshStr= doc.read(row, 6).toString().trimmed();
        QString mfgStr   = doc.read(row, 7).toString().trimmed();
        QString expStr   = doc.read(row, 8).toString().trimmed();
        QString statusStr= doc.read(row, 9).toString().trimmed();

        // Skip header row
        if (row == 1 && (nameVal.compare("name", Qt::CaseInsensitive) == 0 ||
                          nameVal.compare("product name", Qt::CaseInsensitive) == 0))
            continue;

        // Skip completely empty rows
        if (nameVal.isEmpty() && barcode.isEmpty()) continue;

        // ── Detect conflict: same name OR same barcode ────────────────────────
        Product existingByName;
        Product existingByBarcode;
        bool nameConflict    = !nameVal.isEmpty()  && byName.contains(nameVal.toLower());
        bool barcodeConflict = !barcode.isEmpty()  && byBarcode.contains(barcode.toLower());

        if (nameConflict)    existingByName    = byName[nameVal.toLower()];
        if (barcodeConflict) existingByBarcode = byBarcode[barcode.toLower()];

        // Pick the "best" existing product for display (prefer name match)
        bool hasConflict = nameConflict || barcodeConflict;
        Product existingProd = nameConflict ? existingByName : existingByBarcode;

        if (hasConflict && !replaceAll)
        {
            // ── Conflict dialog ───────────────────────────────────────────────
            QDialog conflictDlg(this);
            conflictDlg.setWindowTitle("Duplicate Product Found");
            conflictDlg.setMinimumWidth(560);
            conflictDlg.setModal(true);

            auto* cl = new QVBoxLayout(&conflictDlg);
            cl->setSpacing(12);
            cl->setContentsMargins(20, 16, 20, 16);

            // Title
            auto* titleLbl = new QLabel(
                QString("<b style='color:#F87171;'>⚠ Duplicate detected on row %1</b>").arg(row));
            titleLbl->setWordWrap(true);
            cl->addWidget(titleLbl);

            QString conflictReason;
            if (nameConflict && barcodeConflict)
                conflictReason = "Same Name and Barcode";
            else if (nameConflict)
                conflictReason = "Same Name";
            else
                conflictReason = "Same Barcode";

            auto* reasonLbl = new QLabel(
                QString("<span style='color:#94A3B8;'>Conflict reason: %1</span>")
                    .arg(conflictReason));
            cl->addWidget(reasonLbl);

            // Comparison table
            auto* compareWidget = new QWidget;
            auto* grid = new QGridLayout(compareWidget);
            grid->setSpacing(6);
            grid->setContentsMargins(0, 4, 0, 4);

            auto addHeader = [&](int col, const QString& text) {
                auto* lbl = new QLabel(text);
                lbl->setStyleSheet("font-weight:bold; color:#94A3B8;");
                grid->addWidget(lbl, 0, col);
            };
            auto addCell = [&](int row2, int col, const QString& text,
                               bool highlight = false) {
                auto* lbl = new QLabel(text.isEmpty() ? "—" : text);
                if (highlight)
                    lbl->setStyleSheet("color:#F87171;");
                else
                    lbl->setStyleSheet("color:#E2E8F0;");
                grid->addWidget(lbl, row2, col);
            };

            addHeader(0, "Field");
            addHeader(1, "In Excel (new)");
            addHeader(2, "In Database (existing)");

            // Resolve category display for existing product
            QString existingCatName = m_categoryMap.value(existingProd.categoryId(), "—");

            QStringList fields     = {"Name", "Barcode", "Price", "Category",
                                      "Stock Qty", "Low Stock Threshold",
                                      "Manufacture Date", "Expiry Date", "Status"};
            QStringList excelVals  = {nameVal, barcode, priceStr, catName,
                                      stockStr, threshStr, mfgStr, expStr,
                                      statusStr.isEmpty() ? "Active" : statusStr};
            QStringList dbVals     = {
                existingProd.name(),
                existingProd.barcode(),
                QString::number(existingProd.price(), 'f', 2),
                existingCatName,
                QString::number(existingProd.stockQty()),
                QString::number(existingProd.lowStockThreshold()),
                existingProd.manufactureDate().isValid()
                    ? existingProd.manufactureDate().toString("yyyy-MM-dd") : "",
                existingProd.expiryDate().isValid()
                    ? existingProd.expiryDate().toString("yyyy-MM-dd") : "",
                Product::statusToString(existingProd.status())
            };

            for (int i = 0; i < fields.size(); ++i) {
                auto* fLbl = new QLabel(fields[i]);
                fLbl->setStyleSheet("color:#64748B;");
                grid->addWidget(fLbl, i + 1, 0);
                bool diff = (excelVals[i] != dbVals[i]) && !excelVals[i].isEmpty();
                addCell(i + 1, 1, excelVals[i], diff);
                addCell(i + 1, 2, dbVals[i],    false);
            }

            cl->addWidget(compareWidget);

            // Separator
            auto* sep = new QFrame;
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color:#334155;");
            cl->addWidget(sep);

            // Action buttons
            auto* actionRow = new QHBoxLayout;
            auto* skipBtn       = new QPushButton("⏭ Skip");
            auto* replaceBtn    = new QPushButton("🔄 Replace");
            auto* replaceAllBtn = new QPushButton("🔄 Replace All");
            auto* stopBtn       = new QPushButton("⛔ Stop Import");
            skipBtn->setObjectName("secondaryBtn");
            replaceBtn->setObjectName("primaryBtn");
            replaceAllBtn->setObjectName("primaryBtn");
            stopBtn->setObjectName("dangerBtn");
            actionRow->addWidget(skipBtn);
            actionRow->addStretch();
            actionRow->addWidget(replaceBtn);
            actionRow->addWidget(replaceAllBtn);
            actionRow->addWidget(stopBtn);
            cl->addLayout(actionRow);

            // Result from conflict dialog
            enum class ConflictResult { Skip, Replace, ReplaceAll, Stop };
            ConflictResult conflictResult = ConflictResult::Skip;

            QObject::connect(skipBtn,       &QPushButton::clicked, &conflictDlg, [&]() {
                conflictResult = ConflictResult::Skip; conflictDlg.accept(); });
            QObject::connect(replaceBtn,    &QPushButton::clicked, &conflictDlg, [&]() {
                conflictResult = ConflictResult::Replace; conflictDlg.accept(); });
            QObject::connect(replaceAllBtn, &QPushButton::clicked, &conflictDlg, [&]() {
                conflictResult = ConflictResult::ReplaceAll; conflictDlg.accept(); });
            QObject::connect(stopBtn,       &QPushButton::clicked, &conflictDlg, [&]() {
                conflictResult = ConflictResult::Stop; conflictDlg.accept(); });

            conflictDlg.exec();

            if (conflictResult == ConflictResult::Stop) {
                abortImport = true;
                break;
            }
            if (conflictResult == ConflictResult::Skip) {
                ++skipped;
                continue;
            }
            if (conflictResult == ConflictResult::ReplaceAll) {
                replaceAll = true;
                // fall through to replace this row too
            }
            // Replace (or ReplaceAll): update existing product
            if (conflictResult == ConflictResult::Replace ||
                conflictResult == ConflictResult::ReplaceAll)
            {
                // Build updated product from Excel data, keeping existing id
                Product updated = existingProd;
                if (!nameVal.isEmpty())  updated.setName(nameVal);
                if (!barcode.isEmpty())  updated.setBarcode(barcode);
                if (!priceStr.isEmpty()) updated.setPrice(priceStr.toDouble());
                if (!catName.isEmpty()) {
                    int cid = catByName.value(catName.toLower(), 0);
                    if (cid > 0) updated.setCategoryId(cid);
                }
                if (!stockStr.isEmpty())  updated.setStockQty(stockStr.toInt());
                if (!threshStr.isEmpty()) updated.setLowStockThreshold(threshStr.toInt());
                QDate mfgDate = mfgStr.isEmpty()  ? QDate() : QDate::fromString(mfgStr,  "yyyy-MM-dd");
                QDate expDate = expStr.isEmpty()   ? QDate() : QDate::fromString(expStr,  "yyyy-MM-dd");
                if (mfgDate.isValid()) updated.setManufactureDate(mfgDate);
                if (expDate.isValid()) updated.setExpiryDate(expDate);
                if (!statusStr.isEmpty())
                    updated.setStatus(Product::stringToStatus(statusStr));

                if (m_productRepo->save(updated)) {
                    // Refresh lookup maps
                    byName[updated.name().toLower()]   = updated;
                    if (!updated.barcode().isEmpty())
                        byBarcode[updated.barcode().toLower()] = updated;
                    ++replaced;
                    Logger::instance().info("Product replaced via import: " + updated.name());
                    AuditService::instance().logUpdate("Product", updated.id(),
                        QString("{\"source\":\"excel_import\",\"name\":\"%1\"}").arg(updated.name()));
                }
                continue;
            }
        }
        else if (hasConflict && replaceAll)
        {
            // replaceAll was set by a previous row — apply silently
            Product updated = existingProd;
            if (!nameVal.isEmpty())  updated.setName(nameVal);
            if (!barcode.isEmpty())  updated.setBarcode(barcode);
            if (!priceStr.isEmpty()) updated.setPrice(priceStr.toDouble());
            if (!catName.isEmpty()) {
                int cid = catByName.value(catName.toLower(), 0);
                if (cid > 0) updated.setCategoryId(cid);
            }
            if (!stockStr.isEmpty())  updated.setStockQty(stockStr.toInt());
            if (!threshStr.isEmpty()) updated.setLowStockThreshold(threshStr.toInt());
            QDate mfgDate = mfgStr.isEmpty()  ? QDate() : QDate::fromString(mfgStr,  "yyyy-MM-dd");
            QDate expDate = expStr.isEmpty()   ? QDate() : QDate::fromString(expStr,  "yyyy-MM-dd");
            if (mfgDate.isValid()) updated.setManufactureDate(mfgDate);
            if (expDate.isValid()) updated.setExpiryDate(expDate);
            if (!statusStr.isEmpty())
                updated.setStatus(Product::stringToStatus(statusStr));

            if (m_productRepo->save(updated)) {
                byName[updated.name().toLower()]   = updated;
                if (!updated.barcode().isEmpty())
                    byBarcode[updated.barcode().toLower()] = updated;
                ++replaced;
                Logger::instance().info("Product replaced via import (all): " + updated.name());
                AuditService::instance().logUpdate("Product", updated.id(),
                    QString("{\"source\":\"excel_import\",\"name\":\"%1\"}").arg(updated.name()));
            }
            continue;
        }

        // ── No conflict: insert new product ──────────────────────────────────
        if (nameVal.isEmpty()) continue;   // must have at least a name

        Product newProd;
        newProd.setName(nameVal);
        newProd.setBarcode(barcode);
        newProd.setPrice(priceStr.isEmpty() ? 0.0 : priceStr.toDouble());
        if (!catName.isEmpty()) {
            int cid = catByName.value(catName.toLower(), 0);
            if (cid > 0) newProd.setCategoryId(cid);
        }
        newProd.setStockQty(stockStr.isEmpty()  ? 0 : stockStr.toInt());
        newProd.setLowStockThreshold(threshStr.isEmpty() ? 5 : threshStr.toInt());
        QDate mfgDate = mfgStr.isEmpty() ? QDate() : QDate::fromString(mfgStr, "yyyy-MM-dd");
        QDate expDate = expStr.isEmpty() ? QDate() : QDate::fromString(expStr, "yyyy-MM-dd");
        newProd.setManufactureDate(mfgDate);
        newProd.setExpiryDate(expDate);
        newProd.setStatus(statusStr.isEmpty()
            ? Product::Status::Active : Product::stringToStatus(statusStr));

        if (m_productRepo->save(newProd)) {
            byName[newProd.name().toLower()]   = newProd;
            if (!newProd.barcode().isEmpty())
                byBarcode[newProd.barcode().toLower()] = newProd;
            ++imported;
            Logger::instance().info("Product imported: " + newProd.name());
            AuditService::instance().logCreate("Product", newProd.id(), newProd.name());
        }
    }

    loadCategories();
    loadProducts();

    // ── Summary ───────────────────────────────────────────────────────────────
    QString summary;
    if (abortImport)
        summary = "Import stopped by user.\n\n";
    summary += QString("✅ Imported (new):  %1\n"
                       "🔄 Replaced:       %2\n"
                       "⏭ Skipped:        %3")
                   .arg(imported).arg(replaced).arg(skipped);
    QMessageBox::information(this, "Import Complete", summary);
    Logger::instance().info(QString("Products Directory import — new:%1 replaced:%2 skipped:%3")
                                .arg(imported).arg(replaced).arg(skipped));
}

void ProductsPage::onSearchProducts() {
    loadProducts();
}

void ProductsPage::onManageSuppliers() {
    auto selection = m_prodTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                             LangManager::tr("please_select_product"));
        return;
    }

    QModelIndex proxyIdx = selection.first();
    QModelIndex sourceIdx = qobject_cast<QSortFilterProxyModel*>(m_prodTableView->model())->mapToSource(proxyIdx);
    int row = sourceIdx.row();
    const Product& product = m_prodModel->productAt(row);

    // Open suppliers management dialog
    auto* dialog = new ProductSuppliersDialog(product, m_productSupplierRepo, m_supplierRepo, this);
    dialog->exec();
    dialog->deleteLater();
}

// ── Promotions Magazine Implementation ──────────────────────────────────────

void ProductsPage::loadPromotions() {
    if (!m_promotionsTable) return;
    
    m_promotionsTable->setSortingEnabled(false);
    m_promotionsTable->setRowCount(0);
    
    // Get all promotions
    QList<Promotion> promotions = m_promotionRepo->getAll();
    
    for (const auto& promo : promotions) {
        int row = m_promotionsTable->rowCount();
        m_promotionsTable->insertRow(row);
        
        // ID (hidden)
        m_promotionsTable->setItem(row, 0, new QTableWidgetItem(QString::number(promo.id)));
        
        // Title
        m_promotionsTable->setItem(row, 1, new QTableWidgetItem(promo.title));
        
        // Type
        QString typeStr;
        switch (promo.type) {
            case Promotion::Type::Percentage: typeStr = LangManager::tr("promotion_type_percentage"); break;
            case Promotion::Type::FixedAmount: typeStr = LangManager::tr("promotion_type_fixed"); break;
            case Promotion::Type::BuyXGetY: typeStr = LangManager::tr("promotion_type_bogo"); break;
            case Promotion::Type::FreeShipping: typeStr = LangManager::tr("promotion_type_freeship"); break;
            default: typeStr = LangManager::tr("promotion_type_magazine"); break; // Magazine is default
        }
        m_promotionsTable->setItem(row, 2, new QTableWidgetItem(typeStr));
        
        // Start Date (column 3, was 4)
        QString startStr = promo.startDate.isValid() 
            ? promo.startDate.toString("yyyy-MM-dd") 
            : "-";
        m_promotionsTable->setItem(row, 3, new QTableWidgetItem(startStr));
        
        // End Date (column 4, was 5)
        QString endStr = promo.endDate.isValid() 
            ? promo.endDate.toString("yyyy-MM-dd") 
            : "-";
        m_promotionsTable->setItem(row, 4, new QTableWidgetItem(endStr));
        
        // Status (column 5, was 6)
        QString statusStr;
        switch (promo.status) {
            case Promotion::Status::Active: statusStr = LangManager::tr("promotion_status_active"); break;
            case Promotion::Status::Scheduled: statusStr = LangManager::tr("promotion_status_scheduled"); break;
            case Promotion::Status::Expired: statusStr = LangManager::tr("promotion_status_expired"); break;
            case Promotion::Status::Inactive: statusStr = LangManager::tr("promotion_status_inactive"); break;
            default: statusStr = "-"; break;
        }
        m_promotionsTable->setItem(row, 5, new QTableWidgetItem(statusStr));
    }
    
    m_promotionsTable->setSortingEnabled(true);
}

void ProductsPage::onAddPromotion() {
    auto* dialog = new MagazineDialog(m_promotionRepo, m_productRepo, this);
    
    if (dialog->exec() == QDialog::Accepted) {
        // MagazineDialog::onSave() already persisted the promotion, its
        // product associations, AND each product's magazine price directly
        // via the repository before calling accept(). Do not repeat any of
        // that here — see onEditPromotion() below for what happens when we do.
        Promotion promo = dialog->getPromotion();

        Logger::instance().info("Created promotion magazine: " + promo.title);
        AuditService::instance().logCreate("Promotion", promo.id, promo.title);

        loadPromotions();
        QMessageBox::information(this, LangManager::tr("success"), 
                               LangManager::tr("promotion_created_successfully"));
    }
    
    dialog->deleteLater();
}

void ProductsPage::onEditPromotion() {
    int row = m_promotionsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                           LangManager::tr("please_select_promotion"));
        return;
    }
    
    // Get promotion ID from hidden column
    int promoId = m_promotionsTable->item(row, 0)->text().toInt();
    Promotion promo = m_promotionRepo->getById(promoId);
    
    if (promo.id == 0) {
        QMessageBox::critical(this, LangManager::tr("error"), 
                            LangManager::tr("promotion_not_found"));
        return;
    }
    
    // Get associated products
    QList<int> productIds = m_promotionRepo->getProductsForPromotion(promoId);
    
    // Open dialog
    auto* dialog = new MagazineDialog(m_promotionRepo, m_productRepo, this);
    dialog->setPromotion(promo, productIds);
    
    if (dialog->exec() == QDialog::Accepted) {
        // MagazineDialog::onSave() already did the full save itself:
        // it re-saved the promotion, called removeAllProductsFromPromotion()
        // then addProductToPromotion() + setProductMagazinePrice() for every
        // row in its table, using the price the user actually typed.
        //
        // This code used to repeat removeAllProductsFromPromotion() +
        // addProductToPromotion() a SECOND time right here. addProductToPromotion()
        // inserts each row with MagazinePrice hard-coded to 0, and this second
        // pass never called setProductMagazinePrice() afterward — so every
        // magazine price the dialog had just saved correctly was immediately
        // wiped back to 0 on every edit. That's why POS showed the original
        // price instead of the magazine price after editing a magazine.
        Promotion updatedPromo = dialog->getPromotion();

        Logger::instance().info("Updated promotion magazine: " + updatedPromo.title);
        AuditService::instance().logUpdate("Promotion", updatedPromo.id, updatedPromo.title);

        loadPromotions();
        QMessageBox::information(this, LangManager::tr("success"), 
                               LangManager::tr("promotion_updated_successfully"));
    }
    
    dialog->deleteLater();
}

void ProductsPage::onDeletePromotion() {
    int row = m_promotionsTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                           LangManager::tr("please_select_promotion"));
        return;
    }
    
    int promoId = m_promotionsTable->item(row, 0)->text().toInt();
    QString title = m_promotionsTable->item(row, 1)->text();
    
    auto reply = QMessageBox::question(this, 
                                      LangManager::tr("confirm_delete"), 
                                      LangManager::tr("delete_promotion_confirm").arg(title),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    // Remove product associations first
    m_promotionRepo->removeAllProductsFromPromotion(promoId);
    
    // Delete promotion
    if (m_promotionRepo->remove(promoId)) {
        Logger::instance().info("Deleted promotion: " + title);
        AuditService::instance().logDelete("Promotion", promoId, title);
        
        m_promotionsTable->removeRow(row);
        QMessageBox::information(this, LangManager::tr("success"), 
                               LangManager::tr("promotion_deleted_successfully"));
    } else {
        QMessageBox::critical(this, LangManager::tr("error"), 
                            LangManager::tr("failed_to_delete_promotion"));
    }
}

void ProductsPage::onRefreshPromotions() {
    loadPromotions();
}

void ProductsPage::onPromotionSearchChanged() {
    if (!m_promotionSearchEdit || !m_promotionsTable) return;
    
    QString keyword = m_promotionSearchEdit->text().trimmed().toLower();
    
    for (int row = 0; row < m_promotionsTable->rowCount(); ++row) {
        bool match = false;
        
        if (keyword.isEmpty()) {
            match = true;
        } else {
            // Search in title, type, and status
            QString title = m_promotionsTable->item(row, 1)->text().toLower();
            QString type = m_promotionsTable->item(row, 2)->text().toLower();
            QString status = m_promotionsTable->item(row, 6)->text().toLower();
            
            match = title.contains(keyword) || 
                   type.contains(keyword) || 
                   status.contains(keyword);
        }
        
        m_promotionsTable->setRowHidden(row, !match);
    }
}
