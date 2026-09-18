#include "product_suppliers_dialog.h"
#include "services/lang_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>

ProductSuppliersDialog::ProductSuppliersDialog(const Product& product,
                                               IProductSupplierRepository* psRepo,
                                               ISupplierRepository* supplierRepo,
                                               QWidget* parent)
    : QDialog(parent)
    , m_product(product)
    , m_psRepo(psRepo)
    , m_supplierRepo(supplierRepo)
{
    setupUi();
    loadSuppliers();
    loadProductSuppliers();
    refreshTable();
}

ProductSuppliersDialog::~ProductSuppliersDialog() = default;

void ProductSuppliersDialog::setupUi() {
    setWindowTitle(LangManager::tr("product_suppliers_title") + " - " + m_product.name());
    resize(700, 500);
    
    auto* mainLayout = new QVBoxLayout(this);
    
    // Current suppliers table
    auto* tableGroup = new QGroupBox(LangManager::tr("current_suppliers"), this);
    auto* tableLayout = new QVBoxLayout(tableGroup);
    
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({
        LangManager::tr("supplier_name"),
        LangManager::tr("purchase_price"),
        LangManager::tr("last_purchase_date"),
        LangManager::tr("preferred"),
        LangManager::tr("best_price_indicator")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableLayout->addWidget(m_table);
    
    // Action buttons for table
    auto* tableActionsLayout = new QHBoxLayout();
    m_preferredBtn = new QPushButton(LangManager::tr("set_as_preferred"), this);
    m_removeBtn = new QPushButton(LangManager::tr("remove_supplier"), this);
    tableActionsLayout->addWidget(m_preferredBtn);
    tableActionsLayout->addWidget(m_removeBtn);
    tableActionsLayout->addStretch();
    tableLayout->addLayout(tableActionsLayout);
    
    mainLayout->addWidget(tableGroup);
    
    // Add new supplier section
    auto* addGroup = new QGroupBox(LangManager::tr("add_supplier"), this);
    auto* formLayout = new QFormLayout(addGroup);
    
    m_supplierCombo = new QComboBox(this);
    formLayout->addRow(LangManager::tr("supplier"), m_supplierCombo);
    
    m_priceSpin = new QDoubleSpinBox(this);
    m_priceSpin->setRange(0.0, 1000000.0);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSuffix(" " + LangManager::tr("currency"));
    formLayout->addRow(LangManager::tr("purchase_price"), m_priceSpin);
    
    m_addBtn = new QPushButton(LangManager::tr("add"), this);
    formLayout->addRow("", m_addBtn);
    
    mainLayout->addWidget(addGroup);
    
    // Dialog buttons
    auto* buttonLayout = new QHBoxLayout();
    m_saveBtn = new QPushButton(LangManager::tr("save"), this);
    m_cancelBtn = new QPushButton(LangManager::tr("cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveBtn);
    buttonLayout->addWidget(m_cancelBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_addBtn, &QPushButton::clicked, this, &ProductSuppliersDialog::onAddSupplier);
    connect(m_removeBtn, &QPushButton::clicked, this, &ProductSuppliersDialog::onRemoveSupplier);
    connect(m_preferredBtn, &QPushButton::clicked, this, &ProductSuppliersDialog::onSetPreferred);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProductSuppliersDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ProductSuppliersDialog::loadSuppliers() {
    m_allSuppliers = m_supplierRepo->getActive();
    m_supplierCombo->clear();
    for (const auto& supplier : m_allSuppliers) {
        m_supplierCombo->addItem(supplier.name(), supplier.id());
    }
}

void ProductSuppliersDialog::loadProductSuppliers() {
    m_productSuppliers = m_psRepo->getByProductId(m_product.id());
}

void ProductSuppliersDialog::refreshTable() {
    m_table->setRowCount(0);
    
    if (m_productSuppliers.empty()) {
        return;
    }
    
    // Find best price
    double bestPrice = m_productSuppliers[0].purchasePrice;
    for (const auto& ps : m_productSuppliers) {
        if (ps.purchasePrice < bestPrice) {
            bestPrice = ps.purchasePrice;
        }
    }
    
    for (const auto& ps : m_productSuppliers) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        m_table->setItem(row, 0, new QTableWidgetItem(ps.supplierName));
        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(ps.purchasePrice, 'f', 2)));
        m_table->setItem(row, 2, new QTableWidgetItem(ps.lastPurchaseDate));
        m_table->setItem(row, 3, new QTableWidgetItem(ps.isPreferred ? "✓" : ""));
        
        // Best price indicator
        QString indicator = (ps.purchasePrice == bestPrice) ? "⭐ " + LangManager::tr("best_price") : "";
        m_table->setItem(row, 4, new QTableWidgetItem(indicator));
        
        // Store supplier ID in first column
        m_table->item(row, 0)->setData(Qt::UserRole, ps.supplierId);
    }
}

void ProductSuppliersDialog::onAddSupplier() {
    if (m_supplierCombo->currentIndex() < 0) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                             LangManager::tr("please_select_supplier"));
        return;
    }
    
    int supplierId = m_supplierCombo->currentData().toInt();
    
    // Check if already exists
    for (const auto& ps : m_productSuppliers) {
        if (ps.supplierId == supplierId) {
            QMessageBox::warning(this, LangManager::tr("warning"), 
                                 LangManager::tr("supplier_already_added"));
            return;
        }
    }
    
    ProductSupplier ps;
    ps.productId = m_product.id();
    ps.supplierId = supplierId;
    ps.purchasePrice = m_priceSpin->value();
    ps.isPreferred = m_productSuppliers.empty(); // First one is preferred by default
    ps.lastPurchaseDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    ps.supplierName = m_supplierCombo->currentText();
    
    if (m_psRepo->save(ps)) {
        m_productSuppliers.push_back(ps);
        refreshTable();
        m_priceSpin->setValue(0.0);
        QMessageBox::information(this, LangManager::tr("success"), 
                                 LangManager::tr("supplier_added_successfully"));
    } else {
        QMessageBox::critical(this, LangManager::tr("error"), 
                              LangManager::tr("failed_to_add_supplier"));
    }
}

void ProductSuppliersDialog::onRemoveSupplier() {
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                             LangManager::tr("please_select_supplier_to_remove"));
        return;
    }
    
    int supplierId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    
    auto reply = QMessageBox::question(this, LangManager::tr("confirm"), 
                                       LangManager::tr("confirm_remove_supplier"),
                                       QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_psRepo->remove(m_product.id(), supplierId)) {
            m_productSuppliers.erase(
                std::remove_if(m_productSuppliers.begin(), m_productSuppliers.end(),
                               [supplierId](const ProductSupplier& ps) {
                                   return ps.supplierId == supplierId;
                               }),
                m_productSuppliers.end());
            refreshTable();
            QMessageBox::information(this, LangManager::tr("success"), 
                                     LangManager::tr("supplier_removed_successfully"));
        } else {
            QMessageBox::critical(this, LangManager::tr("error"), 
                                  LangManager::tr("failed_to_remove_supplier"));
        }
    }
}

void ProductSuppliersDialog::onSetPreferred() {
    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, LangManager::tr("warning"), 
                             LangManager::tr("please_select_supplier_to_set_preferred"));
        return;
    }
    
    int supplierId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    
    if (m_psRepo->setPreferred(m_product.id(), supplierId)) {
        // Update local data
        for (auto& ps : m_productSuppliers) {
            ps.isPreferred = (ps.supplierId == supplierId);
        }
        refreshTable();
        QMessageBox::information(this, LangManager::tr("success"), 
                                 LangManager::tr("preferred_supplier_set_successfully"));
    } else {
        QMessageBox::critical(this, LangManager::tr("error"), 
                              LangManager::tr("failed_to_set_preferred_supplier"));
    }
}

void ProductSuppliersDialog::onSave() {
    accept();
}
