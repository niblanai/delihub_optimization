#include "orders_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "order_dialog.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "ui/tr_helper.h"
#include "ui/arabic_sort_proxy.h"
#include "services/session_manager.h"
#include "services/invoice_generator.h"
#include "services/audit_service.h"
#include "ui/pagination_bar.h"
#include <QFileDialog>
#include <QCoreApplication>
#include <QFile>
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCheckBox>
#include <QFrame>
#include <QSplitter>
#include <QTimer>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

OrdersPage::OrdersPage(QWidget* parent)
    : QWidget(parent)
{
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_orderRepo         = new SQLiteOrderRepository;
        m_customerRepo      = new SQLiteCustomerRepository;
        m_productRepo       = new SQLiteProductRepository;
        m_driverRepo        = new SQLiteDeliveryDriverRepository;
        m_regionRepo        = new SQLiteRegionRepository;   // Task 2
        m_couponRepo        = new SQLiteCouponRepository;   // Task 12
        m_stockMovementRepo = new SQLiteStockMovementRepository; // Phase 1 Task 15
    } else {
        m_orderRepo         = new AccessOrderRepository;
        m_customerRepo      = new AccessCustomerRepository;
        m_productRepo       = new AccessProductRepository;
        m_driverRepo        = new AccessDeliveryDriverRepository;
        m_regionRepo        = new AccessRegionRepository;   // Task 2
        m_couponRepo        = new AccessCouponRepository;   // Task 12
        m_stockMovementRepo = new AccessStockMovementRepository; // Phase 1 Task 15
    }

    m_orderService = new OrderService(m_orderRepo, m_customerRepo, m_productRepo, nullptr);

    setupUi();
    // Data is loaded by MainWindow::navigateTo(3) — no singleShot needed
}

// ─────────────────────────────────────────────────────────────────────────────
// UI construction
// ─────────────────────────────────────────────────────────────────────────────
void OrdersPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto* tabs = new QTabWidget;

    // ═══════════════════════════════════════════════════════════════════════
    // TAB 1 — ALL ORDERS
    // ═══════════════════════════════════════════════════════════════════════
    auto* allOrdersWidget = new QWidget;
    auto* allOrdersLayout = new QVBoxLayout(allOrdersWidget);
    allOrdersLayout->setSpacing(0);
    allOrdersLayout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QVBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 8, 16, 8);
    tbLayout->setSpacing(6);

    // Row 1: title + action buttons
    auto* tbRow1 = new QHBoxLayout;
    auto* titleLabel = new QLabel("Orders");
    titleLabel->setObjectName("pageTitle");
    titleLabel->setVisible(false);

    m_addOrderBtn    = new QPushButton(LangManager::instance().t("＋ New Order"));
    m_addOrderBtn->setObjectName("primaryBtn");
    
    m_editOrderBtn   = new QPushButton(LangManager::instance().t("Edit"));
    m_editOrderBtn->setObjectName("secondaryBtn");
    m_editOrderBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                  QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editOrderBtn->setIconSize(QSize(18, 18));
    m_editOrderBtn->setEnabled(false);
    
    m_deleteOrderBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteOrderBtn->setObjectName("dangerBtn");
    m_deleteOrderBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                    QColor("#FFFFFF"), 18));
    m_deleteOrderBtn->setIconSize(QSize(18, 18));
    m_deleteOrderBtn->setEnabled(false);
    
    m_invoiceBtn     = new QPushButton(LangManager::instance().t("🧾 Invoice"));
    m_invoiceBtn->setObjectName("secondaryBtn");
    m_invoiceBtn->setEnabled(false);

    tbRow1->addWidget(titleLabel);
    tbRow1->addStretch();
    tbRow1->addWidget(m_addOrderBtn);
    tbRow1->addWidget(m_editOrderBtn);
    tbRow1->addWidget(m_deleteOrderBtn);
    tbRow1->addWidget(m_invoiceBtn);
    tbLayout->addLayout(tbRow1);

    // Row 2: search + date filter
    auto* tbRow2 = new QHBoxLayout;
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search by customer name or invoice # (e.g. INV-0042)..."));
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMinimumWidth(220);
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);

    m_dateFilterCheck = new QCheckBox(LangManager::instance().t("Filter by date:"));
    m_dateFilterCheck->setStyleSheet("color:#94A3B8;");

    m_dateFromEdit = new QDateEdit(QDate::currentDate().addDays(-30));
    m_dateFromEdit->setCalendarPopup(true);
    m_dateFromEdit->setDisplayFormat("yyyy-MM-dd");
    m_dateFromEdit->setEnabled(false);

    auto* toLabel = new QLabel("to");
    toLabel->setStyleSheet("color:#94A3B8;");

    m_dateToEdit = new QDateEdit(QDate::currentDate());
    m_dateToEdit->setCalendarPopup(true);
    m_dateToEdit->setDisplayFormat("yyyy-MM-dd");
    m_dateToEdit->setEnabled(false);

    tbRow2->addWidget(m_searchEdit);
    tbRow2->addStretch();
    tbRow2->addWidget(m_dateFilterCheck);
    tbRow2->addWidget(m_dateFromEdit);
    tbRow2->addWidget(toLabel);
    tbRow2->addWidget(m_dateToEdit);
    tbLayout->addLayout(tbRow2);

    allOrdersLayout->addWidget(toolbar);

    // Table
    m_ordersModel = new OrderTableModel(this);
    auto* ordProxy = new ArabicSortProxy(this);
    ordProxy->setSourceModel(m_ordersModel);

    m_ordersTableView = new QTableView;
    m_ordersTableView->setObjectName("dataTable");
    m_ordersTableView->setModel(ordProxy);
    m_ordersTableView->setSortingEnabled(true);
    m_ordersTableView->horizontalHeader()->setSortIndicatorShown(true);
    m_ordersTableView->horizontalHeader()->setStretchLastSection(false);
    m_ordersTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_ordersTableView->horizontalHeader()->setSectionResizeMode(
        OrderTableModel::ColCustomer, QHeaderView::Stretch);
    m_ordersTableView->verticalHeader()->setVisible(false);
    m_ordersTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ordersTableView->setAlternatingRowColors(true);
    m_ordersTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ordersTableView->setColumnHidden(OrderTableModel::ColId, true);
    allOrdersLayout->addWidget(m_ordersTableView, 1);

    // Pagination bar
    m_ordersPagination = new PaginationBar;
    allOrdersLayout->addWidget(m_ordersPagination);

    // Status bar
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 4, 16, 4);
    m_statusLabel = new QLabel("0 orders");
    m_statusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(m_statusLabel);
    allOrdersLayout->addWidget(statusBar);

    tabs->addTab(allOrdersWidget, "");
    tabs->setTabIcon(0, SvgIconHelper::icon(sidebarSvgPath("cart-shopping-fast-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(0, "All Orders");

    // ═══════════════════════════════════════════════════════════════════════
    // TAB 2 — ORDER HISTORY (per customer)
    // ═══════════════════════════════════════════════════════════════════════
    auto* historyWidget = new QWidget;
    auto* historyLayout = new QVBoxLayout(historyWidget);
    historyLayout->setSpacing(0);
    historyLayout->setContentsMargins(0, 0, 0, 0);

    auto* historyToolbar = new QFrame;
    historyToolbar->setObjectName("pageToolbar");
    auto* histTbLayout = new QVBoxLayout(historyToolbar);
    histTbLayout->setContentsMargins(16, 10, 16, 10);
    histTbLayout->setSpacing(8);

    // Row 1: title
    auto* histRow1 = new QHBoxLayout;
    auto* histTitle = new QLabel("Order History");
    histTitle->setObjectName("pageTitle");
    histRow1->addWidget(histTitle);
    histRow1->addStretch();
    histTbLayout->addLayout(histRow1);

    // Row 2: customer search (name / phone / address) — like Customers page
    auto* histRow2 = new QHBoxLayout;
    histRow2->setSpacing(10);
    m_historySearchEdit = new QLineEdit;
    m_historySearchEdit->setPlaceholderText("Search customer by name, phone, or address...");
    m_historySearchEdit->setObjectName("searchEdit");
    m_historySearchEdit->setMinimumWidth(300);
    
    // Add search icon
    QAction* historySearchAction = new QAction(m_historySearchEdit);
    historySearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                      QColor("#9CA3AF"), 16));
    m_historySearchEdit->addAction(historySearchAction, QLineEdit::LeadingPosition);

    histRow2->addWidget(m_historySearchEdit, 1);
    histTbLayout->addLayout(histRow2);

    historyLayout->addWidget(historyToolbar);

    m_historyTable = new QTableWidget;
    m_historyTable->setObjectName("dataTable");
    m_historyTable->setColumnCount(7);
    m_historyTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Invoice #"),
        LangManager::instance().t("Order #"),
        LangManager::instance().t("Date & Time"),
        LangManager::instance().t("Items"),
        LangManager::instance().t("Subtotal"),
        LangManager::instance().t("Delivery"),
        LangManager::instance().t("Grand Total")
    });
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setAlternatingRowColors(true);
    historyLayout->addWidget(m_historyTable, 1);

    // Summary bar
    auto* summaryBar = new QFrame;
    summaryBar->setObjectName("statusBar");
    auto* summaryLayout = new QHBoxLayout(summaryBar);
    summaryLayout->setContentsMargins(16, 4, 16, 4);
    m_historySummaryLabel = new QLabel;
    m_historySummaryLabel->setObjectName("statusLabel");
    summaryLayout->addWidget(m_historySummaryLabel);
    historyLayout->addWidget(summaryBar);

    tabs->addTab(historyWidget, "");
    tabs->setTabIcon(1, SvgIconHelper::icon(sidebarSvgPath("history-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(1, "Customer History");

    root->addWidget(tabs);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_searchEdit,  &QLineEdit::textChanged, this, &OrdersPage::onSearch);
    connect(m_dateFilterCheck, &QCheckBox::toggled, this, &OrdersPage::onDateFilterToggled);
    connect(m_dateFromEdit, &QDateEdit::dateChanged, this, [this](){ if (m_dateFilterCheck->isChecked()) loadOrders(); });
    connect(m_dateToEdit,   &QDateEdit::dateChanged, this, [this](){ if (m_dateFilterCheck->isChecked()) loadOrders(); });

    connect(m_addOrderBtn,    &QPushButton::clicked, this, &OrdersPage::onAddOrder);
    connect(m_editOrderBtn,   &QPushButton::clicked, this, &OrdersPage::onEditOrder);
    connect(m_deleteOrderBtn, &QPushButton::clicked, this, &OrdersPage::onDeleteOrder);
    connect(m_invoiceBtn,     &QPushButton::clicked, this, &OrdersPage::onPrintInvoice);

    connect(m_ordersTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &OrdersPage::onOrderSelectionChanged);
    connect(m_ordersTableView, &QTableView::doubleClicked,
            this, [this](const QModelIndex&){ onEditOrder(); });

    // Pagination
    connect(m_ordersPagination, &PaginationBar::pageChanged,
            this, [this](int, int){ loadOrders(); });

    // History search: use repository search (name / phone / address) — same as Customers page
    connect(m_historySearchEdit, &QLineEdit::textChanged, this, [this](const QString& txt) {
        const QString kw = txt.trimmed();
        if (kw.isEmpty()) {
            m_historyTable->setRowCount(0);
            m_historySummaryLabel->clear();
            return;
        }
        // Use the same search backend as the Customers page
        QList<Customer> results = m_customerRepo->search(kw);
        if (results.isEmpty()) {
            m_historyTable->setRowCount(0);
            m_historySummaryLabel->setText(
                QString("No customer found matching \"%1\"").arg(kw));
            return;
        }
        // If exactly one result, load their history immediately
        // If multiple, load the first and show a count hint
        const Customer& first = results.first();
        loadHistory(first.id());
        if (results.size() > 1) {
            m_historySummaryLabel->setText(
                QString("%1 customers match \"%2\" — showing history for: %3")
                    .arg(results.size()).arg(kw).arg(first.name()));
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Data loading
// ─────────────────────────────────────────────────────────────────────────────
void OrdersPage::refresh() {
    loadCustomers();
    loadProducts();
    m_drivers = m_driverRepo->getActive();
    // Task 2: build regionId → deliveryFee map
    m_regionFeeMap.clear();
    for (const auto& r : m_regionRepo->getAll())
        m_regionFeeMap[r.id] = r.deliveryFee;
    loadOrders();

    // Clear history search on refresh — user can re-search
    if (m_historySearchEdit) m_historySearchEdit->clear();
    m_historyTable->setRowCount(0);
    m_historySummaryLabel->clear();
}

void OrdersPage::loadCustomers() {
    m_customers = m_customerRepo->getAll();
    m_customerMap.clear();
    for (const auto& c : m_customers)
        m_customerMap[c.id()] = c.name();
    m_ordersModel->setCustomerMap(m_customerMap);
}

void OrdersPage::loadProducts() {
    m_products = m_productRepo->getAll();
}

void OrdersPage::loadOrders() {
    if (!m_ordersPagination || !m_ordersModel || !m_statusLabel) return;

    const QString keyword = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();
    bool hasSearch = !keyword.isEmpty();
    bool hasDateFilter = (m_dateFilterCheck && m_dateFilterCheck->isChecked());

    int total = 0;
    QList<Order> page;

    if (hasDateFilter) {
        // Date filter: load all orders in range (typically small result set),
        // then paginate client-side over that filtered set.
        // EXCLUDE POS orders (registerSessionId > 0)
        QDateTime from(m_dateFromEdit->date(), QTime(0, 0, 0));
        QDateTime to(  m_dateToEdit->date(),   QTime(23, 59, 59));
        QList<Order> allOrders = m_orderRepo->getOrdersDateRange(from, to);
        QList<Order> orders;
        for (auto& o : allOrders) {
            if (o.registerSessionId() == 0) { // Only delivery orders
                if (o.customerName().isEmpty())
                    o.setCustomerName(m_customerMap.value(o.customerId()));
                orders.append(o);
            }
        }

        total = orders.size();
        m_ordersPagination->setTotalItems(total);
        page = orders.mid(m_ordersPagination->offset(), m_ordersPagination->pageSize());
    } else if (hasSearch) {
        // SEARCH: load ALL orders (search must not be affected by pagination),
        // filter by name/invoice, then paginate client-side over the matches.
        // EXCLUDE POS orders (registerSessionId > 0)
        QList<Order> allOrders = m_orderRepo->getAll();
        QList<Order> orders;
        for (auto& o : allOrders) {
            // Show delivery orders (Pending, Out for Delivery, Cancelled)
            // Exclude instant POS orders (status = "Delivered")
            if (o.status() != "Delivered") {
                if (o.customerName().isEmpty())
                    o.setCustomerName(m_customerMap.value(o.customerId()));
                orders.append(o);
            }
        }

        QList<Order> filtered;
        for (const auto& o : orders) {
            bool nameMatch = o.customerName().toLower().contains(keyword);
            // Also allow searching by invoice number (e.g. "INV-0042" or just "42")
            int invNo = (o.invoiceNumber() > 0) ? o.invoiceNumber() : o.id();
            bool invMatch = QString::number(invNo).contains(keyword)
                         || QString("INV-%1").arg(invNo, 4, 10, QChar('0')).toLower().contains(keyword);
            if (nameMatch || invMatch) filtered.append(o);
        }

        total = filtered.size();
        m_ordersPagination->setTotalItems(total);
        page = filtered.mid(m_ordersPagination->offset(), m_ordersPagination->pageSize());
    } else {
        // TRUE server-side pagination: ask the database for exactly the
        // page currently on screen instead of pulling a fixed 200-row
        // chunk and slicing it client-side.
        // Show delivery orders (status != "Delivered") - includes orders from POS marked as delivery
        QList<Order> allPage = m_orderRepo->getPage(m_ordersPagination->offset(), m_ordersPagination->pageSize() * 2); // Get more to compensate for filtered
        QList<Order> deliveryOrders;
        
        for (auto& o : allPage) {
            // Show delivery orders (Pending, Out for Delivery, Cancelled)
            if (o.status() != "Delivered") {
                if (o.customerName().isEmpty())
                    o.setCustomerName(m_customerMap.value(o.customerId()));
                deliveryOrders.append(o);
            }
        }
        
        // Count only delivery orders for pagination
        int allTotal = m_orderRepo->getTotalCount();
        QList<Order> allOrdersList = m_orderRepo->getAll();
        int deliveryTotal = 0;
        for (const auto& o : allOrdersList) {
            if (o.status() != "Delivered") deliveryTotal++;
        }
        
        total = deliveryTotal;
        m_ordersPagination->setTotalItems(total);
        page = deliveryOrders.mid(0, qMin(deliveryOrders.size(), m_ordersPagination->pageSize()));
    }

    Logger::instance().info(QString("loadOrders: total=%1 off=%2 size=%3 page=%4")
        .arg(total).arg(m_ordersPagination->offset()).arg(m_ordersPagination->pageSize()).arg(page.size()));

    m_ordersModel->setOrders(page);
    m_statusLabel->setText(QString("%1 order(s) total").arg(total));

    m_editOrderBtn->setEnabled(false);
    m_deleteOrderBtn->setEnabled(false);
    m_invoiceBtn->setEnabled(false);
}

void OrdersPage::loadHistory(int customerId) {
    if (customerId <= 0) {
        m_historyTable->setRowCount(0);
        m_historySummaryLabel->clear();
        return;
    }

    QList<Order> orders = m_orderRepo->getOrdersByCustomerId(customerId);
    for (auto& o : orders)
        if (o.customerName().isEmpty())
            o.setCustomerName(m_customerMap.value(o.customerId()));

    populateHistoryTable(orders);

    // Summary
    double total = 0.0;
    for (const auto& o : orders) total += o.grandTotal();
    const QString sym = ConfigManager::instance().currencySymbol();
    m_historySummaryLabel->setText(
        QString("%1 order(s)  ·  Total spent: %2 %3")
            .arg(orders.size())
            .arg(QString::number(total, 'f', 2), sym));
}

void OrdersPage::populateHistoryTable(const QList<Order>& orders) {
    const QString sym = ConfigManager::instance().currencySymbol();
    m_historyTable->setRowCount(0);

    for (const auto& o : orders) {
        int row = m_historyTable->rowCount();
        m_historyTable->insertRow(row);

        auto cell = [&](const QString& text, Qt::Alignment align = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(text);
            it->setTextAlignment(align);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        // Invoice # column — use InvoiceNumber if set, otherwise fall back to DB Id
        int invNo = (o.invoiceNumber() > 0) ? o.invoiceNumber() : o.id();
        auto* invItem = cell(QString("INV-%1").arg(invNo, 4, 10, QChar('0')));
        invItem->setForeground(QBrush(QColor("#38BDF8")));
        invItem->setFont([&]{ QFont f; f.setBold(true); return f; }());
        m_historyTable->setItem(row, 0, invItem);

        // Order # (DB primary key)
        m_historyTable->setItem(row, 1, cell(QString::number(o.id())));
        m_historyTable->setItem(row, 2, cell(o.dateTime().toString("yyyy-MM-dd  hh:mm")));
        m_historyTable->setItem(row, 3, cell(QString::number(o.items().size())));
        m_historyTable->setItem(row, 4, cell(
            QString("%1 %2").arg(QString::number(o.subtotal(),    'f', 2), sym)));
        m_historyTable->setItem(row, 5, cell(
            QString("%1 %2").arg(QString::number(o.deliveryFee(), 'f', 2), sym)));

        auto* grandItem = cell(
            QString("%1 %2").arg(QString::number(o.grandTotal(), 'f', 2), sym));
        grandItem->setForeground(QBrush(QColor("#4ADE80")));
        m_historyTable->setItem(row, 6, grandItem);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — All Orders tab
// ─────────────────────────────────────────────────────────────────────────────
void OrdersPage::onSearch() {
    m_ordersPagination->resetToFirst();
    loadOrders();
}

void OrdersPage::onDateFilterToggled(bool checked) {
    m_dateFromEdit->setEnabled(checked);
    m_dateToEdit->setEnabled(checked);
    m_ordersPagination->resetToFirst();
    loadOrders();
}

void OrdersPage::onAddOrder() {
    if (!SessionManager::instance().currentRole().canAddOrder) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to add orders.");
        return;
    }
    QStringList statuses = m_orderRepo
        ? [this]() {
            // Load from OrderStatuses table
            QStringList sl = Order::defaultStatuses();
            return sl;
        }()
        : Order::defaultStatuses();

    OrderDialog dlg(m_customers, m_products, statuses, m_drivers, m_regionFeeMap, m_couponRepo, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Order order = dlg.getOrder();
    if (!m_orderService->createOrder(order)) {
        QMessageBox::critical(this, "Error", "Failed to save the order.");
        return;
    }
    
    // Phase 1 Task 15: Create stock movements for sales
    createStockMovementsForOrder(order);
    
    // Task 12: increment coupon usage if a coupon was applied
    if (dlg.appliedCouponId() > 0 && m_couponRepo)
        m_couponRepo->incrementUsage(dlg.appliedCouponId());
    refresh();
    Logger::instance().info(QString("Order created ID=%1").arg(order.id()));
    AuditService::instance().logCreate("Order", order.id(),
        QString("Customer: %1, Total: %2").arg(order.customerName()).arg(order.grandTotal()));
}

void OrdersPage::onEditOrder() {
    if (!SessionManager::instance().currentRole().canEditOrder) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to edit orders.");
        return;
    }
    QModelIndexList sel = m_ordersTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    auto* proxy = qobject_cast<ArabicSortProxy*>(m_ordersTableView->model());
    int srcRow = proxy ? proxy->mapToSource(sel.first()).row() : sel.first().row();
    Order order = m_ordersModel->orderAt(srcRow);
    QStringList statuses = Order::defaultStatuses();

    OrderDialog dlg(m_customers, m_products, statuses, m_drivers, m_regionFeeMap, m_couponRepo, this);
    dlg.setOrder(order);
    if (dlg.exec() != QDialog::Accepted) return;

    Order updated = dlg.getOrder();
    if (!m_orderService->updateOrder(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update the order.");
        return;
    }
    
    // Update the model row directly instead of full refresh
    // This ensures the updated order is visible even if it's beyond page 1
    m_ordersModel->updateOrder(srcRow, updated);
    
    Logger::instance().info(QString("Order updated ID=%1").arg(updated.id()));

    // Full snapshot helper — includes all fields + items
    auto makeOrderSnap = [](const Order& o) -> QJsonObject {
        QJsonObject s;
        s["id"]                = o.id();
        s["customerId"]        = o.customerId();
        s["customerName"]      = o.customerName();
        s["status"]            = o.status();
        s["cancelReason"]      = o.cancelReason();
        s["subtotal"]          = o.subtotal();
        s["grandTotal"]        = o.grandTotal();
        s["deliveryFee"]       = o.deliveryFee();
        s["discountAmount"]    = o.discountAmount();
        s["discountReason"]    = o.discountReason();
        s["paymentMethod"]     = o.paymentMethod();
        s["paymentOtherDetail"]= o.paymentOtherDetail();
        s["driverId"]          = o.driverId();
        s["driverName"]        = o.driverName();
        s["dateTime"]          = o.dateTime().toString(Qt::ISODate);
        // Items array
        QJsonArray items;
        for (const auto& it : o.items()) {
            QJsonObject item;
            item["productId"]  = it.productId;
            item["productName"]= it.productName;
            item["qty"]        = it.quantity;
            item["unitPrice"]  = it.unitPrice;
            items.append(item);
        }
        s["items"] = items;
        return s;
    };
    // Snapshot is of the BEFORE state (order = loaded before dialog)
    QString details = QJsonDocument(QJsonObject{{"before", makeOrderSnap(order)}})
                          .toJson(QJsonDocument::Compact);
    AuditService::instance().logUpdate("Order", updated.id(), details);
}

void OrdersPage::onDeleteOrder() {
    if (!SessionManager::instance().currentRole().canDeleteOrders) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to delete orders.");
        return;
    }
    QModelIndexList sel = m_ordersTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    auto* proxy2 = qobject_cast<ArabicSortProxy*>(m_ordersTableView->model());
    int row = proxy2 ? proxy2->mapToSource(sel.first()).row() : sel.first().row();
    if (row < 0 || row >= m_ordersModel->rowCount()) return;

    // Copy the order by value before any refresh
    Order order = m_ordersModel->orderAt(row);
    int orderId = order.id();
    if (orderId <= 0) return;

    QString custName = order.customerName().isEmpty()
        ? m_customerMap.value(order.customerId(), "unknown")
        : order.customerName();

    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete order #%1 for %2?").arg(orderId).arg(custName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // Clear selection before delete to avoid stale index crash
    m_ordersTableView->selectionModel()->clearSelection();

    // Reload full order from DB (with items) before deleting for complete snapshot
    Order fullOrder = m_orderRepo->getById(orderId);
    if (fullOrder.customerName().isEmpty())
        fullOrder.setCustomerName(m_customerMap.value(fullOrder.customerId(), custName));

    int archiveId = m_orderService->deleteOrder(orderId);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error", "Failed to delete the order.");
        return;
    }
    refresh();
    Logger::instance().info(QString("Order deleted ID=%1").arg(orderId));

    // Full JSON snapshot for undo restore (includes all fields + items)
    QJsonObject snap;
    snap["id"]                = fullOrder.id();
    snap["customerId"]        = fullOrder.customerId();
    snap["customerName"]      = fullOrder.customerName();
    snap["status"]            = fullOrder.status();
    snap["cancelReason"]      = fullOrder.cancelReason();
    snap["subtotal"]          = fullOrder.subtotal();
    snap["grandTotal"]        = fullOrder.grandTotal();
    snap["deliveryFee"]       = fullOrder.deliveryFee();
    snap["discountAmount"]    = fullOrder.discountAmount();
    snap["discountReason"]    = fullOrder.discountReason();
    snap["paymentMethod"]     = fullOrder.paymentMethod();
    snap["paymentOtherDetail"]= fullOrder.paymentOtherDetail();
    snap["driverId"]          = fullOrder.driverId();
    snap["driverName"]        = fullOrder.driverName();
    snap["dateTime"]          = fullOrder.dateTime().toString(Qt::ISODate);
    QJsonArray itemsArr;
    for (const auto& it : fullOrder.items()) {
        QJsonObject item;
        item["productId"]  = it.productId;
        item["productName"]= it.productName;
        item["qty"]        = it.quantity;
        item["unitPrice"]  = it.unitPrice;
        itemsArr.append(item);
    }
    snap["items"] = itemsArr;
    QString details = QJsonDocument(QJsonObject{
        {"before", snap},
        {"archiveId", archiveId}
    }).toJson(QJsonDocument::Compact);
    AuditService::instance().logDelete("Order", orderId, details);
}

void OrdersPage::onOrderSelectionChanged() {
    bool has = !m_ordersTableView->selectionModel()->selectedRows().isEmpty();
    m_editOrderBtn->setEnabled(has);
    m_deleteOrderBtn->setEnabled(has);
    m_invoiceBtn->setEnabled(has);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — History tab
// ─────────────────────────────────────────────────────────────────────────────
void OrdersPage::onCustomerFilterChanged(int /*index*/) {
    // No longer used — history search is driven by m_historySearchEdit
}


void OrdersPage::retranslateUi() { retranslateWidget(this); }

void OrdersPage::openOrderForEdit(int orderId) {
    // Reload orders to include the newly created one
    refresh();

    // Find the order in the model and select it
    auto* proxy3 = qobject_cast<ArabicSortProxy*>(m_ordersTableView->model());
    for (int i = 0; i < m_ordersModel->rowCount(); ++i) {
        const Order& o = m_ordersModel->orderAt(i);
        if (o.id() == orderId) {
            QModelIndex srcIdx = m_ordersModel->index(i, 0);
            QModelIndex idx = proxy3 ? proxy3->mapFromSource(srcIdx) : srcIdx;
            m_ordersTableView->selectionModel()->select(idx,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_ordersTableView->scrollTo(idx);
            break;
        }
    }

    // Open the edit dialog for this order
    Order order = m_orderRepo->getById(orderId);
    if (order.id() == 0) return;

    // Inject customer name
    if (order.customerName().isEmpty())
        order.setCustomerName(m_customerMap.value(order.customerId()));

    QStringList statuses = Order::defaultStatuses();
    OrderDialog dlg(m_customers, m_products, statuses, m_drivers, m_regionFeeMap, m_couponRepo, this);
    dlg.setOrder(order);
    if (dlg.exec() != QDialog::Accepted) return;

    Order updated = dlg.getOrder();
    if (!m_orderService->updateOrder(updated))
        QMessageBox::critical(this, "Error", "Failed to update the order.");
    else {
        // Reload the entire page to see the updated order
        refresh();
    }
}

void OrdersPage::onPrintInvoice() {
    QModelIndexList sel = m_ordersTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    const Order& order = m_ordersModel->orderAt(sel.first().row());

    // Get customer
    Customer customer = m_customerRepo->getById(order.customerId());

    // Choose output path
    QString defaultName = QString("invoice_INV-%1.pdf")
        .arg(ConfigManager::instance().nextInvoiceNumber(), 6, 10, QChar('0'));
    QString path = QFileDialog::getSaveFileName(
        this, "Save Invoice PDF", defaultName, "PDF Files (*.pdf)");
    if (path.isEmpty()) return;

    QString result = InvoiceGenerator::generatePdf(order, customer, path);
    if (!result.isEmpty()) {
        QMessageBox::information(this, "Invoice",
            QString("Invoice saved:\n%1").arg(result));
        Logger::instance().info("Invoice generated: " + result);
    } else {
        QMessageBox::critical(this, "Error", "Failed to generate invoice.");
    }
}


// Phase 1 Task 15: Create stock movements when order is created/updated
void OrdersPage::createStockMovementsForOrder(const Order& order) {
    if (!m_stockMovementRepo || !m_productRepo) return;
    
    int currentUserId = SessionManager::instance().currentUser().id();
    QDateTime now = QDateTime::currentDateTime();
    
    for (const auto& item : order.items()) {
        // Get current product to update stock
        Product product = m_productRepo->getById(item.productId);
        if (product.id() == 0) {
            Logger::instance().warn(QString("Product not found ID=%1 for order stock movement").arg(item.productId));
            continue;
        }
        
        // Create stock movement (movementType="Sale")
        StockMovement movement;
        movement.productId = item.productId;
        movement.movementType = "Sale";
        movement.quantity = static_cast<int>(item.quantity);
        movement.dateTime = now;
        movement.referenceType = "Order";
        movement.referenceId = order.id();
        movement.notes = QString("Sale - Order #%1 to %2").arg(order.id()).arg(order.customerName());
        movement.userId = currentUserId;
        
        if (!m_stockMovementRepo->save(movement)) {
            Logger::instance().error(QString("Failed to create stock movement for product %1 in order %2")
                .arg(item.productId).arg(order.id()));
            continue;
        }
        
        // Decrease product stock
        int newStock = product.stockQty() - static_cast<int>(item.quantity);
        if (newStock < 0) newStock = 0;
        product.setStockQty(newStock);
        
        if (!m_productRepo->save(product)) {
            Logger::instance().error(QString("Failed to update stock for product %1").arg(product.id()));
        }
    }
}
