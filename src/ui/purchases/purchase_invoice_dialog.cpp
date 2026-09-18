#include "purchase_invoice_dialog.h"
#include "services/session_manager.h"
#include "services/audit_service.h"
#include "infra/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidget>

PurchaseInvoiceDialog::PurchaseInvoiceDialog(IPurchaseInvoiceRepository* invoiceRepo,
                                           ISupplierRepository* supplierRepo,
                                           IProductRepository* productRepo,
                                           IStockMovementRepository* stockMovementRepo,
                                           IProductCostHistoryRepository* costHistoryRepo,
                                           QWidget* parent)
    : QDialog(parent)
    , m_invoiceRepo(invoiceRepo)
    , m_supplierRepo(supplierRepo)
    , m_productRepo(productRepo)
    , m_stockMovementRepo(stockMovementRepo)
    , m_costHistoryRepo(costHistoryRepo)
{
    setWindowTitle("Purchase Invoice");
    setMinimumSize(900, 600);
    setupUi();
    loadSuppliers();
    loadProducts();
}

void PurchaseInvoiceDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    
    // Header section
    auto* headerLayout = new QFormLayout;
    
    m_invoiceNumberEdit = new QLineEdit;
    m_invoiceNumberEdit->setText(generateInvoiceNumber());
    headerLayout->addRow("Invoice Number:", m_invoiceNumberEdit);
    
    m_supplierCombo = new QComboBox;
    headerLayout->addRow("Supplier:", m_supplierCombo);
    
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setCalendarPopup(true);
    headerLayout->addRow("Date:", m_dateEdit);
    
    root->addLayout(headerLayout);
    
    // Items section
    auto* itemsLabel = new QLabel("<b>Invoice Items:</b>");
    root->addWidget(itemsLabel);
    
    m_itemsTable = new QTableWidget(0, 5);
    m_itemsTable->setHorizontalHeaderLabels({"Product", "Quantity", "Unit Cost", "Total", "Actions"});
    m_itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_itemsTable);
    
    auto* addItemBtn = new QPushButton("+ Add Item");
    addItemBtn->setObjectName("primaryBtn");
    connect(addItemBtn, &QPushButton::clicked, this, &PurchaseInvoiceDialog::onAddItem);
    root->addWidget(addItemBtn);
    
    // Totals section
    auto* totalsLayout = new QHBoxLayout;
    totalsLayout->addStretch();
    m_totalLbl = new QLabel("Total: 0.00");
    m_totalLbl->setStyleSheet("font-size: 18px; font-weight: bold;");
    totalsLayout->addWidget(m_totalLbl);
    root->addLayout(totalsLayout);
    
    // Footer section
    auto* footerLayout = new QFormLayout;
    
    m_paidAmountEdit = new QLineEdit("0.00");
    footerLayout->addRow("Paid Amount:", m_paidAmountEdit);
    
    m_notesEdit = new QTextEdit;
    m_notesEdit->setMaximumHeight(80);
    footerLayout->addRow("Notes:", m_notesEdit);
    
    root->addLayout(footerLayout);
    
    // Buttons
    auto* btnBox = new QDialogButtonBox;
    m_saveBtn = btnBox->addButton("Save as Draft", QDialogButtonBox::ActionRole);
    m_confirmBtn = btnBox->addButton("Confirm & Update Stock", QDialogButtonBox::ActionRole);
    m_confirmBtn->setObjectName("primaryBtn");
    auto* cancelBtn = btnBox->addButton(QDialogButtonBox::Cancel);
    
    connect(m_saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_confirmBtn, &QPushButton::clicked, this, &PurchaseInvoiceDialog::onConfirm);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    root->addWidget(btnBox);
}

void PurchaseInvoiceDialog::loadSuppliers() {
    m_suppliers = m_supplierRepo->getAll();
    m_supplierCombo->clear();
    for (const auto& supplier : m_suppliers) {
        // All suppliers (no status filter for now)
        m_supplierCombo->addItem(supplier.name(), supplier.id());
    }
}

void PurchaseInvoiceDialog::loadProducts() {
    m_products = m_productRepo->getAll();
}

QString PurchaseInvoiceDialog::generateInvoiceNumber() {
    return QString("PI-%1").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
}

void PurchaseInvoiceDialog::onAddItem() {
    // Show product search dialog
    QDialog searchDlg(this);
    searchDlg.setWindowTitle("Select Product");
    searchDlg.setMinimumSize(600, 400);
    
    auto* layout = new QVBoxLayout(&searchDlg);
    
    auto* searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Search by name or barcode...");
    layout->addWidget(searchEdit);
    
    auto* listWidget = new QListWidget;
    layout->addWidget(listWidget);
    
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(btnBox);
    
    // Populate list with all active products initially
    QList<Product> activeProducts;
    for (const auto& p : m_products) {
        if (p.status() == Product::Status::Active) {
            activeProducts.append(p);
        }
    }
    
    auto updateList = [&](const QString& filter) {
        listWidget->clear();
        QString lowerFilter = filter.toLower();
        
        for (const auto& p : activeProducts) {
            bool matches = filter.isEmpty() ||
                          p.name().toLower().contains(lowerFilter) ||
                          p.barcode().toLower().contains(lowerFilter);
            
            if (matches) {
                QString displayText = QString("%1 (Current: %2)")
                    .arg(p.name())
                    .arg(p.stockQty());
                
                auto* item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, p.id());
                listWidget->addItem(item);
            }
        }
    };
    
    // Initial population
    updateList("");
    
    // Connect search
    connect(searchEdit, &QLineEdit::textChanged, updateList);
    connect(listWidget, &QListWidget::itemDoubleClicked, &searchDlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::accepted, &searchDlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &searchDlg, &QDialog::reject);
    
    if (searchDlg.exec() != QDialog::Accepted) return;
    
    auto* selectedItem = listWidget->currentItem();
    if (!selectedItem) return;
    
    int productId = selectedItem->data(Qt::UserRole).toInt();
    
    // Find product
    Product product;
    for (const auto& p : m_products) {
        if (p.id() == productId) {
            product = p;
            break;
        }
    }
    
    if (product.id() == 0) return;
    
    // Get quantity
    bool ok;
    double qty = QInputDialog::getDouble(this, "Quantity", "Enter quantity:", 1.0, 0.01, 100000, 2, &ok);
    if (!ok) return;
    
    // Get unit cost
    double cost = QInputDialog::getDouble(this, "Unit Cost", "Enter unit cost:", product.costPrice(), 0.01, 1000000, 2, &ok);
    if (!ok) return;
    
    // Add item
    PurchaseInvoiceItem item;
    item.setProductId(product.id());
    item.setQuantity(qty);
    item.setUnitCost(cost);
    // No setTotal - it will be calculated when needed
    m_items.append(item);
    
    // Add to table
    int row = m_itemsTable->rowCount();
    m_itemsTable->insertRow(row);
    
    auto* nameItem = new QTableWidgetItem(product.name());
    nameItem->setData(Qt::UserRole, product.id());
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    auto* qtyItem = new QTableWidgetItem(QString::number(qty, 'f', 2));
    qtyItem->setTextAlignment(Qt::AlignCenter);
    qtyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    auto* costItem = new QTableWidgetItem(QString::number(cost, 'f', 2));
    costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    costItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    auto* totalItem = new QTableWidgetItem(QString::number(qty * cost, 'f', 2));
    totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    m_itemsTable->setItem(row, 0, nameItem);
    m_itemsTable->setItem(row, 1, qtyItem);
    m_itemsTable->setItem(row, 2, costItem);
    m_itemsTable->setItem(row, 3, totalItem);
    
    auto* removeBtn = new QPushButton("Remove");
    removeBtn->setProperty("row", row);
    connect(removeBtn, &QPushButton::clicked, this, &PurchaseInvoiceDialog::onRemoveItem);
    m_itemsTable->setCellWidget(row, 4, removeBtn);
    
    updateTotals();
}

void PurchaseInvoiceDialog::onRemoveItem() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    int row = btn->property("row").toInt();
    if (row >= 0 && row < m_items.size()) {
        m_items.removeAt(row);
        m_itemsTable->removeRow(row);
        
        // Update row properties for remaining buttons
        for (int i = row; i < m_itemsTable->rowCount(); ++i) {
            auto* cellBtn = qobject_cast<QPushButton*>(m_itemsTable->cellWidget(i, 4));
            if (cellBtn) cellBtn->setProperty("row", i);
        }
        
        updateTotals();
    }
}

void PurchaseInvoiceDialog::updateTotals() {
    double total = 0.0;
    for (const auto& item : m_items) {
        total += (item.quantity() * item.unitCost());
    }
    m_totalLbl->setText(QString("Total: %1").arg(total, 0, 'f', 2));
}

void PurchaseInvoiceDialog::onConfirm() {
    if (m_items.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Please add at least one item.");
        return;
    }
    
    auto reply = QMessageBox::question(this, "Confirm Purchase",
        "This will:\n"
        "• Update product costs (Last Cost method)\n"
        "• Increase stock quantities\n"
        "• Create stock movement records\n"
        "• Save cost history\n\n"
        "Confirm purchase?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    // Create invoice
    PurchaseInvoice invoice;
    invoice.setInvoiceNumber(m_invoiceNumberEdit->text());
    invoice.setSupplierId(m_supplierCombo->currentData().toInt());
    invoice.setDate(m_dateEdit->date());
    invoice.setTotalAmount(0); // Will be calculated from items
    invoice.setPaidAmount(m_paidAmountEdit->text().toDouble());
    invoice.setStatus(PurchaseInvoice::Status::Confirmed);
    invoice.setNotes(m_notesEdit->toPlainText());
    invoice.setUserId(SessionManager::instance().currentUser().id());
    
    // Calculate total
    double total = 0.0;
    for (const auto& item : m_items) {
        total += (item.quantity() * item.unitCost());
    }
    invoice.setTotalAmount(total);
    
    // Save invoice
    if (!m_invoiceRepo->save(invoice)) {
        QMessageBox::critical(this, "Error", "Failed to save purchase invoice.");
        return;
    }
    
    // Save items and update products
    if (!m_invoiceRepo->saveItems(invoice.id(), m_items)) {
        QMessageBox::critical(this, "Error", "Failed to save invoice items.");
        return;
    }
    
    int currentUserId = SessionManager::instance().currentUser().id();
    QDateTime now = QDateTime::currentDateTime();
    
    for (const auto& item : m_items) {
        // Get product
        Product product = m_productRepo->getById(item.productId());
        if (product.id() <= 0) continue;
        
        double oldCost = product.costPrice();
        double newCost = item.unitCost();
        
        // Task #13: Update CostPrice (Last Cost method)
        product.setCostPrice(newCost);
        
        // Update stock quantity
        product.setStockQty(product.stockQty() + item.quantity());
        
        if (!m_productRepo->save(product)) {
            Logger::instance().error(QString("Failed to update product %1 after purchase").arg(product.id()));
            continue;
        }
        
        // Task #13: Create StockMovement entry
        StockMovement movement;
        movement.productId = product.id();
        movement.movementType = "Purchase";
        movement.quantity = static_cast<int>(item.quantity());
        movement.dateTime = now;
        movement.referenceType = "PurchaseInvoice";
        movement.referenceId = invoice.id();
        movement.notes = QString("Purchase from %1 - Invoice %2").arg(m_supplierCombo->currentText(), invoice.invoiceNumber());
        movement.userId = currentUserId;
        
        if (!m_stockMovementRepo->save(movement)) {
            Logger::instance().error(QString("Failed to create stock movement for product %1").arg(product.id()));
        }
        
        // Task #14: Create cost history if cost changed
        if (qAbs(oldCost - newCost) > 0.01) {
            ProductCostHistory history;
            history.setProductId(product.id());
            history.setCostPrice(newCost);
            history.setEffectiveDate(now.date());
            history.setPurchaseInvoiceId(invoice.id());
            
            if (!m_costHistoryRepo->save(history)) {
                Logger::instance().error(QString("Failed to save cost history for product %1").arg(product.id()));
            }
        }
    }
    
    // Log audit
    AuditService::instance().log("Purchase", "Confirmed", invoice.id(),
        QString("Purchase Invoice %1 confirmed - Total: %2").arg(invoice.invoiceNumber()).arg(total, 0, 'f', 2));
    
    QMessageBox::information(this, "Success",
        QString("Purchase invoice %1 confirmed successfully!\n\n"
                "• %2 item(s) processed\n"
                "• Stock quantities updated\n"
                "• Cost prices updated")
            .arg(invoice.invoiceNumber())
            .arg(m_items.size()));
    
    m_invoiceId = invoice.id();
    accept();
}

void PurchaseInvoiceDialog::setInvoice(const PurchaseInvoice& invoice) {
    m_invoiceId = invoice.id();
    m_invoiceNumberEdit->setText(invoice.invoiceNumber());
    m_dateEdit->setDate(invoice.date());
    m_notesEdit->setPlainText(invoice.notes());
    m_paidAmountEdit->setText(QString::number(invoice.paidAmount(), 'f', 2));
    
    // Set supplier
    int supplierIndex = m_supplierCombo->findData(invoice.supplierId());
    if (supplierIndex >= 0) {
        m_supplierCombo->setCurrentIndex(supplierIndex);
    }
    
    // Load items
    m_items = m_invoiceRepo->getItems(invoice.id());
    m_itemsTable->setRowCount(0);
    for (const auto& item : m_items) {
        // TODO: Add items to table display
    }
    
    // Disable confirm if already confirmed
    if (invoice.status() == PurchaseInvoice::Status::Confirmed) {
        m_confirmBtn->setEnabled(false);
        m_confirmBtn->setText("Already Confirmed");
    }
}

PurchaseInvoice PurchaseInvoiceDialog::getInvoice() const {
    PurchaseInvoice invoice;
    invoice.setId(m_invoiceId);
    invoice.setInvoiceNumber(m_invoiceNumberEdit->text());
    invoice.setSupplierId(m_supplierCombo->currentData().toInt());
    invoice.setDate(m_dateEdit->date());
    double paidAmt = m_paidAmountEdit->text().toDouble();
    invoice.setPaidAmount(paidAmt);
    invoice.setNotes(m_notesEdit->toPlainText());
    invoice.setStatus(PurchaseInvoice::Status::Draft);
    invoice.setUserId(SessionManager::instance().currentUser().id());
    
    double total = 0.0;
    for (const auto& item : m_items) {
        total += (item.quantity() * item.unitCost());
    }
    invoice.setTotalAmount(total);
    
    return invoice;
}
