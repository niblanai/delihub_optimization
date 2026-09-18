#include "dashboard_page.h"
#include "ui/dashboard/charts_widget.h"
#include "ui/tr_helper.h"
#include "ui/svg_icon_helper.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "services/notification_service.h"
#include "services/theme_manager.h"
#include "services/branch_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QColor>
#include <QFile>
#include <QCoreApplication>
#include <algorithm>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

// ── Section label builder — uses objectName for theme-aware coloring ─────────
static QLabel* makeSectionLabel(const QString& text, QWidget* parent = nullptr) {
    auto* lbl = new QLabel(text.toUpper(), parent);
    lbl->setObjectName("dashSectionLabel");
    // Base style: only layout/font properties — color comes from ThemeManager QSS
    lbl->setStyleSheet(
        "QLabel#dashSectionLabel {"
        "  font-size:11px; font-weight:400; letter-spacing:0.8px;"
        "  background:transparent; margin-bottom:2px;"
        "}");
    return lbl;
}

// ── KPI Card builder — theme-aware colors throughout ─────────────────────────
//
// card_style:
//   "normal"  – neutral surface, textPrimary value
//   "green"   – neutral surface, semantic success value
//   "amber"   – neutral surface, semantic warning value
//   "warning" – danger-tinted surface + border, red/coral value
//
static QFrame* makeKpiCard(const QString& icon, const QString& title,
                            QLabel*& valueLabel,
                            QWidget* parent = nullptr,
                            const QString& cardStyle = "normal") {
    const DesignTokens& tk = ThemeManager::instance().tokens();

    auto* card = new QFrame(parent);
    card->setObjectName("kpiCard");

    // Warning cards: danger-tinted bg from token (works in all themes)
    if (cardStyle == "warning") {
        card->setStyleSheet(QString(
            "QFrame#kpiCard {"
            "  background:%1;"
            "  border:0.5px solid %2;"
            "  border-radius:10px;"
            "  padding:0px;"
            "}").arg(tk.dangerBg, tk.danger));
    }

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);

    auto* labelRow = new QHBoxLayout;
    labelRow->setSpacing(5);
    if (!icon.isEmpty()) {
        auto* iconLbl = new QLabel(card);
        // Check if it's a file path (starts with :/ or contains .svg)
        if (icon.startsWith(":/") || icon.contains(".svg")) {
            QString iconPath = icon;
            // If it's a resource path, convert to file path
            if (iconPath.startsWith(":/sidebar/")) {
                QString fileName = iconPath.mid(10); // Remove ":/sidebar/"
                iconPath = sidebarSvgPath(fileName);
            }
            
            if (!iconPath.isEmpty()) {
                QIcon svgIcon = SvgIconHelper::icon(iconPath, QColor(tk.textSecondary), 18);
                QPixmap pixmap = svgIcon.pixmap(18, 18);
                iconLbl->setPixmap(pixmap);
            } else {
                // Fallback to text if path not found
                iconLbl->setText(icon);
            }
        } else {
            // It's emoji text
            iconLbl->setText(icon);
        }
        iconLbl->setStyleSheet("background:transparent;");
        iconLbl->setFixedSize(18, 18);
        labelRow->addWidget(iconLbl);
    }
    auto* titleLbl = new QLabel(title, card);
    // Use objectName so ThemeManager QSS controls the color per-theme
    // Warning cards keep inline danger color; others use dashKpiTitle (= textSecondary)
    if (cardStyle == "warning") {
        titleLbl->setStyleSheet(QString(
            "font-size:11px; font-weight:400; color:%1; background:transparent;")
            .arg(tk.danger));
    } else {
        titleLbl->setObjectName("dashKpiTitle");
        titleLbl->setStyleSheet("background:transparent;");
    }
    labelRow->addWidget(titleLbl, 1);
    layout->addLayout(labelRow);

    // Value label — the dominant element
    // Use objectName-based styling so ThemeManager QSS updates it on theme switch
    valueLabel = new QLabel("—", card);
    if      (cardStyle == "green")   valueLabel->setObjectName("dashKpiValueGreen");
    else if (cardStyle == "amber")   valueLabel->setObjectName("dashKpiValueAmber");
    else if (cardStyle == "warning") valueLabel->setObjectName("dashKpiValueDanger");
    else                             valueLabel->setObjectName("dashKpiValue");

    valueLabel->setStyleSheet("background:transparent; margin-top:4px;");
    valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(valueLabel);

    return card;
}

// ── Section wrapper: label + grid row ────────────────────────────────────────
static QWidget* makeSectionBlock(const QString& sectionName,
                                  const QList<QFrame*>& cards,
                                  QWidget* parent = nullptr) {
    auto* block  = new QWidget(parent);
    auto* blay   = new QVBoxLayout(block);
    blay->setContentsMargins(0, 0, 0, 0);
    blay->setSpacing(6);

    blay->addWidget(makeSectionLabel(sectionName, block));

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    for (auto* c : cards) row->addWidget(c);
    blay->addLayout(row);

    return block;
}

// ─────────────────────────────────────────────────────────────────────────────
DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_orderRepo    = new SQLiteOrderRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_productRepo  = new SQLiteProductRepository;
        m_schedRepo    = new SQLiteScheduledOrderRepository;
        m_purchaseRepo = new SQLitePurchaseInvoiceRepository;
    } else {
        m_orderRepo    = new AccessOrderRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_productRepo  = new AccessProductRepository;
        m_schedRepo    = new AccessScheduledOrderRepository;
        m_purchaseRepo = new AccessPurchaseInvoiceRepository;
    }
    m_orderService  = new OrderService(m_orderRepo, m_customerRepo, m_productRepo, nullptr);
    // Task 8: scheduling service for due-today check
    m_schedService  = new SchedulingService(m_schedRepo, m_customerRepo);

    setupUi();

    // When theme changes, refresh data so KPI values stay current.
    // The static section labels and card titles will pick up their new colors
    // from ThemeManager QSS (via objectName rules added to cardsStyle()).
    // Charts update via ChartsWidget's own themeChanged handler.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeType) { refresh(); });

    // Task 8: subscribe to NotificationService so the banner appears whenever
    // a ScheduledOrderDueToday event fires (even from the Scheduled Orders page timer).
    m_notifHandle = NotificationService::instance().subscribe(
        [this](const Notification& n) {
            if (n.event == NotificationEvent::ScheduledOrderDueToday) {
                QList<ScheduledOrder> due = m_schedService->dueTodayOrders();
                if (due.isEmpty()) {
                    // checkDueToday() may not have run yet — show at least a count of 1
                    ScheduledOrder placeholder;
                    placeholder.customerName = n.message;
                    due.append(placeholder);
                }
                showScheduledOrderBanner(due);
            }
        });
    // Data loaded on first navigateTo(0), not in constructor
}

// Task 8: unsubscribe when destroyed to prevent dangling callback
DashboardPage::~DashboardPage() {
    NotificationService::instance().unsubscribe(m_notifHandle);
}

void DashboardPage::setupUi() {
    // Outer scroll area so dashboard works at any window size
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(0);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Task 8: Due-today banner — same design as ScheduledOrdersPage banner
    m_dueTodayBanner = new QFrame;
    m_dueTodayBanner->setObjectName("dashDueBanner");
    m_dueTodayBanner->setStyleSheet(
        "QFrame#dashDueBanner { background-color:#065F46; border-bottom:2px solid #34D399; }"
        "QFrame#dashDueBanner * { background:transparent; color:#D1FAE5; }");
    m_dueTodayBanner->setFixedHeight(44);
    m_dueTodayBanner->setVisible(false);
    auto* bannerLayout = new QHBoxLayout(m_dueTodayBanner);
    bannerLayout->setContentsMargins(16, 0, 16, 0);
    bannerLayout->setSpacing(8);
    auto* bellLbl = new QLabel("🔔");
    bellLbl->setFixedWidth(24);
    m_dueTodayLabel = new QLabel;
    m_dueTodayLabel->setStyleSheet("font-size:12px;");
    bannerLayout->addWidget(bellLbl);
    bannerLayout->addWidget(m_dueTodayLabel, 1);
    outerLayout->addWidget(m_dueTodayBanner);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* contents = new QWidget;
    auto* root = new QVBoxLayout(contents);
    root->setSpacing(18);
    root->setContentsMargins(20, 20, 20, 20);
    scrollArea->setWidget(contents);
    outerLayout->addWidget(scrollArea);

    // ── Title bar with connection status ─────────────────────────────────────
    auto* titleRow = new QHBoxLayout;
    auto* pageTitle = new QLabel("📊  Dashboard");
    pageTitle->setObjectName("pageTitle");
    pageTitle->setVisible(false);
    titleRow->addWidget(pageTitle);
    
    // Connection status indicator (for cloud branches)
    BranchInfo activeBranch = BranchManager::instance().activeBranch();
    bool isCloud = activeBranch.isCloud 
                   || activeBranch.dbPath.startsWith("postgresql://")
                   || activeBranch.dbPath.startsWith("postgres://");
    
    if (isCloud) {
        m_connectionStatusIcon = new QLabel("🟢");
        m_connectionStatusIcon->setFixedSize(16, 16);
        m_connectionStatusLabel = new QLabel("Online");
        m_connectionStatusLabel->setStyleSheet("font-size: 12px; color: #10B981; font-weight: bold;");
        titleRow->addWidget(m_connectionStatusIcon);
        titleRow->addWidget(m_connectionStatusLabel);
        
        // Start status checking timer
        m_statusCheckTimer = new QTimer(this);
        connect(m_statusCheckTimer, &QTimer::timeout, this, &DashboardPage::checkConnectionStatus);
        m_statusCheckTimer->start(10000);  // Check every 10 seconds
    }
    
    titleRow->addStretch();
    auto* refreshBtn = new QPushButton("Refresh");
    refreshBtn->setObjectName("secondaryBtn");
    refreshBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("refresh-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    refreshBtn->setIconSize(QSize(18, 18));
    connect(refreshBtn, &QPushButton::clicked, this, &DashboardPage::refresh);
    titleRow->addWidget(refreshBtn);
    root->addLayout(titleRow);

    // ── Section 1: Orders (4 cards) ───────────────────────────────────────────
    {
        auto* c1 = makeKpiCard(":/sidebar/cart-shopping-fast-svgrepo-com.svg", "Today's Orders",    m_kpiOrderCount, contents, "normal");
        auto* c2 = makeKpiCard(":/sidebar/contract-pending-line-svgrepo-com.svg", "Pending Orders",    m_kpiPending,    contents, "amber");
        auto* c3 = makeKpiCard(":/sidebar/close-svgrepo-com.svg", "Cancelled Today",   m_kpiCancelled,  contents, "normal");
        auto* c4 = makeKpiCard("🔁", "Scheduled Due Today",m_kpiDueToday,  contents, "normal");
        root->addWidget(makeSectionBlock("Orders", {c1, c2, c3, c4}, contents));
    }

    // ── Section 2: Revenue (4 cards) ──────────────────────────────────────────
    {
        auto* c1 = makeKpiCard("💰", "Today's Revenue",    m_kpiSalesTotal,   contents, "green");
        auto* c2 = makeKpiCard("📊", "Today's Profit",     m_kpiTodayProfit,  contents, "success");  // NEW
        auto* c3 = makeKpiCard("📅", "This Month Revenue", m_kpiMonthRevenue, contents, "green");
        auto* c4 = makeKpiCard("🚚", "Delivery Fees Today",m_kpiDeliveryFees, contents, "normal");
        root->addWidget(makeSectionBlock("Revenue", {c1, c2, c3, c4}, contents));
    }

    // ── Section 3: Inventory Alerts (3 cards) ────────────────────────────────
    {
        auto* c1 = makeKpiCard("⚠️", "Low Stock Items",  m_kpiLowStock,     contents, "warning");
        auto* c2 = makeKpiCard("⌛", "Expired Products", m_kpiExpiredCount, contents, "warning");
        auto* c3 = makeKpiCard("📦", "Best Product",     m_kpiBestProduct,  contents, "normal");
        root->addWidget(makeSectionBlock("Inventory Alerts", {c1, c2, c3}, contents));
    }

    // ── Section 4: Customers (2 cards) ────────────────────────────────────────
    {
        auto* c1 = makeKpiCard("👥", "Total Customers", m_kpiCustCount,   contents, "normal");
        auto* c2 = makeKpiCard("🏆", "Top Customer",    m_kpiTopCustomer, contents, "normal");
        root->addWidget(makeSectionBlock("Customers", {c1, c2}, contents));
    }

    // ── Section: Customer Status tables ─────────────────────────────────────
    {
        auto makeCustTable = [&](const QString& title, QTableWidget*& tbl,
                                  const QString& accentColor) -> QFrame* {
            const DesignTokens& tk = ThemeManager::instance().tokens();
            auto* card = new QFrame(contents);
            card->setObjectName("kpiCard");
            auto* lay = new QVBoxLayout(card);
            lay->setContentsMargins(14, 12, 14, 12);
            lay->setSpacing(0);

            auto* hdr = new QLabel(title, card);
            hdr->setStyleSheet(
                QString("font-size:11px; font-weight:400; color:%1;"
                        " background:transparent; margin-bottom:8px;").arg(accentColor));
            lay->addWidget(hdr);

            tbl = new QTableWidget(0, 2, card);
            tbl->setObjectName("dataTable");
            tbl->setHorizontalHeaderLabels({"Name", "Phone"});
            tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            tbl->horizontalHeader()->setStyleSheet(
                QString("QHeaderView::section {"
                "  font-size:11px; font-weight:400; color:%1;"
                "  background:transparent; border:none; padding:4px 0;"
                "}").arg(tk.tableHeaderText));
            tbl->verticalHeader()->setVisible(false);
            tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tbl->setAlternatingRowColors(false);
            tbl->setShowGrid(false);
            tbl->setStyleSheet(QString(
                "QTableWidget { border:none; background:transparent; font-size:12px; color:%1; }"
                "QTableWidget::item { padding:6px 0; border-top:0.5px solid %2; }")
                .arg(tk.textPrimary, tk.tableBorder));
            // Row height and scroll container height
            const int rowH    = 30;
            const int hdrH    = 28;
            const int maxRows = 5;
            tbl->setMinimumHeight(hdrH + rowH * 2);   // always show at least 2 rows
            tbl->setMaximumHeight(hdrH + rowH * maxRows + 2);
            tbl->verticalHeader()->setDefaultSectionSize(rowH);
            tbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
            lay->addWidget(tbl);
            return card;
        };

        auto* tableBlock = new QWidget(contents);
        auto* tblLay    = new QVBoxLayout(tableBlock);
        tblLay->setContentsMargins(0, 0, 0, 0);
        tblLay->setSpacing(6);
        tblLay->addWidget(makeSectionLabel("Customer Status", tableBlock));

        auto* tblRow = new QHBoxLayout;
        tblRow->setSpacing(12);
        tblRow->addWidget(makeCustTable("😴  Inactive  (14+ days)",  m_inactiveTable, "#FB923C"), 1);
        tblRow->addWidget(makeCustTable("🤔  Hesitant  (7–14 days)", m_hesitantTable, "#FCD34D"), 1);
        tblLay->addLayout(tblRow);
        root->addWidget(tableBlock);
    }

    // Initialize legacy table pointers (not used in display)
    m_recentOrdersTable    = nullptr;
    m_cancelledOrdersTable = nullptr;
    m_dueTodayTable        = nullptr;

    // ── Section: Analytics (charts) ───────────────────────────────────────────
    {
        auto* analyticsBlock = new QWidget(contents);
        analyticsBlock->setContentsMargins(0, 8, 0, 0);   // 8px extra top gap
        auto* alay = new QVBoxLayout(analyticsBlock);
        alay->setContentsMargins(0, 0, 0, 0);
        alay->setSpacing(6);
        alay->addWidget(makeSectionLabel("Analytics", analyticsBlock));

        m_chartsWidget = new ChartsWidget(analyticsBlock);
        alay->addWidget(m_chartsWidget);
        root->addWidget(analyticsBlock);
    }
}

// ── Refresh ───────────────────────────────────────────────────────────────────
void DashboardPage::refresh() {
    if (!m_chartsWidget || !m_orderRepo) return;
    loadKPIs();

    // Task 8: check for due-today scheduled orders and show/update banner
    QList<ScheduledOrder> dueToday = m_schedService->checkDueToday();
    showScheduledOrderBanner(dueToday);
    // Task 10: populate inactive/hesitant customer tables
    loadCustomerStatusTables();

    // Build chart data from real DB
    QDateTime from30 = QDateTime::currentDateTime().addDays(-30);
    QDateTime now    = QDateTime::currentDateTime();
    QList<Order> orders30 = m_orderRepo->getOrdersDateRange(from30, now);

    QMap<QString, double> salesData;
    QMap<QString, int>    statusData;
    for (const auto& o : orders30) {
        QString day = o.dateTime().toString("yyyy-MM-dd");
        if (o.status() != "Cancelled") salesData[day] += o.grandTotal();
        statusData[o.status().isEmpty() ? "Pending" : o.status()]++;
    }
    // Ensure all dates in range appear (fill zeros)
    for (int d = 29; d >= 0; --d) {
        QString day = QDate::currentDate().addDays(-d).toString("yyyy-MM-dd");
        if (!salesData.contains(day)) salesData[day] = 0.0;
    }

    m_chartsWidget->setSalesData(salesData);
    m_chartsWidget->setStatusData(statusData);

    // Top products by quantity sold (all time, non-cancelled)
    QMap<int,int>    qtySold;
    QMap<int,QString> prodNames;
    for (const auto& p : m_productRepo->getAll())
        prodNames[p.id()] = p.name();
    for (const auto& o : m_orderRepo->getAll())
        if (o.status() != "Cancelled")
            for (const auto& item : o.items())
                qtySold[item.productId] += item.quantity;

    QList<QPair<int,int>> sorted;
    for (auto it = qtySold.begin(); it != qtySold.end(); ++it)
        sorted.append({it.value(), it.key()});
    std::sort(sorted.begin(), sorted.end(), std::greater<QPair<int,int>>());

    QMap<QString,int> topProds;
    for (int i = 0; i < qMin(6, sorted.size()); ++i)
        topProds[prodNames.value(sorted[i].second, "?")] = sorted[i].first;

    m_chartsWidget->setTopProductsData(topProds);
}

// ── KPIs ──────────────────────────────────────────────────────────────────────
void DashboardPage::loadKPIs() {
    const QString sym   = ConfigManager::instance().currencySymbol();
    QDate today         = QDate::currentDate();
    QDateTime dayStart  = QDateTime(today, QTime(0,0,0));
    QDateTime dayEnd    = QDateTime(today, QTime(23,59,59));
    QList<Order> todayOrders = m_orderRepo->getOrdersDateRange(dayStart, dayEnd);

    // Today stats
    int    todayCount     = 0;
    double todayRevenue   = 0.0;
    double todayFees      = 0.0;
    int    pendingCount   = 0;
    int    cancelledToday = 0;
    QMap<int,int> custOrderCount;

    for (const auto& o : todayOrders) {
        ++todayCount;
        if (o.status() == "Cancelled") { ++cancelledToday; continue; }
        if (o.status() == "Pending" || o.status().isEmpty()) ++pendingCount;
        todayRevenue += o.grandTotal();
        todayFees    += o.deliveryFee();
        custOrderCount[o.customerId()]++;
    }
    
    // Calculate today's costs from purchase invoices
    double todayCosts = 0.0;
    QList<PurchaseInvoice> todayPurchases = m_purchaseRepo->getByDateRange(today, today);
    for (const auto& invoice : todayPurchases) {
        if (invoice.isConfirmed()) {  // Only count confirmed invoices, not drafts
            todayCosts += invoice.totalAmount();
        }
    }
    
    // Calculate profit (revenue - costs)
    double todayProfit = todayRevenue - todayCosts;

    m_kpiOrderCount->setText(QString::number(todayCount));
    m_kpiSalesTotal->setText(QString("%1 %2").arg(
        QString::number(todayRevenue, 'f', 2), sym));
    m_kpiTodayProfit->setText(QString("%1 %2").arg(
        QString::number(todayProfit, 'f', 2), sym));
    m_kpiDeliveryFees->setText(QString("%1 %2").arg(
        QString::number(todayFees, 'f', 2), sym));
    m_kpiPending->setText(QString::number(pendingCount));
    m_kpiCancelled->setText(QString::number(cancelledToday));

    // Month revenue
    QDateTime monthStart = QDateTime(QDate(today.year(), today.month(), 1), QTime(0,0,0));
    double monthRev = 0.0;
    for (const auto& o : m_orderRepo->getOrdersDateRange(monthStart, dayEnd))
        if (o.status() != "Cancelled") monthRev += o.grandTotal();
    m_kpiMonthRevenue->setText(QString("%1 %2").arg(
        QString::number(monthRev, 'f', 2), sym));

    // Total customers
    m_kpiCustCount->setText(QString::number(m_customerRepo->getAll().size()));

    // Top customer (by all-time order count)
    // Build map: customerId → {orderCount, customerName from order records}
    QMap<int,int>     allCustOrders;
    QMap<int,QString> custNamesFromOrders;
    for (const auto& o : m_orderRepo->getAll()) {
        if (o.status() == "Cancelled") continue;
        allCustOrders[o.customerId()]++;
        if (!o.customerName().isEmpty())
            custNamesFromOrders[o.customerId()] = o.customerName();
    }
    int topCustId  = 0;
    int topCustCnt = 0;
    for (auto it = allCustOrders.begin(); it != allCustOrders.end(); ++it) {
        if (it.value() > topCustCnt) { topCustCnt = it.value(); topCustId = it.key(); }
    }
    if (topCustId > 0) {
        // Try repo first, fall back to name stored in orders
        Customer top = m_customerRepo->getById(topCustId);
        QString topName = top.name().isEmpty()
            ? custNamesFromOrders.value(topCustId, QString("ID %1").arg(topCustId))
            : top.name();
        m_kpiTopCustomer->setText(
            QString("%1\n(%2 orders)").arg(topName).arg(topCustCnt));
    } else {
        m_kpiTopCustomer->setText("—");
    }

    // Best selling product (all time)
    Product best = m_orderService->getBestSellingProduct();
    m_kpiBestProduct->setText(best.id() > 0 ? best.name() : "—");

    // Low-stock products
    int lowCount = 0;
    for (const auto& p : m_productRepo->getAll())
        if (p.status() == Product::Status::Active && p.isLowStock()) ++lowCount;
    m_kpiLowStock->setText(lowCount > 0
        ? QString::number(lowCount) + " items ⚠️"
        : "All OK ✓");

    // Expired products
    QDate today2 = QDate::currentDate();
    int expiredCount = 0;
    for (const auto& p : m_productRepo->getAll())
        if (p.expiryDate().isValid() && p.expiryDate() < today2) ++expiredCount;
    m_kpiExpiredCount->setText(expiredCount > 0
        ? QString::number(expiredCount) + " expired"
        : "None ✓");

    // Scheduled due today
    int dueCount = 0;
    int todayDow = today.dayOfWeek();
    for (const auto& s : m_schedRepo->getAll())
        if (s.isScheduledFor(todayDow)) ++dueCount;
    m_kpiDueToday->setText(QString::number(dueCount));
}

// ── Recent Orders ─────────────────────────────────────────────────────────────
void DashboardPage::loadRecentOrders() {
    // Show last 10 orders (not just today) so tables are always populated
    QList<Order> allOrders = m_orderRepo->getAll();
    const QString sym = ConfigManager::instance().currencySymbol();

    // Limit to last 10 for display
    QList<Order> orders = allOrders.mid(0, qMin(10, allOrders.size()));

    auto statusColor = [](const QString& s) -> QString {
        if (s == "Delivered")        return "#4ADE80";
        if (s == "Cancelled")        return "#F87171";
        if (s == "Out for Delivery") return "#38BDF8";
        return "#FCD34D";
    };

    m_recentOrdersTable->setRowCount(0);
    m_cancelledOrdersTable->setRowCount(0);
    int cancelCount = 0;

    for (const auto& o : allOrders) {
        QString cname = o.customerName().isEmpty()
            ? QString("ID %1").arg(o.customerId()) : o.customerName();

        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        if (o.status() == "Cancelled" && cancelCount < 10) {
            ++cancelCount;
            int row = m_cancelledOrdersTable->rowCount();
            m_cancelledOrdersTable->insertRow(row);
            m_cancelledOrdersTable->setItem(row, 0, cell(cname, Qt::AlignLeft|Qt::AlignVCenter));
            m_cancelledOrdersTable->setItem(row, 1, cell(o.dateTime().toString("MM/dd hh:mm")));
            auto* valIt = cell(QString("%1 %2").arg(QString::number(o.grandTotal(),'f',2), sym));
            valIt->setForeground(QBrush(QColor("#F87171")));
            m_cancelledOrdersTable->setItem(row, 2, valIt);
            m_cancelledOrdersTable->setItem(row, 3, cell(
                o.cancelReason().isEmpty() ? "—" : o.cancelReason(),
                Qt::AlignLeft|Qt::AlignVCenter));
        }
    }

    // Recent (non-cancelled) — last 10
    for (const auto& o : orders) {
        if (o.status() == "Cancelled") continue;
        int row = m_recentOrdersTable->rowCount();
        if (row >= 10) break;
        m_recentOrdersTable->insertRow(row);

        QString cname = o.customerName().isEmpty()
            ? QString("ID %1").arg(o.customerId()) : o.customerName();

        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        m_recentOrdersTable->setItem(row, 0, cell(cname, Qt::AlignLeft|Qt::AlignVCenter));
        m_recentOrdersTable->setItem(row, 1, cell(o.dateTime().toString("MM/dd hh:mm")));
        m_recentOrdersTable->setItem(row, 2, cell(
            QString("%1 %2").arg(QString::number(o.grandTotal(),'f',2), sym)));
        auto* sIt = cell(o.status().isEmpty() ? "Pending" : o.status());
        sIt->setForeground(QBrush(QColor(statusColor(o.status()))));
        m_recentOrdersTable->setItem(row, 3, sIt);
    }
}

// ── Due Today ─────────────────────────────────────────────────────────────────
void DashboardPage::loadDueToday() {
    int todayDow = QDate::currentDate().dayOfWeek();
    m_dueTodayTable->setRowCount(0);
    const auto& all = m_schedRepo->getAll();
    // Show today's scheduled first, then rest
    QList<ScheduledOrder> todayList, otherList;
    for (const auto& s : all) {
        if (s.isScheduledFor(todayDow)) todayList.append(s);
        else otherList.append(s);
    }
    QList<ScheduledOrder> display = todayList + otherList;
    for (int i = 0; i < qMin(10, display.size()); ++i) {
        const auto& s = display.at(i);
        int row = m_dueTodayTable->rowCount();
        m_dueTodayTable->insertRow(row);
        auto c = [](const QString& t, Qt::Alignment a = Qt::AlignCenter){
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a); it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        QString cname = s.customerName.isEmpty()
            ? QString("ID %1").arg(s.customerId) : s.customerName;
        auto* nameIt = c(cname, Qt::AlignLeft|Qt::AlignVCenter);
        if (s.isScheduledFor(todayDow))
            nameIt->setForeground(QBrush(QColor("#4ADE80")));  // green = today
        m_dueTodayTable->setItem(row, 0, nameIt);
        m_dueTodayTable->setItem(row, 1, c(s.time.toString("hh:mm AP")));
    }
}

// Task 10: populate the Inactive and Hesitant customer tables on the dashboard
void DashboardPage::loadCustomerStatusTables() {
    if (!m_inactiveTable || !m_hesitantTable || !m_customerRepo) return;

    // Re-apply stylesheets with fresh tokens every time — the tables were built
    // once in setupUi() with a snapshot of the startup theme tokens, so inline
    // setStyleSheet() overrides would keep stale colors after a theme switch.
    const DesignTokens& tk = ThemeManager::instance().tokens();
    const QString tblStyle = QString(
        "QTableWidget { border:none; background:transparent; font-size:12px; color:%1; }"
        "QTableWidget::item { padding:6px 0; border-top:0.5px solid %2; }")
        .arg(tk.textPrimary, tk.tableBorder);
    const QString hdrStyle = QString(
        "QHeaderView::section {"
        "  font-size:11px; font-weight:400; color:%1;"
        "  background:transparent; border:none; padding:4px 0;"
        "}").arg(tk.tableHeaderText);
    for (QTableWidget* tbl : {m_inactiveTable, m_hesitantTable}) {
        tbl->setStyleSheet(tblStyle);
        tbl->horizontalHeader()->setStyleSheet(hdrStyle);
    }

    m_inactiveTable->setRowCount(0);
    m_hesitantTable->setRowCount(0);

    const QList<Customer> customers = m_customerRepo->getAll();
    for (const auto& c : customers) {
        QTableWidget* target = nullptr;
        if      (c.status() == Customer::Status::Inactive)  target = m_inactiveTable;
        else if (c.status() == Customer::Status::Hesitant)  target = m_hesitantTable;
        if (!target) continue;

        int row = target->rowCount();
        target->insertRow(row);

        QString phone = c.phones().isEmpty() ? "—" : c.phones().first().number;

        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        target->setItem(row, 0, cell(c.name()));
        target->setItem(row, 1, cell(phone, Qt::AlignCenter));
    }
}

// Task 8: show the due-today banner at the top of the Dashboard
void DashboardPage::showScheduledOrderBanner(const QList<ScheduledOrder>& dueList) {
    if (!m_dueTodayBanner || !m_dueTodayLabel) return;
    if (dueList.isEmpty()) {
        m_dueTodayBanner->setVisible(false);
        return;
    }
    QStringList names;
    for (const auto& s : dueList) {
        // Issue 3: show customer name (not ID), "at" not "@", AM/PM format
        QString name = s.customerName.isEmpty()
            ? QString("Customer %1").arg(s.customerId)
            : s.customerName;
        QString timeStr = s.time.isValid() ? s.time.toString("hh:mm AP") : "";
        names << (timeStr.isEmpty() ? name : QString("%1 at %2").arg(name, timeStr));
    }
    m_dueTodayLabel->setText(
        QString("<b>%1 scheduled order(s) due today:</b>  %2")
            .arg(dueList.size())
            .arg(names.join("   ·   ")));
    m_dueTodayBanner->setVisible(true);
}

void DashboardPage::retranslateUi() { retranslateWidget(this); }

// ── Connection Status Monitoring ──────────────────────────────────────────────
void DashboardPage::checkConnectionStatus() {
    if (!m_connectionStatusLabel || !m_connectionStatusIcon) return;
    
    bool isAlive = DatabaseConnectionManager::instance().isConnectionAlive();
    
    if (isAlive) {
        m_connectionStatusIcon->setText("🟢");
        m_connectionStatusLabel->setText("Online");
        m_connectionStatusLabel->setStyleSheet("font-size: 12px; color: #10B981; font-weight: bold;");
    } else {
        m_connectionStatusIcon->setText("🔴");
        m_connectionStatusLabel->setText("Offline");
        m_connectionStatusLabel->setStyleSheet("font-size: 12px; color: #EF4444; font-weight: bold;");
    }
}

void DashboardPage::updateConnectionStatus() {
    checkConnectionStatus();
}
