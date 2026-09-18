#include "returns_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "ui/products/product_quick_edit_dialog.h"
#include "services/archive_manager.h"
#include "services/audit_service.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/tr_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFrame>
#include <QTimer>
#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QDateTime>
#include <QColor>
#include <QBrush>
#include <QCoreApplication>
#include <QFile>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

ReturnsPage::ReturnsPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_repo         = useSqlite ? static_cast<IOrderReturnRepository*>(new SQLiteOrderReturnRepository)
                               : static_cast<IOrderReturnRepository*>(new SQLiteOrderReturnRepository);
    m_orderRepo    = useSqlite ? static_cast<IOrderRepository*>(new SQLiteOrderRepository)    : static_cast<IOrderRepository*>(new AccessOrderRepository);
    m_customerRepo = useSqlite ? static_cast<ICustomerRepository*>(new SQLiteCustomerRepository) : static_cast<ICustomerRepository*>(new AccessCustomerRepository);
    m_productRepo  = useSqlite ? static_cast<IProductRepository*>(new SQLiteProductRepository)   : static_cast<IProductRepository*>(new AccessProductRepository);
    setupUi();
    // Data loaded by MainWindow::navigateTo — no singleShot needed
}

void ReturnsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0); root->setContentsMargins(0,0,0,0);

    auto* toolbar = new QFrame; toolbar->setObjectName("pageToolbar");
    auto* tbL = new QHBoxLayout(toolbar); tbL->setContentsMargins(16,10,16,10);
    auto* title = new QLabel("Returns & Refunds"); title->setObjectName("pageTitle");
    title->setVisible(false);
    m_addBtn    = new QPushButton(LangManager::instance().t("＋ New Return"));    
    m_addBtn->setObjectName("primaryBtn");
    
    m_deleteBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                              QColor("#FFFFFF"), 18));
    m_deleteBtn->setIconSize(QSize(18, 18));
    m_deleteBtn->setEnabled(false);
    
    m_printBtn  = new QPushButton(LangManager::instance().t("Credit Note"));
    m_printBtn->setObjectName("secondaryBtn");
    m_printBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("invoice-receipt-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_printBtn->setIconSize(QSize(18, 18));
    m_printBtn->setEnabled(false);
    tbL->addWidget(title); tbL->addStretch();
    tbL->addWidget(m_addBtn); tbL->addWidget(m_deleteBtn); tbL->addWidget(m_printBtn);
    root->addWidget(toolbar);

    m_table = new QTableWidget(0, 7);
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({
        "#",
        LangManager::instance().t("Order #"),
        LangManager::instance().t("Customer"),
        LangManager::instance().t("Date & Time"),
        LangManager::instance().t("Type"),
        LangManager::instance().t("Refund"),
        LangManager::instance().t("Reason")
    });
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    // Task 7: hide the corner widget that renders as a white box in dark themes
    if (auto* corner = m_table->findChild<QAbstractButton*>())
        corner->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    auto* sb  = new QFrame; sb->setObjectName("statusBar");
    auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(16,4,16,4);
    m_statusLbl = new QLabel("0 returns"); m_statusLbl->setObjectName("statusLabel");
    m_totalLbl  = new QLabel; m_totalLbl->setObjectName("statusLabel");
    sbl->addWidget(m_statusLbl); sbl->addStretch(); sbl->addWidget(m_totalLbl);
    root->addWidget(sb);

    connect(m_addBtn,    &QPushButton::clicked, this, &ReturnsPage::onAdd);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ReturnsPage::onDelete);
    connect(m_printBtn,  &QPushButton::clicked, this, &ReturnsPage::onPrintCreditNote);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ReturnsPage::onSelectionChanged);
    // Task 8: double-click a return row to quick-edit the first product's name/barcode/qty
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int){ onEditReturn(row); });
}

void ReturnsPage::refresh() { loadReturns(); }
void ReturnsPage::retranslateUi() { retranslateWidget(this); }

void ReturnsPage::loadReturns() {
    m_returns = m_repo->getAll();
    m_table->setSortingEnabled(false);   // prevent row scrambling during populate
    m_table->setRowCount(0);
    const QString sym = ConfigManager::instance().currencySymbol();
    double totalRefund = 0.0;
    for (const auto& r : m_returns) {
        int row = m_table->rowCount(); m_table->insertRow(row);
        auto cell = [](const QString& t, Qt::Alignment a=Qt::AlignCenter){
            auto* it=new QTableWidgetItem(t); it->setTextAlignment(a); it->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable); return it;
        };
        m_table->setItem(row,0,cell(QString::number(r.id())));
        m_table->setItem(row,1,cell(QString::number(r.orderId())));
        m_table->setItem(row,2,cell(r.customerName(), Qt::AlignLeft|Qt::AlignVCenter));
        m_table->setItem(row,3,cell(r.dateTime().toString("yyyy-MM-dd hh:mm")));
        m_table->setItem(row,4,cell(r.isFullReturn()?"Full":"Partial"));
        auto* refundItem = cell(QString("%1 %2").arg(QString::number(r.totalRefundAmount(),'f',2),sym));
        refundItem->setForeground(QBrush(QColor("#F87171")));
        m_table->setItem(row,5,refundItem);
        m_table->setItem(row,6,cell(r.reason(), Qt::AlignLeft|Qt::AlignVCenter));
        totalRefund += r.totalRefundAmount();
    }
    m_statusLbl->setText(QString("%1 return(s)").arg(m_returns.size()));
    m_totalLbl->setText(QString("Total Refunded: %1 %2").arg(QString::number(totalRefund,'f',2),sym));
    m_table->setSortingEnabled(true);
    m_deleteBtn->setEnabled(false); m_printBtn->setEnabled(false);
}

void ReturnsPage::onAdd() {
    // Pre-load data
    QList<Customer> customers = m_customerRepo->getAll();
    QList<Product>  products  = m_productRepo->getAll();

    QDialog dlg(this);
    dlg.setWindowTitle("New Return / Refund");
    dlg.setMinimumWidth(540); dlg.setMinimumHeight(520); dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10); layout->setContentsMargins(16,16,16,16);

    auto* form = new QFormLayout;
    form->setSpacing(10);

    // ── Search: by customer name OR order number ──────────────────────────────
    auto* searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Search by customer name or order number...");
    searchEdit->setObjectName("searchEdit");
    form->addRow("Search Order *", searchEdit);

    // Results dropdown
    auto* orderCombo = new QComboBox;
    orderCombo->setMinimumWidth(320);
    orderCombo->addItem("— Enter search above to find orders —", 0);
    form->addRow("Select Order *", orderCombo);

    auto* reasonEdit = new QLineEdit;
    reasonEdit->setPlaceholderText("Reason for return...");
    form->addRow("Reason *", reasonEdit);

    auto* fullChk = new QCheckBox("Full return (all items)");
    fullChk->setChecked(true);
    form->addRow("", fullChk);

    layout->addLayout(form);

    // Items table for partial return
    auto* itemsTable = new QTableWidget(0, 3);
    itemsTable->setObjectName("dataTable");
    itemsTable->setHorizontalHeaderLabels({"Product","Original Qty","Return Qty"});
    itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    itemsTable->verticalHeader()->setVisible(false);
    itemsTable->verticalHeader()->setDefaultSectionSize(40);  // T6: prevent spin clipping
    itemsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);  // T6: lock row height
    itemsTable->setVisible(false);
    layout->addWidget(itemsTable);

    // Cache of loaded orders for search results
    QList<Order> searchResults;
    Order loadedOrder;

    // ── Search logic: match customer name OR invoice number ───────────────────
    auto doSearch = [&](const QString& text) {
        QString lower = text.trimmed().toLower();
        if (lower.isEmpty()) return;

        QList<Order> allOrders = m_orderRepo->getAll();
        QMap<int,QString> custMap;
        for (const auto& c : customers) custMap[c.id()] = c.name();

        searchResults.clear();
        orderCombo->blockSignals(true);
        orderCombo->clear();
        orderCombo->addItem("— Select order —", 0);

        for (auto& o : allOrders) {
            if (o.customerName().isEmpty())
                o.setCustomerName(custMap.value(o.customerId()));

            bool nameMatch = o.customerName().toLower().contains(lower);
            bool numMatch  = QString::number(o.invoiceNumber() > 0
                               ? o.invoiceNumber() : o.id()).contains(lower);
            if (nameMatch || numMatch) {
                searchResults.append(o);
                QString label = QString("INV-%1 — %2 — %3 (%4)")
                    .arg(o.invoiceNumber() > 0 ? o.invoiceNumber() : o.id(), 4, 10, QChar('0'))
                    .arg(o.customerName())
                    .arg(o.dateTime().toString("yyyy-MM-dd"))
                    .arg(o.status());
                orderCombo->addItem(label, o.id());
            }
        }
        orderCombo->blockSignals(false);
        orderCombo->setCurrentIndex(searchResults.size() == 1 ? 1 : 0);
    };

    // ── Auto-fill items when order is selected ────────────────────────────────
    auto fillItems = [&](int orderId) {
        loadedOrder = Order();
        for (const auto& o : searchResults)
            if (o.id() == orderId) { loadedOrder = o; break; }
        if (loadedOrder.id() == 0) return;

        itemsTable->setRowCount(0);
        for (const auto& item : loadedOrder.items()) {
            int row = itemsTable->rowCount();
            itemsTable->insertRow(row);
            QString pname = item.productName;
            if (pname.isEmpty())
                for (const auto& p : products) if (p.id()==item.productId) { pname=p.name(); break; }

            auto* nameItem = new QTableWidgetItem(pname);
            nameItem->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            auto* origItem = new QTableWidgetItem(QString::number(item.quantity));
            origItem->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            origItem->setTextAlignment(Qt::AlignCenter);
            auto* retSpin  = new QSpinBox;
            retSpin->setRange(0, item.quantity); retSpin->setValue(item.quantity);
            retSpin->setProperty("productId", item.productId);
            retSpin->setProperty("unitPrice",  item.unitPrice);
            retSpin->setMinimumHeight(32);
            itemsTable->setItem(row, 0, nameItem);
            itemsTable->setItem(row, 1, origItem);
            itemsTable->setCellWidget(row, 2, retSpin);
        }
    };

    QObject::connect(searchEdit, &QLineEdit::returnPressed, &dlg, [&](){
        doSearch(searchEdit->text());
    });
    QObject::connect(searchEdit, &QLineEdit::textChanged, &dlg, [&](const QString& t){
        if (t.length() >= 2) doSearch(t);
    });
    QObject::connect(orderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
        [&](int idx) {
            int oid = orderCombo->itemData(idx).toInt();
            if (oid > 0) fillItems(oid);
        });
    QObject::connect(fullChk, &QCheckBox::toggled, itemsTable,
        [itemsTable](bool full){ itemsTable->setVisible(!full); });

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    layout->addWidget(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&](){
        if (loadedOrder.id() == 0) {
            QMessageBox::warning(&dlg,"Validation","Please search and select an order first.");
            return;
        }
        if (reasonEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg,"Validation","Please enter a reason for the return.");
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    // ── Build return object ───────────────────────────────────────────────────
    OrderReturn ret;
    ret.setOrderId(loadedOrder.id());
    ret.setCustomerId(loadedOrder.customerId());
    ret.setCustomerName(loadedOrder.customerName());
    ret.setReason(reasonEdit->text().trimmed());
    ret.setDateTime(QDateTime::currentDateTime());
    ret.setFullReturn(fullChk->isChecked());

    QList<OrderReturnItem> items;
    if (!fullChk->isChecked()) {
        for (int r = 0; r < itemsTable->rowCount(); ++r) {
            auto* spin = qobject_cast<QSpinBox*>(itemsTable->cellWidget(r, 2));
            if (!spin || spin->value() == 0) continue;
            OrderReturnItem i;
            i.orderItemProductId = spin->property("productId").toInt();
            i.productName        = itemsTable->item(r,0)->text();
            i.quantityReturned   = spin->value();
            i.unitPrice          = spin->property("unitPrice").toDouble();
            items.append(i);
        }
    } else {
        for (const auto& oi : loadedOrder.items()) {
            OrderReturnItem i;
            i.orderItemProductId = oi.productId;
            i.productName        = oi.productName;
            i.quantityReturned   = oi.quantity;
            i.unitPrice          = oi.unitPrice;
            items.append(i);
        }
    }
    ret.setItems(items);

    if (!m_repo->save(ret)) { QMessageBox::critical(this,"Error","Failed to save return."); return; }

    // Add stock back
    for (const auto& i : ret.items()) {
        Product p = m_productRepo->getById(i.orderItemProductId);
        if (p.id() > 0) {
            p.setStockQty(p.stockQty() + i.quantityReturned);
            m_productRepo->save(p);
        }
    }

    Logger::instance().info(QString("Return created for Order #%1 — refund: %2")
        .arg(ret.orderId()).arg(ret.totalRefundAmount()));
    loadReturns();
}

void ReturnsPage::onDelete() {
    int row = m_table->currentRow(); if (row<0||row>=m_returns.size()) return;
    auto reply = QMessageBox::question(this,"Confirm","Delete this return record?",QMessageBox::Yes|QMessageBox::No);
    if (reply!=QMessageBox::Yes) return;
    // Task 11: soft-delete
    int rid = m_returns.at(row).id();
    QString archErr;
    int archiveId = ArchiveManager::instance().softDelete("OrderReturns", "Id", rid, &archErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive return before deletion:\n" + archErr);
        return;
    }
    AuditService::instance().logDelete("Return", rid,
        QString("{\"archiveId\":%1}").arg(archiveId));
    loadReturns();
}

void ReturnsPage::onSelectionChanged() {
    bool has = m_table->currentRow()>=0;
    m_deleteBtn->setEnabled(has); m_printBtn->setEnabled(has);
}

void ReturnsPage::onPrintCreditNote() {
    int row = m_table->currentRow(); if (row<0||row>=m_returns.size()) return;
    const OrderReturn& ret = m_returns.at(row);
    const QString sym = ConfigManager::instance().currencySymbol();
    const auto& cfg = ConfigManager::instance();

    QString path = QFileDialog::getSaveFileName(this,"Save Credit Note",
        QString("credit_note_%1.pdf").arg(ret.id()),"PDF Files (*.pdf)");
    if (path.isEmpty()) return;

    QString rows;
    for (const auto& i : ret.items())
        rows += QString("<tr><td>%1</td><td align='center'>%2</td><td align='right'>%3 %4</td><td align='right'>%5 %4</td></tr>")
            .arg(i.productName).arg(i.quantityReturned)
            .arg(QString::number(i.unitPrice,'f',2),sym).arg(QString::number(i.totalRefund(),'f',2));

    QString html = QString("<html><body style='font-family:Segoe UI;font-size:11pt;color:#1e293b;padding:20px;'>"
        "<h2 style='color:#0ea5e9;'>CREDIT NOTE</h2>"
        "<p><b>%1</b><br>%2<br>📞 %3</p><hr>"
        "<p><b>Return #:</b> RET-%4 &nbsp;&nbsp; <b>Date:</b> %5</p>"
        "<p><b>Original Order #:</b> %6 &nbsp;&nbsp; <b>Customer:</b> %7</p>"
        "<p><b>Reason:</b> %8</p>"
        "<table border='1' cellspacing='0' cellpadding='4' width='100%%'>"
        "<thead><tr style='background:#1e293b;color:#fff;'><th>Product</th><th>Qty</th><th>Unit Price</th><th>Refund</th></tr></thead>"
        "<tbody>%9</tbody></table>"
        "<p style='text-align:right;font-size:14pt;color:#f87171;'><b>Total Refund: %10 %11</b></p>"
        "</body></html>")
        .arg(cfg.companyName(), cfg.companyAddress(), cfg.companyPhone())
        .arg(ret.id()).arg(ret.dateTime().toString("yyyy-MM-dd hh:mm"))
        .arg(ret.orderId()).arg(ret.customerName())
        .arg(ret.reason()).arg(rows)
        .arg(QString::number(ret.totalRefundAmount(),'f',2), sym);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path);
    printer.setPageOrientation(QPageLayout::Portrait); printer.setPageSize(QPageSize::A4);
    QTextDocument doc; doc.setHtml(html); doc.print(&printer);
    QMessageBox::information(this,"Credit Note","Credit note saved: "+path);
    Logger::instance().info("Credit note printed for return #"+QString::number(ret.id()));
}


// ── Task 8: double-click a return row → quick-edit the product ───────────────
// A return can contain multiple items.  When there is exactly one product
// involved we open ProductQuickEditDialog immediately.  When there are
// multiple items we show a small picker so the user can choose which one
// they want to fix, then open the dialog for that product.
void ReturnsPage::onEditReturn(int row) {
    if (row < 0 || row >= m_returns.size()) return;

    const OrderReturn& ret = m_returns.at(row);
    const QList<OrderReturnItem>& items = ret.items();

    // Collect unique product IDs (a product can appear only once per return)
    QList<int> productIds;
    for (const auto& i : items)
        if (i.orderItemProductId > 0 && !productIds.contains(i.orderItemProductId))
            productIds.append(i.orderItemProductId);

    if (productIds.isEmpty()) {
        QMessageBox::information(this, "No Product",
            "This return has no product records that can be edited.");
        return;
    }

    int targetProductId = 0;

    if (productIds.size() == 1) {
        // Single product — open directly
        targetProductId = productIds.first();
    } else {
        // Multiple products — show a picker dialog
        QDialog picker(this);
        picker.setWindowTitle(QString("Return #%1 — Choose Product to Edit").arg(ret.id()));
        picker.setMinimumWidth(340);
        picker.setModal(true);

        auto* layout = new QVBoxLayout(&picker);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* hint = new QLabel("Double-click the product you want to edit:");
        hint->setObjectName("hintLabel");
        layout->addWidget(hint);

        auto* list = new QListWidget;
        for (const auto& i : items) {
            if (i.orderItemProductId <= 0) continue;
            auto* item = new QListWidgetItem(
                QString("%1  (qty: %2)").arg(
                    i.productName.isEmpty()
                        ? QString("Product #%1").arg(i.orderItemProductId)
                        : i.productName,
                    QString::number(i.quantityReturned)));
            item->setData(Qt::UserRole, i.orderItemProductId);
            list->addItem(item);
        }
        layout->addWidget(list);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        btns->button(QDialogButtonBox::Ok)->setText("Edit Selected");
        btns->button(QDialogButtonBox::Ok)->setObjectName("saveBtn");
        layout->addWidget(btns);

        QObject::connect(btns, &QDialogButtonBox::accepted, &picker, [&]() {
            if (!list->currentItem()) {
                QMessageBox::warning(&picker, "Select", "Please select a product first.");
                return;
            }
            picker.accept();
        });
        QObject::connect(btns, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
        // double-click also accepts
        QObject::connect(list, &QListWidget::itemDoubleClicked, &picker, [&](QListWidgetItem*) {
            picker.accept();
        });

        if (picker.exec() != QDialog::Accepted) return;
        if (!list->currentItem()) return;
        targetProductId = list->currentItem()->data(Qt::UserRole).toInt();
    }

    if (targetProductId <= 0) return;

    ProductQuickEditDialog dlg(targetProductId, m_productRepo, this);
    if (dlg.exec() == QDialog::Accepted && dlg.saveSucceeded()) {
        // Reload the table so the product name column reflects the change
        loadReturns();
    }
}


// Phase 1 Task 15 TODO: When return is processed:
// 1. Create StockMovement with movementType="Return" 
// 2. Increase product stock quantity
// 3. Link to return via referenceType="Return" and referenceId
