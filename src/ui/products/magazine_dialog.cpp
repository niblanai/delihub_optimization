#include "magazine_dialog.h"
#include "services/lang_manager.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

MagazineDialog::MagazineDialog(IPromotionRepository* promotionRepo,
                               IProductRepository* productRepo,
                               QWidget* parent)
    : QDialog(parent)
    , m_promotionRepo(promotionRepo)
    , m_productRepo(productRepo)
{
    m_allProducts = m_productRepo->getAll();
    setupUi();
    retranslateUi();
}

void MagazineDialog::setupUi() {
    setMinimumWidth(700);
    setMinimumHeight(600);
    
    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    // Title
    m_titleEdit = new QLineEdit(this);
    formLayout->addRow(LangManager::instance().t("magazine_title") + ":", m_titleEdit);

    // Description
    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setMaximumHeight(80);
    formLayout->addRow(LangManager::instance().t("magazine_description") + ":", m_descriptionEdit);

    // Start Date
    m_startDateEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_startDateEdit->setCalendarPopup(true);
    formLayout->addRow(LangManager::instance().t("magazine_start_date") + ":", m_startDateEdit);

    // End Date
    m_endDateEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), this);
    m_endDateEdit->setCalendarPopup(true);
    formLayout->addRow(LangManager::instance().t("magazine_end_date") + ":", m_endDateEdit);

    // Status
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItem(LangManager::instance().t("magazine_status_active"), static_cast<int>(Promotion::Status::Active));
    m_statusCombo->addItem(LangManager::instance().t("magazine_status_inactive"), static_cast<int>(Promotion::Status::Inactive));
    m_statusCombo->addItem(LangManager::instance().t("magazine_status_scheduled"), static_cast<int>(Promotion::Status::Scheduled));
    m_statusCombo->addItem(LangManager::instance().t("magazine_status_expired"), static_cast<int>(Promotion::Status::Expired));
    formLayout->addRow(LangManager::instance().t("magazine_status") + ":", m_statusCombo);

    mainLayout->addLayout(formLayout);

    // ── Products Section ───────────────────────────────────────────────────
    auto* productsLabel = new QLabel("<b>" + LangManager::instance().t("magazine_add_products") + ":</b>");
    mainLayout->addWidget(productsLabel);

    // Search & Add
    auto* searchLayout = new QHBoxLayout();
    m_productSearchEdit = new QLineEdit(this);
    m_productSearchEdit->setPlaceholderText(LangManager::instance().t("magazine_search_products"));
    connect(m_productSearchEdit, &QLineEdit::returnPressed, this, &MagazineDialog::onSearchProduct);
    
    m_btnAddProduct = new QPushButton(LangManager::instance().t("magazine_add_product_btn"), this);
    m_btnAddProduct->setObjectName("primaryBtn");
    connect(m_btnAddProduct, &QPushButton::clicked, this, &MagazineDialog::onAddProduct);

    searchLayout->addWidget(m_productSearchEdit, 1);
    searchLayout->addWidget(m_btnAddProduct);
    mainLayout->addLayout(searchLayout);

    // Products Table
    m_productsTable = new QTableWidget(this);
    m_productsTable->setColumnCount(5);
    m_productsTable->setHorizontalHeaderLabels({
        "ID", 
        LangManager::instance().t("Name"), 
        LangManager::instance().t("Barcode"), 
        LangManager::instance().t("magazine_current_price"), 
        LangManager::instance().t("magazine_price")
    });
    m_productsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_productsTable->setColumnHidden(0, true); // hide ID
    m_productsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_productsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    // Allow editing only column 4 (Magazine Price)
    m_productsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_productsTable->setAlternatingRowColors(true);
    mainLayout->addWidget(m_productsTable, 1);

    // Remove Button
    m_btnRemoveProduct = new QPushButton(LangManager::instance().t("magazine_remove_product"), this);
    m_btnRemoveProduct->setObjectName("dangerBtn");
    connect(m_btnRemoveProduct, &QPushButton::clicked, this, &MagazineDialog::onRemoveProduct);
    mainLayout->addWidget(m_btnRemoveProduct);

    // ── Save/Cancel Buttons ────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    m_btnSave = new QPushButton(LangManager::tr("save"), this);
    m_btnSave->setObjectName("primaryBtn");
    m_btnCancel = new QPushButton(LangManager::tr("cancel"), this);
    
    connect(m_btnSave, &QPushButton::clicked, this, &MagazineDialog::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &MagazineDialog::onCancel);
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCancel);
    
    mainLayout->addLayout(btnLayout);
}

void MagazineDialog::setPromotion(const Promotion& promotion, const QList<int>& productIds) {
    m_promotion = promotion;
    
    m_titleEdit->setText(promotion.title);
    m_descriptionEdit->setPlainText(promotion.description);
    m_startDateEdit->setDateTime(promotion.startDate);
    m_endDateEdit->setDateTime(promotion.endDate);
    m_statusCombo->setCurrentIndex(m_statusCombo->findData(static_cast<int>(promotion.status)));

    // Load products with magazine prices
    m_productsTable->setRowCount(0);
    for (int pid : productIds) {
        for (const Product& p : m_allProducts) {
            if (p.id() == pid) {
                int row = m_productsTable->rowCount();
                m_productsTable->insertRow(row);
                
                // ID (read-only)
                auto* idItem = new QTableWidgetItem(QString::number(p.id()));
                idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
                m_productsTable->setItem(row, 0, idItem);
                
                // Name (read-only)
                auto* nameItem = new QTableWidgetItem(p.name());
                nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
                m_productsTable->setItem(row, 1, nameItem);
                
                // Barcode (read-only)
                auto* barcodeItem = new QTableWidgetItem(p.barcode());
                barcodeItem->setFlags(barcodeItem->flags() & ~Qt::ItemIsEditable);
                m_productsTable->setItem(row, 2, barcodeItem);
                
                // Current Price (read-only)
                auto* currentPriceItem = new QTableWidgetItem(QString::number(p.price(), 'f', 2));
                currentPriceItem->setFlags(currentPriceItem->flags() & ~Qt::ItemIsEditable);
                m_productsTable->setItem(row, 3, currentPriceItem);
                
                // Get magazine price from repository
                double magPrice = m_promotionRepo->getProductMagazinePrice(promotion.id, pid);
                if (magPrice == 0.0) magPrice = p.price(); // default to product price
                
                // Magazine Price (EDITABLE)
                auto* priceItem = new QTableWidgetItem(QString::number(magPrice, 'f', 2));
                priceItem->setFlags(priceItem->flags() | Qt::ItemIsEditable);
                priceItem->setBackground(QBrush(QColor(255, 255, 200))); // light yellow to indicate editable
                m_productsTable->setItem(row, 4, priceItem);
                break;
            }
        }
    }
}

QList<int> MagazineDialog::getProductIds() const {
    QList<int> ids;
    for (int row = 0; row < m_productsTable->rowCount(); ++row) {
        int id = m_productsTable->item(row, 0)->text().toInt();
        ids.append(id);
    }
    return ids;
}

void MagazineDialog::onSearchProduct() {
    // Auto-add first match on Enter
    onAddProduct();
}

void MagazineDialog::onAddProduct() {
    QString search = m_productSearchEdit->text().trimmed();
    if (search.isEmpty()) {
        QMessageBox::warning(this, "Search", "Please enter product name or barcode");
        return;
    }

    // Search product
    Product found;
    for (const Product& p : m_allProducts) {
        if (p.name().contains(search, Qt::CaseInsensitive) || 
            p.barcode().contains(search, Qt::CaseInsensitive)) {
            found = p;
            break;
        }
    }

    if (found.id() == 0) {
        QMessageBox::information(this, "Not Found", "Product not found");
        return;
    }

    // Check if already added
    for (int row = 0; row < m_productsTable->rowCount(); ++row) {
        if (m_productsTable->item(row, 0)->text().toInt() == found.id()) {
            QMessageBox::information(this, "Already Added", "Product already in magazine");
            m_productSearchEdit->clear();
            return;
        }
    }

    // Add to table
    int row = m_productsTable->rowCount();
    m_productsTable->insertRow(row);
    
    // ID (read-only)
    auto* idItem = new QTableWidgetItem(QString::number(found.id()));
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    m_productsTable->setItem(row, 0, idItem);
    
    // Name (read-only)
    auto* nameItem = new QTableWidgetItem(found.name());
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    m_productsTable->setItem(row, 1, nameItem);
    
    // Barcode (read-only)
    auto* barcodeItem = new QTableWidgetItem(found.barcode());
    barcodeItem->setFlags(barcodeItem->flags() & ~Qt::ItemIsEditable);
    m_productsTable->setItem(row, 2, barcodeItem);
    
    // Current Price (read-only)
    auto* currentPriceItem = new QTableWidgetItem(QString::number(found.price(), 'f', 2));
    currentPriceItem->setFlags(currentPriceItem->flags() & ~Qt::ItemIsEditable);
    m_productsTable->setItem(row, 3, currentPriceItem);
    
    // Magazine price (EDITABLE) - default to product price
    auto* priceItem = new QTableWidgetItem(QString::number(found.price(), 'f', 2));
    priceItem->setFlags(priceItem->flags() | Qt::ItemIsEditable);
    priceItem->setBackground(QBrush(QColor(255, 255, 200))); // light yellow to indicate editable
    m_productsTable->setItem(row, 4, priceItem);

    m_productSearchEdit->clear();
    m_productSearchEdit->setFocus();
}

void MagazineDialog::onRemoveProduct() {
    int currentRow = m_productsTable->currentRow();
    if (currentRow >= 0) {
        m_productsTable->removeRow(currentRow);
    }
}

void MagazineDialog::onSave() {
    if (m_titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Title is required");
        return;
    }

    if (m_productsTable->rowCount() == 0) {
        QMessageBox::warning(this, "Validation", "Please add at least one product");
        return;
    }
    
    // Validate all magazine prices
    for (int row = 0; row < m_productsTable->rowCount(); ++row) {
        double magPrice = m_productsTable->item(row, 4)->text().toDouble();
        if (magPrice <= 0) {
            QMessageBox::warning(this, "Validation", 
                QString("Invalid magazine price for product: %1").arg(m_productsTable->item(row, 1)->text()));
            return;
        }
    }
    
    m_promotion.title = m_titleEdit->text().trimmed();
    m_promotion.description = m_descriptionEdit->toPlainText().trimmed();
    m_promotion.startDate = m_startDateEdit->dateTime();
    m_promotion.endDate = m_endDateEdit->dateTime();
    m_promotion.status = static_cast<Promotion::Status>(m_statusCombo->currentData().toInt());
    
    // Save promotion (this will set m_promotion.id if it's a new promotion)
    if (!m_promotionRepo->save(m_promotion)) {
        // Get the REAL error from the actual query, not a ghost connection
        // The repository should handle its own connection via db()
        QMessageBox::critical(this, "Error", 
            "Failed to save magazine.\n\n"
            "Possible causes:\n"
            "- Promotions table doesn't exist in database\n"
            "- Check that schema_deployer created the table\n"
            "- Run create_phase1_tables.sql if needed");
        return;
    }
    
    // Now m_promotion.id has the correct value (either existing or new)
    // Remove old product associations
    m_promotionRepo->removeAllProductsFromPromotion(m_promotion.id);
    
    // Add new product associations with magazine prices
    bool allSuccess = true;
    for (int row = 0; row < m_productsTable->rowCount(); ++row) {
        int productId = m_productsTable->item(row, 0)->text().toInt();
        double magazinePrice = m_productsTable->item(row, 4)->text().toDouble();
        
        qDebug() << "Saving product" << productId << "with magazine price:" << magazinePrice;
        
        if (!m_promotionRepo->addProductToPromotion(m_promotion.id, productId)) {
            qDebug() << "Failed to add product" << productId << "to promotion";
            allSuccess = false;
            break;
        }
        
        if (!m_promotionRepo->setProductMagazinePrice(m_promotion.id, productId, magazinePrice)) {
            qDebug() << "Failed to set magazine price for product" << productId;
            allSuccess = false;
            break;
        }
        
        qDebug() << "Successfully saved product" << productId << "with magazine price" << magazinePrice;
    }
    
    if (allSuccess) {
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save product associations");
    }
}

void MagazineDialog::onCancel() {
    reject();
}

void MagazineDialog::retranslateUi() {
    setWindowTitle(LangManager::tr(m_promotion.id == 0 ? "add_promotion" : "edit_promotion"));
    m_btnSave->setText(LangManager::tr("save"));
    m_btnCancel->setText(LangManager::tr("cancel"));
    
    // Update status combo box with translated text while preserving current selection
    int currentStatus = m_statusCombo->currentData().toInt();
    m_statusCombo->clear();
    m_statusCombo->addItem(LangManager::instance().t("promotion_status_active"), static_cast<int>(Promotion::Status::Active));
    m_statusCombo->addItem(LangManager::instance().t("promotion_status_inactive"), static_cast<int>(Promotion::Status::Inactive));
    m_statusCombo->addItem(LangManager::instance().t("promotion_status_scheduled"), static_cast<int>(Promotion::Status::Scheduled));
    m_statusCombo->addItem(LangManager::instance().t("promotion_status_expired"), static_cast<int>(Promotion::Status::Expired));
    m_statusCombo->setCurrentIndex(m_statusCombo->findData(currentStatus));
}
