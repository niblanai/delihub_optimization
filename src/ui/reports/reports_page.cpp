#include "reports_page.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "ui/tr_helper.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFrame>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QPrinter>
#include <QPainter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTimer>
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

ReportsPage::ReportsPage(QWidget* parent)
    : QWidget(parent)
{
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    if (useSqlite) {
        m_orderRepo    = new SQLiteOrderRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_productRepo  = new SQLiteProductRepository;
        m_missingRepo  = new SQLiteMissingProductRepository;
    } else {
        m_orderRepo    = new AccessOrderRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_productRepo  = new AccessProductRepository;
        m_missingRepo  = new AccessMissingProductRepository;
    }
    setupUi();
}

// ─────────────────────────────────────────────────────────────────────────────
void ReportsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QVBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    tbLayout->setSpacing(8);

    auto* row1 = new QHBoxLayout;
    auto* title = new QLabel("Reports");
    title->setObjectName("pageTitle");
    title->setVisible(false);
    row1->addWidget(title);
    row1->addStretch();
    tbLayout->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    row2->setSpacing(10);

    auto hint = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:#94A3B8;");
        return l;
    };

    m_reportTypeCombo = new QComboBox;
    m_reportTypeCombo->addItem("Sales by Date Range",          0);
    m_reportTypeCombo->addItem("Top Selling Products",         1);
    m_reportTypeCombo->addItem("Top Customers",                2);
    m_reportTypeCombo->addItem("Inactive Customers (30+ days)",3);
    m_reportTypeCombo->addItem("Delivery Fee by Day",          4);
    m_reportTypeCombo->addItem("Missing Products",             5);
    m_reportTypeCombo->addItem("Expired Products",             6);
    m_reportTypeCombo->addItem("Cancelled Orders",             7);
    m_reportTypeCombo->addItem("Top Products per Customer",    8);  // New

    // Customer search row — only visible for "Top Products per Customer" (report 8)
    // Lives in its OWN row so it never crowds row2
    m_customerFilterLabel = new QLabel("Search customer (name, phone, or address):");
    m_customerFilterLabel->setStyleSheet("color:#94A3B8;");
    m_customerFilterEdit  = new QLineEdit;
    m_customerFilterEdit->setPlaceholderText("Type name, phone number, or address…");
    m_customerFilterEdit->setObjectName("searchEdit");
    m_customerFilterEdit->setMinimumWidth(340);
    
    // Add search icon
    QAction* custFilterSearchAction = new QAction(m_customerFilterEdit);
    custFilterSearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                         QColor("#9CA3AF"), 16));
    m_customerFilterEdit->addAction(custFilterSearchAction, QLineEdit::LeadingPosition);
    
    m_customerFilterLabel->setVisible(false);
    m_customerFilterEdit->setVisible(false);

    // Task A: status filter — only visible when "Missing Products" is selected
    m_missingStatusLabel = new QLabel("Status:");
    m_missingStatusLabel->setStyleSheet("color:#94A3B8;");
    m_missingStatusCombo = new QComboBox;
    m_missingStatusCombo->addItem("All",     0);
    m_missingStatusCombo->addItem("Pending", 1);
    m_missingStatusCombo->addItem("Resolved",2);
    m_missingStatusLabel->setVisible(false);
    m_missingStatusCombo->setVisible(false);

    m_dateFromEdit = new QDateEdit(QDate::currentDate().addDays(-30));
    m_dateFromEdit->setCalendarPopup(true);
    m_dateFromEdit->setDisplayFormat("yyyy-MM-dd");

    m_dateToEdit = new QDateEdit(QDate::currentDate());
    m_dateToEdit->setCalendarPopup(true);
    m_dateToEdit->setDisplayFormat("yyyy-MM-dd");

    m_generateBtn   = new QPushButton(LangManager::instance().t("Generate"));
    m_generateBtn->setObjectName("primaryBtn");
    m_generateBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("create-new-page-svgrepo-com.svg"), 
                                                QColor("#FFFFFF"), 18));
    m_generateBtn->setIconSize(QSize(18, 18));
    
    m_exportPdfBtn  = new QPushButton(LangManager::instance().t("PDF"));
    m_exportPdfBtn->setObjectName("secondaryBtn");
    m_exportPdfBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("pdf-svgrepo-com.svg"), 
                                                  QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_exportPdfBtn->setIconSize(QSize(18, 18));
    m_exportPdfBtn->setEnabled(false);
    
    m_exportXlsxBtn = new QPushButton(LangManager::instance().t("Excel"));
    m_exportXlsxBtn->setObjectName("secondaryBtn");
    m_exportXlsxBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("file-excel-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_exportXlsxBtn->setIconSize(QSize(18, 18));
    m_exportXlsxBtn->setEnabled(false);

    row2->addWidget(hint(LangManager::instance().t("Report:")));
    row2->addWidget(m_reportTypeCombo);
    row2->addWidget(m_missingStatusLabel);
    row2->addWidget(m_missingStatusCombo);
    row2->addWidget(hint(LangManager::instance().t("From:")));
    row2->addWidget(m_dateFromEdit);
    row2->addWidget(hint(LangManager::instance().t("To:")));
    row2->addWidget(m_dateToEdit);
    row2->addStretch();
    row2->addWidget(m_generateBtn);
    row2->addWidget(m_exportPdfBtn);
    row2->addWidget(m_exportXlsxBtn);
    tbLayout->addLayout(row2);

    // Row 3: customer search — only shown for report type 8
    auto* row3 = new QHBoxLayout;
    row3->setSpacing(8);
    row3->addWidget(m_customerFilterLabel);
    row3->addWidget(m_customerFilterEdit, 1);
    row3->addStretch();
    tbLayout->addLayout(row3);

    root->addWidget(toolbar);

    // Show/hide the status filter based on selected report type
    connect(m_reportTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        int rtype = m_reportTypeCombo->currentData().toInt();
        bool isMissing  = (rtype == 5);
        bool isPerCust  = (rtype == 8);
        m_missingStatusLabel->setVisible(isMissing);
        m_missingStatusCombo->setVisible(isMissing);
        m_customerFilterLabel->setVisible(isPerCust);
        m_customerFilterEdit->setVisible(isPerCust);
        if (isPerCust) m_customerFilterEdit->setFocus();
    });

    // ── Results table + invoice view (stacked) ────────────────────────────────
    m_resultTable = new QTableWidget;
    m_resultTable->setObjectName("dataTable");
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->verticalHeader()->setVisible(false);

    m_invoiceView = new QTextBrowser;
    m_invoiceView->setObjectName("invoiceView");
    m_invoiceView->setOpenExternalLinks(false);
    // Force white background so the HTML report is readable in all themes
    // (Dark theme's global QSS would otherwise paint the background dark,
    //  making the dark-text HTML invisible)
    m_invoiceView->setStyleSheet(
        "QTextBrowser#invoiceView { background-color:#ffffff; color:#111111; }");

    m_resultStack = new QStackedWidget;
    m_resultStack->addWidget(m_resultTable);   // index 0 — all normal reports
    m_resultStack->addWidget(m_invoiceView);   // index 1 — cancelled orders
    root->addWidget(m_resultStack, 1);

    // ── Summary bar ───────────────────────────────────────────────────────────
    auto* summaryBar = new QFrame;
    summaryBar->setObjectName("statusBar");
    auto* sl = new QHBoxLayout(summaryBar);
    sl->setContentsMargins(16, 4, 16, 4);
    m_summaryLabel = new QLabel("Press Generate to run a report.");
    m_summaryLabel->setObjectName("statusLabel");
    sl->addWidget(m_summaryLabel);
    root->addWidget(summaryBar);

    connect(m_generateBtn,   &QPushButton::clicked, this, &ReportsPage::onGenerateReport);
    connect(m_exportPdfBtn,  &QPushButton::clicked, this, &ReportsPage::onExportPDF);
    connect(m_exportXlsxBtn, &QPushButton::clicked, this, &ReportsPage::onExportExcel);
}

// ─────────────────────────────────────────────────────────────────────────────
void ReportsPage::onGenerateReport() {
    // Refresh lookup maps
    m_customerMap.clear();
    QList<Customer> allCustomers = m_customerRepo->getAll();
    for (const auto& c : allCustomers)
        m_customerMap[c.id()] = c.name();
    m_productMap.clear();
    for (const auto& p : m_productRepo->getAll())
        m_productMap[p.id()] = p.name();

    int type = m_reportTypeCombo->currentData().toInt();
    m_cancelledHtml.clear();  // reset before each generate
    switch (type) {
    case 0: m_resultStack->setCurrentIndex(0); generateSalesReport();               break;
    case 1: m_resultStack->setCurrentIndex(0); generateTopProductsReport();         break;
    case 2: m_resultStack->setCurrentIndex(0); generateTopCustomersReport();        break;
    case 3: m_resultStack->setCurrentIndex(0); generateInactiveCustomers();         break;
    case 4: m_resultStack->setCurrentIndex(0); generateDeliveryFeeReport();         break;
    case 5: m_resultStack->setCurrentIndex(0); generateMissingProductsReport();     break;
    case 6: m_resultStack->setCurrentIndex(0); generateExpiredProductsReport();     break;
    case 7: m_resultStack->setCurrentIndex(1); generateCancelledOrdersReport();     break;
    case 8: m_resultStack->setCurrentIndex(0); generateTopProductsPerCustomer();    break;
    }

    // Bug Fix 11: enable export for table reports (index 0) OR when cancelled-orders
    // HTML view (index 1) has actual content.
    bool hasRows     = m_resultTable->rowCount() > 0;
    bool hasCancHtml = (m_resultStack->currentIndex() == 1) && !m_cancelledHtml.isEmpty();
    m_exportPdfBtn->setEnabled(hasRows || hasCancHtml);
    m_exportXlsxBtn->setEnabled(hasRows || hasCancHtml);
}

// ── helper: create a read-only, center-aligned cell ──────────────────────────
static QTableWidgetItem* cell(const QString& text,
                               Qt::Alignment align = Qt::AlignCenter)
{
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(align);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

// ─────────────────────────────────────────────────────────────────────────────
void ReportsPage::generateSalesReport() {
    QDateTime from(m_dateFromEdit->date(), QTime(0, 0, 0));
    QDateTime to(m_dateToEdit->date(),     QTime(23, 59, 59));
    QList<Order> orders = m_orderRepo->getOrdersDateRange(from, to);
    const QString sym = ConfigManager::instance().currencySymbol();

    m_resultTable->setColumnCount(10);
    m_resultTable->setHorizontalHeaderLabels({
        "#",
        LangManager::instance().t("Customer"),
        LangManager::instance().t("Date"),
        LangManager::instance().t("Status"),
        LangManager::instance().t("Payment"),
        LangManager::instance().t("Items"),
        LangManager::instance().t("Subtotal"),
        LangManager::instance().t("Discount"),
        LangManager::instance().t("Delivery"),
        LangManager::instance().t("Grand Total")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultTable->setRowCount(0);

    double totalRevenue  = 0.0;
    double totalDiscount = 0.0;
    double totalFees     = 0.0;
    double totalLosses   = 0.0;
    int    cancelCount   = 0;

    for (const auto& o : orders) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);

        QString cname = o.customerName().isEmpty()
            ? m_customerMap.value(o.customerId(), QString("ID %1").arg(o.customerId()))
            : o.customerName();

        bool cancelled = (o.status() == "Cancelled");

        auto mkC = [&](const QString& text, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(text);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (cancelled) {
                it->setForeground(QBrush(QColor("#F87171")));
                QFont f = it->font(); f.setStrikeOut(true); it->setFont(f);
            }
            return it;
        };

        // Status color
        auto* sIt = new QTableWidgetItem(o.status().isEmpty() ? "Pending" : o.status());
        sIt->setTextAlignment(Qt::AlignCenter);
        sIt->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if      (cancelled)                    sIt->setForeground(QBrush(QColor("#F87171")));
        else if (o.status()=="Delivered")       sIt->setForeground(QBrush(QColor("#4ADE80")));
        else if (o.status()=="Out for Delivery")sIt->setForeground(QBrush(QColor("#38BDF8")));
        else                                    sIt->setForeground(QBrush(QColor("#FCD34D")));

        // Payment badge color
        auto* pmIt = new QTableWidgetItem(o.paymentMethod().isEmpty() ? "Cash" : o.paymentMethod());
        pmIt->setTextAlignment(Qt::AlignCenter);
        pmIt->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if      (o.paymentMethod() == "Visa")  pmIt->setForeground(QBrush(QColor("#38BDF8")));
        else if (o.paymentMethod() == "Cash")  pmIt->setForeground(QBrush(QColor("#4ADE80")));
        else                                   pmIt->setForeground(QBrush(QColor("#FB923C")));

        QString discStr = o.discountAmount() > 0.001
            ? QString("-%1 %2").arg(QString::number(o.discountAmount(),'f',2), sym)
            : "—";

        m_resultTable->setItem(row, 0, mkC(QString::number(o.id())));
        m_resultTable->setItem(row, 1, mkC(cname, Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 2, mkC(o.dateTime().toString("yyyy-MM-dd hh:mm")));
        m_resultTable->setItem(row, 3, sIt);
        m_resultTable->setItem(row, 4, pmIt);
        m_resultTable->setItem(row, 5, mkC(QString::number(o.items().size())));
        m_resultTable->setItem(row, 6, mkC(QString("%1 %2").arg(
            QString::number(o.subtotal(),'f',2), sym)));
        m_resultTable->setItem(row, 7, mkC(discStr));
        m_resultTable->setItem(row, 8, mkC(QString("%1 %2").arg(
            QString::number(o.deliveryFee(),'f',2), sym)));
        m_resultTable->setItem(row, 9, mkC(QString("%1 %2").arg(
            QString::number(o.grandTotal(),'f',2), sym)));

        if (!cancelled) {
            totalRevenue  += o.grandTotal();
            totalDiscount += o.discountAmount();
            totalFees     += o.deliveryFee();
        } else {
            totalLosses += o.grandTotal();
            ++cancelCount;
        }
    }

    m_summaryLabel->setText(
        QString("%1 orders  ·  Revenue: %2 %3  ·  Discounts: %4 %3  ·  Fees: %5 %3  ·  "
                "Cancelled: %6 (-%7 %3)")
            .arg(orders.size())
            .arg(QString::number(totalRevenue,'f',2), sym)
            .arg(QString::number(totalDiscount,'f',2))
            .arg(QString::number(totalFees,'f',2))
            .arg(cancelCount)
            .arg(QString::number(totalLosses,'f',2)));
}

void ReportsPage::generateTopProductsReport() {
    QList<Order> orders = m_orderRepo->getAll();
    QMap<int, int>    qtySold;
    QMap<int, double> revenue;
    for (const auto& o : orders) {
        if (o.status() == "Cancelled") continue;
        for (const auto& item : o.items()) {
            qtySold[item.productId]  += item.quantity;
            revenue[item.productId] += item.totalPrice();
        }
    }

    // Build barcode map from product repo
    QMap<int, QString> barcodeMap;
    for (const auto& p : m_productRepo->getAll())
        barcodeMap[p.id()] = p.barcode();

    QList<int> ids = qtySold.keys();
    std::sort(ids.begin(), ids.end(), [&](int a, int b){ return qtySold[a] > qtySold[b]; });

    const QString sym = ConfigManager::instance().currencySymbol();
    m_resultTable->setColumnCount(4);
    m_resultTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Product"),
        LangManager::instance().t("Barcode"),
        LangManager::instance().t("Qty Sold"),
        LangManager::instance().t("Revenue")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->setRowCount(0);

    for (int id : ids) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        m_resultTable->setItem(row, 0, cell(
            m_productMap.value(id, QString("P%1").arg(id)), Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, cell(
            barcodeMap.value(id, "—")));
        m_resultTable->setItem(row, 2, cell(QString::number(qtySold[id])));
        m_resultTable->setItem(row, 3, cell(
            QString("%1 %2").arg(QString::number(revenue[id],'f',2), sym)));
    }
    m_summaryLabel->setText(QString("%1 product(s)").arg(ids.size()));
}

void ReportsPage::generateTopCustomersReport() {
    QList<Order> orders = m_orderRepo->getAll();
    QMap<int, int>    orderCount;
    QMap<int, double> totalSpent;
    for (const auto& o : orders) {
        if (o.status() == "Cancelled") continue;
        orderCount[o.customerId()]++;
        totalSpent[o.customerId()] += o.grandTotal();
    }

    // Full customer detail map for phone/address/preferred payment
    QMap<int, Customer> custDetailMap;
    for (const auto& c : m_customerRepo->getAll())
        custDetailMap[c.id()] = c;

    QList<int> ids = totalSpent.keys();
    std::sort(ids.begin(), ids.end(), [&](int a, int b){ return totalSpent[a] > totalSpent[b]; });

    const QString sym = ConfigManager::instance().currencySymbol();
    m_resultTable->setColumnCount(6);
    m_resultTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Customer"),
        LangManager::instance().t("Phone"),
        LangManager::instance().t("Address"),
        LangManager::instance().t("Preferred Payment"),
        LangManager::instance().t("Orders"),
        LangManager::instance().t("Total Spent")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultTable->setRowCount(0);

    for (int id : ids) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        const Customer& c = custDetailMap.value(id);

        QString phone = c.phones().isEmpty()    ? "—" : c.phones().first().number;
        QString addr  = c.addresses().isEmpty() ? "—" : c.addresses().first().text;
        QString pm    = c.preferredPaymentMethod().isEmpty() ? "—" : c.preferredPaymentMethod();
        if (pm == "Other" && !c.preferredPaymentOther().isEmpty()) pm = c.preferredPaymentOther();

        m_resultTable->setItem(row, 0, cell(
            c.name().isEmpty() ? m_customerMap.value(id, QString("ID %1").arg(id)) : c.name(),
            Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, cell(phone));
        m_resultTable->setItem(row, 2, cell(addr, Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 3, cell(pm));
        m_resultTable->setItem(row, 4, cell(QString::number(orderCount[id])));
        m_resultTable->setItem(row, 5, cell(
            QString("%1 %2").arg(QString::number(totalSpent[id],'f',2), sym)));
    }
    m_summaryLabel->setText(QString("%1 customer(s) with orders").arg(ids.size()));
}

void ReportsPage::generateInactiveCustomers() {
    QList<Customer> customers = m_customerRepo->getAll();
    QDateTime now    = QDateTime::currentDateTime();
    QDateTime cutoff = now.addDays(-30);

    // Build last-order map directly from Orders table so the report is accurate
    // even when the customer's LastOrderDate column hasn't been refreshed yet.
    QMap<int, QDateTime> lastOrderMap;
    const QList<Order> allOrders = m_orderRepo->getAll();
    for (const auto& o : allOrders) {
        if (o.status() == "Cancelled") continue;
        if (!o.dateTime().isValid())   continue;
        auto& existing = lastOrderMap[o.customerId()];
        if (!existing.isValid() || o.dateTime() > existing)
            existing = o.dateTime();
    }

    m_resultTable->setColumnCount(5);
    m_resultTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Customer"),
        LangManager::instance().t("Phone"),
        LangManager::instance().t("Address"),
        LangManager::instance().t("Last Order"),
        LangManager::instance().t("Days Inactive")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultTable->setRowCount(0);

    for (const auto& c : customers) {
        // Use the live order map — fall back to stored lastOrderDate if no order found
        QDateTime lastOrder = lastOrderMap.contains(c.id())
            ? lastOrderMap.value(c.id())
            : c.lastOrderDate();

        if (lastOrder.isValid() && lastOrder > cutoff) continue;

        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);

        // days is always computable now — if no order ever, show "Never" + "—"
        int days = lastOrder.isValid()
            ? static_cast<int>(lastOrder.daysTo(now)) : -1;

        QString phone = c.phones().isEmpty()    ? "—" : c.phones().first().number;
        QString addr  = c.addresses().isEmpty() ? "—" : c.addresses().first().text;

        m_resultTable->setItem(row, 0, cell(c.name(), Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, cell(phone));
        m_resultTable->setItem(row, 2, cell(addr, Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row, 3, cell(lastOrder.isValid()
            ? lastOrder.toString("yyyy-MM-dd") : "Never"));
        m_resultTable->setItem(row, 4, cell(days >= 0 ? QString::number(days) : "—"));
    }
    m_summaryLabel->setText(QString("%1 inactive customer(s)").arg(m_resultTable->rowCount()));
}

void ReportsPage::generateDeliveryFeeReport() {
    QDateTime from(m_dateFromEdit->date(), QTime(0, 0, 0));
    QDateTime to(m_dateToEdit->date(),     QTime(23, 59, 59));
    QList<Order> orders = m_orderRepo->getOrdersDateRange(from, to);

    // Group by date — exclude cancelled orders (fees not actually collected)
    QMap<QString, double> byDay;
    for (const auto& o : orders) {
        if (o.status() == "Cancelled") continue;
        byDay[o.dateTime().toString("yyyy-MM-dd")] += o.deliveryFee();
    }

    const QString sym = ConfigManager::instance().currencySymbol();
    m_resultTable->setColumnCount(2);
    m_resultTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Date"),
        LangManager::instance().t("Total Delivery Fees")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->setRowCount(0);

    double grandTotal = 0.0;
    for (auto it = byDay.begin(); it != byDay.end(); ++it) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        m_resultTable->setItem(row, 0, cell(it.key(), Qt::AlignLeft | Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, cell(
            QString("%1 %2").arg(QString::number(it.value(), 'f', 2), sym)));
        grandTotal += it.value();
    }
    m_summaryLabel->setText(
        QString("%1 day(s)  ·  Total fees: %2 %3")
            .arg(byDay.size()).arg(QString::number(grandTotal, 'f', 2), sym));
}

void ReportsPage::generateMissingProductsReport() {
    QList<MissingProduct> allItems = m_missingRepo->getAll();

    // Apply status filter (0=All, 1=Pending, 2=Resolved)
    int statusFilter = m_missingStatusCombo->currentData().toInt();
    QList<MissingProduct> items;
    for (const auto& mp : allItems) {
        if (statusFilter == 0)                            items.append(mp);
        else if (statusFilter == 1 && !mp.purchased)      items.append(mp);
        else if (statusFilter == 2 &&  mp.purchased)      items.append(mp);
    }

    m_resultTable->setColumnCount(6);
    m_resultTable->setHorizontalHeaderLabels(
        {"Product Requested", "Barcode", "Qty Needed", "Current Quantity", "Date Added", "Status"});
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->setRowCount(0);

    int pendingCount  = 0;
    int resolvedCount = 0;

    for (const auto& item : items) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);

        auto mkCell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        // Show currentQty for all records where it's set (> 0), not just auto_low_stock
        QString currentQtyText = (item.currentQty > 0)
            ? QString::number(item.currentQty) : "—";

        m_resultTable->setItem(row, 0, mkCell(item.productName, Qt::AlignLeft | Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, mkCell(item.barcode.isEmpty() ? "—" : item.barcode));
        // Issue 5: show "12 كرتونة" style in report
        QString qtyDisplay = item.unitLabel.isEmpty()
            ? QString::number(item.quantityNeeded)
            : QString("%1 %2").arg(item.quantityNeeded).arg(item.unitLabel);
        m_resultTable->setItem(row, 2, mkCell(qtyDisplay));
        m_resultTable->setItem(row, 3, mkCell(currentQtyText));
        m_resultTable->setItem(row, 4, mkCell(item.dateAdded.toString("yyyy-MM-dd")));

        auto* statusItem = new QTableWidgetItem(item.purchased ? "✓ Resolved" : "⏳ Pending");
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        statusItem->setForeground(QBrush(item.purchased ? QColor("#4ADE80") : QColor("#F87171")));
        m_resultTable->setItem(row, 5, statusItem);

        if (item.purchased) ++resolvedCount; else ++pendingCount;
    }

    QString filterLabel = (statusFilter == 0) ? "All"
                        : (statusFilter == 1) ? "Pending" : "Resolved";
    m_summaryLabel->setText(
        QString("%1 item(s) [Filter: %2]  ·  Pending: %3  ·  Resolved: %4")
            .arg(items.size()).arg(filterLabel).arg(pendingCount).arg(resolvedCount));
}

void ReportsPage::generateExpiredProductsReport() {
    QList<Product> all = m_productRepo->getAll();
    QDate today = QDate::currentDate();

    m_resultTable->setColumnCount(5);
    m_resultTable->setHorizontalHeaderLabels({"Product","Barcode","Expiry Date","Days Expired","Status"});
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->setRowCount(0);

    for (const auto& p : all) {
        if (!p.expiryDate().isValid()) continue;
        if (p.expiryDate() >= today) continue;  // not expired
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        int daysExpired = p.expiryDate().daysTo(today);
        auto mkCell = [&](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t); it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            it->setForeground(QBrush(QColor("#F87171")));
            return it;
        };
        m_resultTable->setItem(row,0,mkCell(p.name(), Qt::AlignLeft|Qt::AlignVCenter));
        m_resultTable->setItem(row,1,mkCell(p.barcode().isEmpty()?"—":p.barcode()));
        m_resultTable->setItem(row,2,mkCell(p.expiryDate().toString("yyyy-MM-dd")));
        m_resultTable->setItem(row,3,mkCell(QString::number(daysExpired)+" days"));
        m_resultTable->setItem(row,4,mkCell(Product::statusToString(p.status())));
    }
    m_summaryLabel->setText(QString("%1 expired product(s)").arg(m_resultTable->rowCount()));
}

// ── Cancelled Orders Report — clean table layout for QTextDocument ────────────
void ReportsPage::generateCancelledOrdersReport() {
    QDateTime from(m_dateFromEdit->date(), QTime(0, 0, 0));
    QDateTime to(m_dateToEdit->date(),     QTime(23, 59, 59));
    QList<Order> orders = m_orderRepo->getOrdersDateRange(from, to);
    const QString sym = ConfigManager::instance().currencySymbol();

    double totalLost = 0.0, totalDiscount = 0.0;
    int count = 0;

    // QTextDocument only supports HTML4 + CSS1 subset.
    // No display:flex, no border-radius, no box-shadow — use plain <table> layout.
    QString html = R"(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<style>
  body  { font-family: Arial, sans-serif; font-size: 11pt; color: #111; margin: 8px; }
  h2    { font-size: 14pt; color: #333; margin: 0 0 4px 0; }
  .order-block { margin-bottom: 24px; }
  .order-header { background-color: #f1f5f9; border: 1px solid #cbd5e1;
                  padding: 6px 10px; margin-bottom: 4px; }
  .order-num  { font-size: 13pt; font-weight: bold; color: #dc2626; }
  .order-meta { font-size: 10pt; color: #475569; }
  .items-table { width: 100%; border-collapse: collapse; margin-bottom: 4px; }
  .items-table th { background-color: #1e293b; color: #ffffff; font-size: 10pt;
                    padding: 5px 8px; text-align: left; border: 1px solid #334155; }
  .items-table td { font-size: 10pt; padding: 4px 8px; border: 1px solid #e2e8f0; }
  .items-table .num { text-align: right; }
  .items-table .ctr { text-align: center; }
  .totals-table { width: 40%; margin-left: 60%; border-collapse: collapse;
                  margin-bottom: 4px; }
  .totals-table td { font-size: 10pt; padding: 3px 8px; border: 1px solid #e2e8f0; }
  .totals-table .lbl { color: #475569; }
  .totals-table .val { text-align: right; font-weight: bold; }
  .grand-row td { background-color: #fee2e2; color: #dc2626;
                  font-size: 11pt; font-weight: bold; border: 1px solid #fca5a5; }
  .reason-box { background-color: #fff7ed; border: 1px solid #fed7aa;
                padding: 6px 10px; font-size: 10pt; color: #7c2d12; margin-top: 4px; }
  .reason-lbl { font-weight: bold; color: #c2410c; }
  .sep { border: none; border-top: 2px solid #94a3b8; margin: 16px 0; }
  .empty { text-align: center; color: #94a3b8; font-size: 12pt; padding: 40px; }
</style>
</head><body>
)";

    // Report title
    html += QString("<h2>Cancelled Orders Report</h2>"
                    "<p style='font-size:10pt;color:#475569;'>Period: %1 to %2</p>")
        .arg(m_dateFromEdit->date().toString("yyyy-MM-dd"),
             m_dateToEdit->date().toString("yyyy-MM-dd"));

    for (const auto& o : orders) {
        if (o.status() != "Cancelled") continue;

        QString cname = o.customerName().isEmpty()
            ? m_customerMap.value(o.customerId(), QString("ID %1").arg(o.customerId()))
            : o.customerName();
        QString invNum = o.invoiceNumber() > 0
            ? QString("INV-%1").arg(o.invoiceNumber(), 4, 10, QChar('0'))
            : QString("#%1").arg(o.id());
        QString pm = o.paymentMethod().isEmpty() ? "Cash" : o.paymentMethod();
        // Show custom payment name if "Other"
        if (pm == "Other" && !o.paymentOtherDetail().isEmpty())
            pm = o.paymentOtherDetail();

        // ── Order header block ────────────────────────────────────────────────
        html += "<div class='order-block'>";
        html += QString(
            "<table width='100%' class='order-header' cellspacing='0' cellpadding='0'><tr>"
            "<td><span class='order-num'>%1</span> &nbsp;"
            "<span style='background:#fee2e2;color:#991b1b;font-size:9pt;"
            "font-weight:bold;padding:1px 6px;'>CANCELLED</span><br>"
            "<span class='order-meta'>Customer: <b>%2</b></span><br>"
            "<span class='order-meta'>Date: %3 &nbsp;|&nbsp; Payment: %4</span></td>"
            "<td style='text-align:right;vertical-align:top;'>"
            "<span style='font-size:13pt;font-weight:bold;color:#dc2626;'>%5 %6</span></td>"
            "</tr></table>")
            .arg(invNum.toHtmlEscaped(), cname.toHtmlEscaped(),
                 o.dateTime().toString("yyyy-MM-dd hh:mm"),
                 pm.toHtmlEscaped(),
                 QString::number(o.grandTotal(), 'f', 2), sym);

        // ── Items table ────────────────────────────────────────────────────────
        html += "<table class='items-table' cellspacing='0'>"
                "<thead><tr>"
                "<th>Product</th>"
                "<th>Barcode</th>"
                "<th class='ctr'>Qty</th>"
                "<th class='num'>Unit Price</th>"
                "<th class='num'>Line Total</th>"
                "</tr></thead><tbody>";

        double sub = 0.0;
        for (const auto& it : o.items()) {
            QString pn = it.productName.isEmpty()
                ? m_productMap.value(it.productId, QString("P%1").arg(it.productId))
                : it.productName;
            double lt = it.quantity * it.unitPrice;
            sub += lt;
            html += QString(
                "<tr>"
                "<td>%1</td>"
                "<td style='font-family:monospace;font-size:9pt;color:#475569;'>%2</td>"
                "<td class='ctr'>%3</td>"
                "<td class='num'>%4 %5</td>"
                "<td class='num'><b>%6 %5</b></td>"
                "</tr>")
                .arg(pn.toHtmlEscaped(),
                     it.productBarcode.isEmpty() ? "—" : it.productBarcode.toHtmlEscaped())
                .arg(it.quantity)
                .arg(QString::number(it.unitPrice, 'f', 2), sym,
                     QString::number(lt, 'f', 2));
        }
        html += "</tbody></table>";

        // ── Totals ─────────────────────────────────────────────────────────────
        html += "<table class='totals-table' cellspacing='0'><tbody>";
        html += QString("<tr><td class='lbl'>Subtotal</td>"
                        "<td class='val'>%1 %2</td></tr>")
            .arg(QString::number(sub, 'f', 2), sym);
        html += QString("<tr><td class='lbl'>Delivery Fee</td>"
                        "<td class='val'>%1 %2</td></tr>")
            .arg(QString::number(o.deliveryFee(), 'f', 2), sym);
        if (o.discountAmount() > 0.001) {
            html += QString("<tr><td class='lbl'>Discount (%1)</td>"
                            "<td class='val' style='color:#dc2626;'>-%2 %3</td></tr>")
                .arg(o.discountReason().isEmpty() ? "—" : o.discountReason().toHtmlEscaped(),
                     QString::number(o.discountAmount(), 'f', 2), sym);
        }
        html += QString("<tr class='grand-row'><td>GRAND TOTAL</td>"
                        "<td class='val'>%1 %2</td></tr>")
            .arg(QString::number(o.grandTotal(), 'f', 2), sym);
        html += "</tbody></table>";

        // ── Cancellation reason — use table for reliable QTextDocument rendering ──
        html += "<table width='100%' cellspacing='0' cellpadding='0' style='margin-top:6px;'>"
                "<tr><td style='background-color:#fff7ed;border:2px solid #f97316;"
                "padding:8px 12px;font-size:10pt;'>"
                "<b style='color:#c2410c;'>Cancellation Reason:</b> ";
        html += o.cancelReason().isEmpty()
            ? "<i style='color:#94a3b8;'>No reason provided</i>"
            : QString("<span style='color:#7c2d12;'>%1</span>")
                .arg(o.cancelReason().toHtmlEscaped());
        html += "</td></tr></table>";

        html += "<table width='100%' cellspacing='0' cellpadding='0'"
                " style='margin:16px 0;'><tr><td style='border-top:2px solid #94a3b8;"
                "height:2px;'></td></tr></table></div>";

        totalLost     += o.grandTotal();
        totalDiscount += o.discountAmount();
        ++count;
    }

    if (count == 0)
        html += "<p class='empty'>No cancelled orders in this date range.</p>";

    html += "</body></html>";
    m_cancelledHtml = html;
    m_invoiceView->setHtml(html);

    m_summaryLabel->setText(
        QString("%1 cancelled  ·  Lost: %2 %3  ·  Discounts: %4 %3")
            .arg(count)
            .arg(QString::number(totalLost, 'f', 2), sym)
            .arg(QString::number(totalDiscount, 'f', 2)));
}

// ── Exports ───────────────────────────────────────────────────────────────────
void ReportsPage::onExportPDF() {
    QString typeName = m_reportTypeCombo->currentText()
        .toLower().replace(' ', '_').replace('/', '_')
        .replace(QRegularExpression("[^a-z0-9_]"), "");
    QString defaultName = QString("report_%1_%2.pdf")
        .arg(typeName, QDate::currentDate().toString("yyyyMMdd"));
    QString path = QFileDialog::getSaveFileName(
        this, "Save PDF Report", defaultName, "PDF Files (*.pdf)");
    if (path.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageOrientation(QPageLayout::Landscape);

    // Cancelled Orders: print from stored HTML (Bug Fix 11: use m_cancelledHtml not toHtml())
    if (m_resultStack->currentIndex() == 1) {
        printer.setPageOrientation(QPageLayout::Portrait);
        printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

        // Bug Fix 11: do NOT set doc.setPageSize() — let QPrinter drive the page geometry.
        // Setting it to printer.pageRect(DevicePixel) caused the content to render
        // at device-pixel scale and appear tiny in the top-left corner.
        QTextDocument doc;
        doc.setHtml(m_cancelledHtml.isEmpty() ? m_invoiceView->toHtml() : m_cancelledHtml);
        doc.print(&printer);

        QMessageBox::information(this, "Export", "PDF exported successfully.");
        Logger::instance().info("Report exported to PDF: " + path);
        return;
    }

    // All other reports: build HTML from table
    QString html = "<html><body style='font-family:Segoe UI,Arial;font-size:10pt;'>";
    html += QString("<h2>%1</h2>").arg(m_reportTypeCombo->currentText());
    html += QString("<p>%1</p>").arg(m_summaryLabel->text());
    html += "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
            "<thead><tr style='background:#1E293B;color:#FFFFFF;'>";
    for (int c = 0; c < m_resultTable->columnCount(); ++c) {
        auto* hdr = m_resultTable->horizontalHeaderItem(c);
        html += QString("<th>%1</th>").arg(hdr ? hdr->text() : "");
    }
    html += "</tr></thead><tbody>";
    for (int r = 0; r < m_resultTable->rowCount(); ++r) {
        html += (r % 2 == 0) ? "<tr>" : "<tr style='background:#f0f4f8;'>";
        for (int c = 0; c < m_resultTable->columnCount(); ++c) {
            auto* it = m_resultTable->item(r, c);
            html += QString("<td align='center'>%1</td>").arg(it ? it->text().toHtmlEscaped() : "");
        }
        html += "</tr>";
    }
    html += "</tbody></table></body></html>";

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);

    QMessageBox::information(this, "Export", "PDF exported successfully.");
    Logger::instance().info("Report exported to PDF: " + path);
}

void ReportsPage::onExportExcel() {
    QString typeName = m_reportTypeCombo->currentText()
        .toLower().replace(' ', '_').replace('/', '_')
        .replace(QRegularExpression("[^a-z0-9_]"), "");
    QString defaultName = QString("report_%1_%2.xlsx")
        .arg(typeName, QDate::currentDate().toString("yyyyMMdd"));
    QString path = QFileDialog::getSaveFileName(
        this, "Save Excel Report", defaultName, "Excel Files (*.xlsx)");
    if (path.isEmpty()) return;

    QXlsx::Document xlsx;
    QXlsx::Format headerFmt;
    headerFmt.setFontBold(true);
    headerFmt.setPatternBackgroundColor(QColor("#1E293B"));
    headerFmt.setFontColor(Qt::white);

    // Cancelled Orders: export flat data directly from DB (not HTML)
    if (m_resultStack->currentIndex() == 1) {
        QDateTime from(m_dateFromEdit->date(), QTime(0,0,0));
        QDateTime to(m_dateToEdit->date(),     QTime(23,59,59));
        QList<Order> orders = m_orderRepo->getOrdersDateRange(from, to);
        const QString sym = ConfigManager::instance().currencySymbol();

        // Headers
        QStringList hdrs = {"Invoice#","Customer","Date","Payment","Products",
                             "Subtotal","Discount","Grand Total","Cancel Reason"};
        for (int c = 0; c < hdrs.size(); ++c)
            xlsx.write(1, c+1, hdrs[c], headerFmt);

        int row = 2;
        for (const auto& o : orders) {
            if (o.status() != "Cancelled") continue;
            QString cname = o.customerName().isEmpty()
                ? m_customerMap.value(o.customerId(), QString("ID %1").arg(o.customerId()))
                : o.customerName();
            QString invNum = o.invoiceNumber() > 0
                ? QString("INV-%1").arg(o.invoiceNumber(),4,10,QChar('0'))
                : QString("#%1").arg(o.id());
            QStringList prods;
            for (const auto& it : o.items()) {
                QString pn = it.productName.isEmpty()
                    ? m_productMap.value(it.productId,QString("P%1").arg(it.productId))
                    : it.productName;
                prods << QString("%1 ×%2").arg(pn).arg(it.quantity);
            }
            xlsx.write(row, 1, invNum);
            xlsx.write(row, 2, cname);
            xlsx.write(row, 3, o.dateTime().toString("yyyy-MM-dd hh:mm"));
            xlsx.write(row, 4, o.paymentMethod().isEmpty()?"Cash":o.paymentMethod());
            xlsx.write(row, 5, prods.join(", "));
            xlsx.write(row, 6, o.subtotal());
            xlsx.write(row, 7, o.discountAmount());
            xlsx.write(row, 8, o.grandTotal());
            xlsx.write(row, 9, o.cancelReason());
            ++row;
        }
    } else {
        // All other reports: export from table widget
        for (int c = 0; c < m_resultTable->columnCount(); ++c) {
            auto* hdr = m_resultTable->horizontalHeaderItem(c);
            xlsx.write(1, c+1, hdr ? hdr->text() : "", headerFmt);
        }
        for (int r = 0; r < m_resultTable->rowCount(); ++r) {
            for (int c = 0; c < m_resultTable->columnCount(); ++c) {
                auto* it = m_resultTable->item(r, c);
                xlsx.write(r+2, c+1, it ? it->text() : "");
            }
        }
    }

    if (xlsx.saveAs(path)) {
        QMessageBox::information(this, "Export", "Excel exported successfully.");
        Logger::instance().info("Report exported to Excel: " + path);
    } else {
        QMessageBox::critical(this, "Error", "Failed to save Excel file.");
    }
}


void ReportsPage::retranslateUi() { retranslateWidget(this); }

// ─────────────────────────────────────────────────────────────────────────────
// Report 8 — Top Products per Customer
// Shows which products a specific customer orders most frequently,
// so the branch can keep those items in stock proactively.
// ─────────────────────────────────────────────────────────────────────────────
void ReportsPage::generateTopProductsPerCustomer() {
    const QString sym = ConfigManager::instance().currencySymbol();
    const QString kw  = m_customerFilterEdit ? m_customerFilterEdit->text().trimmed() : QString();

    // Resolve customer from search keyword
    int     customerId = 0;
    QString custName   = "All Customers";

    if (!kw.isEmpty()) {
        // Use the same search backend as the Customers page (name / phone / address)
        QList<Customer> results = m_customerRepo->search(kw);
        if (results.isEmpty()) {
            m_resultTable->setColumnCount(1);
            m_resultTable->setHorizontalHeaderLabels({"Result"});
            m_resultTable->setRowCount(1);
            auto* it = new QTableWidgetItem(
                QString("No customer found matching \"%1\".\nTry a different name, phone, or address.").arg(kw));
            it->setTextAlignment(Qt::AlignCenter);
            it->setFlags(Qt::ItemIsEnabled);
            m_resultTable->setItem(0, 0, it);
            m_summaryLabel->setText(
                QString("No customer found for \"%1\"").arg(kw));
            return;
        }
        // Use first match; show name in summary
        customerId = results.first().id();
        custName   = results.first().name();
        if (results.size() > 1) {
            custName += QString("  (+%1 other matches — showing first)")
                            .arg(results.size() - 1);
        }
    }

    // Load orders for this customer (or all if no filter)
    QList<Order> orders;
    if (customerId > 0)
        orders = m_orderRepo->getOrdersByCustomerId(customerId);
    else
        orders = m_orderRepo->getAll();

    // Aggregate: productId → {totalQty, totalRevenue, ordersAppearedIn}
    struct ProdStat { int qty = 0; double revenue = 0.0; int ordersAppearedIn = 0; };
    QMap<int, ProdStat> stats;

    for (const auto& o : orders) {
        if (o.status() == "Cancelled") continue;
        for (const auto& item : o.items()) {
            stats[item.productId].qty             += item.quantity;
            stats[item.productId].revenue         += item.quantity * item.unitPrice;
            stats[item.productId].ordersAppearedIn++;
        }
    }

    // Sort by qty descending
    QList<int> ids = stats.keys();
    std::sort(ids.begin(), ids.end(),
              [&](int a, int b){ return stats[a].qty > stats[b].qty; });

    // Build barcode map
    QMap<int, QString> barcodeMap;
    for (const auto& p : m_productRepo->getAll())
        barcodeMap[p.id()] = p.barcode();

    m_resultTable->setColumnCount(5);
    m_resultTable->setHorizontalHeaderLabels({
        "Product", "Barcode", "Total Qty Ordered", "Appeared in Orders", "Total Revenue"
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->setRowCount(0);

    for (int id : ids) {
        int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        const ProdStat& s = stats[id];

        auto mkC = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        QString pname = m_productMap.value(id, QString("Product #%1").arg(id));
        QString bc    = barcodeMap.value(id, "—");

        auto* qtyItem = mkC(QString::number(s.qty));
        // Top product highlighted in gold
        if (row == 0) {
            qtyItem->setForeground(QBrush(QColor("#FBBF24")));
            QFont f = qtyItem->font(); f.setBold(true); qtyItem->setFont(f);
        }

        m_resultTable->setItem(row, 0, mkC(pname, Qt::AlignLeft | Qt::AlignVCenter));
        m_resultTable->setItem(row, 1, mkC(bc));
        m_resultTable->setItem(row, 2, qtyItem);
        m_resultTable->setItem(row, 3, mkC(QString::number(s.ordersAppearedIn)));
        m_resultTable->setItem(row, 4, mkC(
            QString("%1 %2").arg(QString::number(s.revenue, 'f', 2), sym)));
    }

    m_summaryLabel->setText(
        QString("Customer: %1  ·  %2 product(s)  ·  %3 order(s) analysed")
            .arg(custName).arg(ids.size()).arg(orders.size()));
}
