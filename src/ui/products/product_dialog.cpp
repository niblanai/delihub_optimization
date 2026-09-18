#include "product_dialog.h"
#include "infra/config_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QCheckBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QPixmap>
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFile>

static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    QString devPath = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(devPath)) return devPath;
    return file;
}

ProductDialog::ProductDialog(const QList<Category>& categories, 
                           IProductSubUnitRepository* subUnitRepo,
                           QWidget* parent)
    : QDialog(parent)
    , m_subUnitRepo(subUnitRepo)
{
    setupUi(categories);
}

void ProductDialog::setupUi(const QList<Category>& categories) {
    setWindowTitle("Product");
    setMinimumWidth(500);
    setMinimumHeight(580);
    setModal(true);

    // ── Outer layout: scroll area + buttons ──────────────────────────────────
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(0);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* scrollContents = new QWidget;
    auto* mainLayout = new QVBoxLayout(scrollContents);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 14, 16, 10);
    scrollArea->setWidget(scrollContents);
    outerLayout->addWidget(scrollArea, 1);

    auto* formGroup = new QGroupBox("Product Details");
    auto* formLayout = new QFormLayout(formGroup);
    formLayout->setSpacing(8);
    formLayout->setContentsMargins(12, 10, 12, 10);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("Product name...");
    m_nameEdit->setFixedHeight(30);
    formLayout->addRow("Name *", m_nameEdit);

    m_barcodeEdit = new QLineEdit;
    m_barcodeEdit->setPlaceholderText("Scan or type barcode (optional)...");
    m_barcodeEdit->setFixedHeight(30);
    formLayout->addRow("Barcode", m_barcodeEdit);

    m_catCombo = new QComboBox;
    m_catCombo->setFixedHeight(30);
    m_catCombo->addItem("— Select Category —", 0);
    for (const auto& c : categories)
        m_catCombo->addItem(c.name, c.id);
    formLayout->addRow("Category *", m_catCombo);

    m_priceSpin = new QDoubleSpinBox;
    m_priceSpin->setRange(0.01, 99999.99);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSuffix(" " + ConfigManager::instance().currencySymbol());
    m_priceSpin->setFixedHeight(30);
    formLayout->addRow("Price *", m_priceSpin);

    m_costPriceSpin = new QDoubleSpinBox;
    m_costPriceSpin->setRange(0.00, 99999.99);
    m_costPriceSpin->setDecimals(2);
    m_costPriceSpin->setSuffix(" " + ConfigManager::instance().currencySymbol());
    m_costPriceSpin->setFixedHeight(30);
    m_costPriceSpin->setToolTip("Purchase cost price (سعر الشراء) - updated from purchase invoices");
    formLayout->addRow("Cost Price", m_costPriceSpin);

    // ── Tax ───────────────────────────────────────────────────────────────────
    m_hasTaxCheck = new QCheckBox("Apply tax to this product");
    m_hasTaxCheck->setToolTip("When enabled, tax rate from settings will be applied in POS");
    formLayout->addRow("Tax", m_hasTaxCheck);

    // ── Product Type (جامد/وزني) ─────────────────────────────────────────────
    auto* typeLayout = new QHBoxLayout;
    m_solidRadio = new QRadioButton("Solid (جامد)");
    m_weightedRadio = new QRadioButton("Weighted (وزني)");
    m_solidRadio->setChecked(true);
    typeLayout->addWidget(m_solidRadio);
    typeLayout->addWidget(m_weightedRadio);
    typeLayout->addStretch();
    formLayout->addRow("Product Type:", typeLayout);
    
    m_pluBarcodeEdit = new QLineEdit;
    m_pluBarcodeEdit->setPlaceholderText("PLU barcode for weighted products...");
    m_pluBarcodeEdit->setFixedHeight(30);
    m_pluBarcodeEdit->setEnabled(false);
    formLayout->addRow("PLU Barcode:", m_pluBarcodeEdit);
    
    connect(m_weightedRadio, &QRadioButton::toggled, [this](bool checked) {
        m_pluBarcodeEdit->setEnabled(checked);
    });

    // ── Inventory ─────────────────────────────────────────────────────────────
    m_stockQtySpin = new QSpinBox;
    m_stockQtySpin->setRange(0, 999999);
    m_stockQtySpin->setValue(0);
    m_stockQtySpin->setFixedHeight(30);
    m_stockQtySpin->setToolTip("Current stock quantity in hand");
    formLayout->addRow("Quantity", m_stockQtySpin);

    m_safetyMarginSpin = new QSpinBox;
    m_safetyMarginSpin->setRange(0, 999999);
    m_safetyMarginSpin->setValue(0);
    m_safetyMarginSpin->setFixedHeight(30);
    m_safetyMarginSpin->setToolTip(
        "Auto low-stock alert threshold.\n"
        "When Quantity ≤ Safety Margin, a Missing Products entry is created automatically.\n"
        "Set to 0 to disable auto-alerting.");
    formLayout->addRow("Safety Margin", m_safetyMarginSpin);

    m_statusCombo = new QComboBox;
    m_statusCombo->setFixedHeight(30);
    m_statusCombo->addItem("Active", static_cast<int>(Product::Status::Active));
    m_statusCombo->addItem("Hidden", static_cast<int>(Product::Status::Hidden));
    formLayout->addRow("Status", m_statusCombo);

    // Optional date fields
    m_manufactureDateEdit = new QDateEdit;
    m_manufactureDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_manufactureDateEdit->setCalendarPopup(true);
    m_manufactureDateEdit->setSpecialValueText("— Not set —");
    m_manufactureDateEdit->setDate(QDate());
    m_manufactureDateEdit->setMinimumDate(QDate(1900,1,1));
    m_manufactureDateEdit->setFixedHeight(30);

    auto* mfgRow = new QHBoxLayout;
    mfgRow->setSpacing(6);
    auto* mfgChk = new QCheckBox;
    mfgRow->addWidget(mfgChk);
    mfgRow->addWidget(m_manufactureDateEdit, 1);
    m_manufactureDateEdit->setEnabled(false);
    connect(mfgChk, &QCheckBox::toggled, m_manufactureDateEdit, &QDateEdit::setEnabled);
    connect(mfgChk, &QCheckBox::toggled, this, [this, mfgChk](bool on){
        if (!on) m_manufactureDateEdit->setDate(QDate());
        else if (!m_manufactureDateEdit->date().isValid())
            m_manufactureDateEdit->setDate(QDate::currentDate());
    });
    formLayout->addRow("Manufacture Date", mfgRow);

    m_expiryDateEdit = new QDateEdit;
    m_expiryDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_expiryDateEdit->setCalendarPopup(true);
    m_expiryDateEdit->setSpecialValueText("— Not set —");
    m_expiryDateEdit->setDate(QDate());
    m_expiryDateEdit->setMinimumDate(QDate(1900,1,1));
    m_expiryDateEdit->setFixedHeight(30);

    auto* expRow = new QHBoxLayout;
    expRow->setSpacing(6);
    auto* expChk = new QCheckBox;
    expRow->addWidget(expChk);
    expRow->addWidget(m_expiryDateEdit, 1);
    m_expiryDateEdit->setEnabled(false);
    connect(expChk, &QCheckBox::toggled, m_expiryDateEdit, &QDateEdit::setEnabled);
    connect(expChk, &QCheckBox::toggled, this, [this, expChk](bool on){
        if (!on) m_expiryDateEdit->setDate(QDate());
        else if (!m_expiryDateEdit->date().isValid())
            m_expiryDateEdit->setDate(QDate::currentDate().addYears(1));
    });
    formLayout->addRow("Expiry Date", expRow);

    // ── Quick-expiry shortcut buttons ─────────────────────────────────────────
    // Label aligned as a form row label, buttons in the field column
    auto* shortcutRow = new QHBoxLayout;
    shortcutRow->setSpacing(6);
    auto* btn3  = new QPushButton("+ 3 days");  btn3->setObjectName("secondaryBtn");  btn3->setFixedHeight(26);
    auto* btn7  = new QPushButton("+ 7 days");  btn7->setObjectName("secondaryBtn");  btn7->setFixedHeight(26);
    auto* btn14 = new QPushButton("+ 14 days"); btn14->setObjectName("secondaryBtn"); btn14->setFixedHeight(26);
    shortcutRow->addWidget(btn3);
    shortcutRow->addWidget(btn7);
    shortcutRow->addWidget(btn14);
    shortcutRow->addStretch();
    formLayout->addRow("Quick set:", shortcutRow);

    // Shortcut connects: enable date + set it
    auto setExpiry = [this, expChk](int days) {
        expChk->setChecked(true);
        m_expiryDateEdit->setEnabled(true);
        m_expiryDateEdit->setDate(QDate::currentDate().addDays(days));
    };
    connect(btn3,  &QPushButton::clicked, this, [setExpiry]{ setExpiry(3);  });
    connect(btn7,  &QPushButton::clicked, this, [setExpiry]{ setExpiry(7);  });
    connect(btn14, &QPushButton::clicked, this, [setExpiry]{ setExpiry(14); });

    mainLayout->addWidget(formGroup);

    // ── Product Image ─────────────────────────────────────────────────────────
    auto* imageGroup = new QGroupBox("Product Image");
    auto* imageGroupLayout = new QVBoxLayout(imageGroup);
    imageGroupLayout->setContentsMargins(16, 12, 16, 12);
    imageGroupLayout->setSpacing(12);
    
    m_imagePreview = new QLabel;
    m_imagePreview->setFixedSize(200, 200);
    m_imagePreview->setScaledContents(false);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setStyleSheet("QLabel { background: #f5f5f5; border: 2px dashed #ccc; border-radius: 8px; }");
    m_imagePreview->setText("📷\n\nNo Image");
    imageGroupLayout->addWidget(m_imagePreview, 0, Qt::AlignCenter);
    
    auto* imageBtnLayout = new QHBoxLayout;
    imageBtnLayout->setSpacing(8);
    m_selectImageBtn = new QPushButton("📷 Select Image");
    m_selectImageBtn->setObjectName("secondaryBtn");
    m_selectImageBtn->setMinimumHeight(36);
    m_clearImageBtn = new QPushButton("✖ Clear");
    m_clearImageBtn->setObjectName("secondaryBtn");
    m_clearImageBtn->setMinimumHeight(36);
    m_clearImageBtn->setEnabled(false);
    imageBtnLayout->addWidget(m_selectImageBtn);
    imageBtnLayout->addWidget(m_clearImageBtn);
    imageGroupLayout->addLayout(imageBtnLayout);
    
    mainLayout->addWidget(imageGroup);
    
    connect(m_selectImageBtn, &QPushButton::clicked, this, &ProductDialog::onSelectImage);
    connect(m_clearImageBtn, &QPushButton::clicked, this, &ProductDialog::onClearImage);

    // ── Sub-Units section (وحدة فرعية) ───────────────────────────────────────
    auto* subUnitsGroup = new QGroupBox("Sub-Units (وحدة فرعية)");
    auto* subUnitsLayout = new QVBoxLayout(subUnitsGroup);
    subUnitsLayout->setContentsMargins(10, 8, 10, 8);
    subUnitsLayout->setSpacing(6);

    m_subUnitsTable = new QTableWidget;
    m_subUnitsTable->setColumnCount(6);
    m_subUnitsTable->setHorizontalHeaderLabels({"Name", "Barcode", "Qty/Unit", "Cost", "Sale Price", "Actions"});
    m_subUnitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_subUnitsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_subUnitsTable->verticalHeader()->setVisible(false);
    m_subUnitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_subUnitsTable->setAlternatingRowColors(true);
    m_subUnitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_subUnitsTable->verticalHeader()->setDefaultSectionSize(32);
    m_subUnitsTable->setMinimumHeight(120);
    m_subUnitsTable->setMaximumHeight(200);
    m_subUnitsTable->setObjectName("dataTable");
    subUnitsLayout->addWidget(m_subUnitsTable);

    auto* addSubUnitBtn = new QPushButton("+ Add Sub-Unit");
    addSubUnitBtn->setObjectName("secondaryBtn");
    connect(addSubUnitBtn, &QPushButton::clicked, this, [this]() {
        // Add sub-unit dialog
        QDialog dlg(this);
        dlg.setWindowTitle("Add Sub-Unit");
        dlg.setMinimumWidth(400);
        
        auto* layout = new QFormLayout(&dlg);
        
        auto* nameEdit = new QLineEdit;
        nameEdit->setPlaceholderText("e.g., Bottle, Piece...");
        layout->addRow("Name:", nameEdit);
        
        auto* barcodeEdit = new QLineEdit;
        barcodeEdit->setPlaceholderText("Sub-unit barcode...");
        layout->addRow("Barcode:", barcodeEdit);
        
        auto* qtySpin = new QDoubleSpinBox;
        qtySpin->setRange(0.01, 99999);
        qtySpin->setDecimals(2);
        qtySpin->setValue(1.0);
        layout->addRow("Qty per Unit:", qtySpin);
        
        auto* costSpin = new QDoubleSpinBox;
        costSpin->setRange(0, 99999.99);
        costSpin->setDecimals(2);
        costSpin->setSuffix(" " + ConfigManager::instance().currencySymbol());
        layout->addRow("Cost Price:", costSpin);
        
        auto* saleSpin = new QDoubleSpinBox;
        saleSpin->setRange(0, 99999.99);
        saleSpin->setDecimals(2);
        saleSpin->setSuffix(" " + ConfigManager::instance().currencySymbol());
        layout->addRow("Sale Price:", saleSpin);
        
        auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addRow(btnBox);
        
        if (dlg.exec() == QDialog::Accepted) {
            int row = m_subUnitsTable->rowCount();
            m_subUnitsTable->insertRow(row);
            
            m_subUnitsTable->setItem(row, 0, new QTableWidgetItem(nameEdit->text()));
            m_subUnitsTable->setItem(row, 1, new QTableWidgetItem(barcodeEdit->text()));
            m_subUnitsTable->setItem(row, 2, new QTableWidgetItem(QString::number(qtySpin->value(), 'f', 2)));
            m_subUnitsTable->setItem(row, 3, new QTableWidgetItem(QString::number(costSpin->value(), 'f', 2)));
            m_subUnitsTable->setItem(row, 4, new QTableWidgetItem(QString::number(saleSpin->value(), 'f', 2)));
            
            auto* deleteBtn = new QPushButton;
            deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 16));
            deleteBtn->setObjectName("dangerBtn");
            deleteBtn->setMaximumWidth(32);
            connect(deleteBtn, &QPushButton::clicked, [this, row]() {
                m_subUnitsTable->removeRow(row);
            });
            m_subUnitsTable->setCellWidget(row, 5, deleteBtn);
        }
    });
    subUnitsLayout->addWidget(addSubUnitBtn);
    
    mainLayout->addWidget(subUnitsGroup);

    // ── Price History section ─────────────────────────────────────────────────
    auto* historyGroup = new QGroupBox("Price Change History");
    auto* historyLayout = new QVBoxLayout(historyGroup);
    historyLayout->setContentsMargins(10, 8, 10, 8);
    historyLayout->setSpacing(4);

    m_historyTable = new QTableWidget;
    m_historyTable->setColumnCount(2);
    m_historyTable->setHorizontalHeaderLabels({"Price", "Effective Date"});
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->verticalHeader()->setDefaultSectionSize(28);
    m_historyTable->setMinimumHeight(100);
    m_historyTable->setMaximumHeight(160);
    m_historyTable->setObjectName("dataTable");
    historyLayout->addWidget(m_historyTable);

    mainLayout->addWidget(historyGroup);
    mainLayout->addStretch();

    // Buttons outside the scroll area
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    auto* btnWrapper = new QWidget;
    btnWrapper->setContentsMargins(16, 6, 16, 10);
    auto* btnWrapLayout = new QVBoxLayout(btnWrapper);
    btnWrapLayout->setContentsMargins(0, 0, 0, 0);
    btnWrapLayout->addWidget(m_buttons);
    outerLayout->addWidget(btnWrapper);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &ProductDialog::validate);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ProductDialog::setProduct(const Product& p, const QList<ProductPriceHistory>& history) {
    m_productId = p.id();
    m_nameEdit->setText(p.name());
    m_barcodeEdit->setText(p.barcode());
    m_priceSpin->setValue(p.price());
    m_costPriceSpin->setValue(p.costPrice());
    m_hasTaxCheck->setChecked(p.hasTax());
    
    // Product type
    if (p.type() == Product::Type::Weighted) {
        m_weightedRadio->setChecked(true);
        m_pluBarcodeEdit->setEnabled(true);
    } else {
        m_solidRadio->setChecked(true);
        m_pluBarcodeEdit->setEnabled(false);
    }
    m_pluBarcodeEdit->setText(p.pluBarcode());
    
    m_stockQtySpin->setValue(p.stockQty());
    m_safetyMarginSpin->setValue(p.lowStockThreshold());

    int catIdx = m_catCombo->findData(p.categoryId());
    if (catIdx >= 0) m_catCombo->setCurrentIndex(catIdx);

    int statusIdx = m_statusCombo->findData(static_cast<int>(p.status()));
    if (statusIdx >= 0) m_statusCombo->setCurrentIndex(statusIdx);

    // Dates — tick the checkbox if date is valid
    if (p.manufactureDate().isValid()) {
        m_manufactureDateEdit->setEnabled(true);
        m_manufactureDateEdit->setDate(p.manufactureDate());
        // find mfg checkbox (first child of parent row widget)
        if (auto* row = qobject_cast<QWidget*>(m_manufactureDateEdit->parent()))
            if (auto* chk = row->findChild<QCheckBox*>())
                chk->setChecked(true);
    }
    if (p.expiryDate().isValid()) {
        m_expiryDateEdit->setEnabled(true);
        m_expiryDateEdit->setDate(p.expiryDate());
        if (auto* row = qobject_cast<QWidget*>(m_expiryDateEdit->parent()))
            if (auto* chk = row->findChild<QCheckBox*>())
                chk->setChecked(true);
    }

    // Image
    m_imagePath = p.imagePath();
    if (!m_imagePath.isEmpty() && QFile::exists(m_imagePath)) {
        QPixmap img(m_imagePath);
        if (!img.isNull()) {
            m_imagePreview->setPixmap(img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_clearImageBtn->setEnabled(true);
        }
    }

    // Populate history table
    m_historyTable->setRowCount(0);
    for (const auto& h : history) {
        int row = m_historyTable->rowCount();
        m_historyTable->insertRow(row);

        QString priceStr = QString("%1 %2").arg(QString::number(h.price, 'f', 2), ConfigManager::instance().currencySymbol());
        auto* itemPrice = new QTableWidgetItem(priceStr);
        itemPrice->setTextAlignment(Qt::AlignCenter);

        auto* itemDate = new QTableWidgetItem(h.effectiveDate.toString("yyyy-MM-dd hh:mm"));
        itemDate->setTextAlignment(Qt::AlignCenter);

        m_historyTable->setItem(row, 0, itemPrice);
        m_historyTable->setItem(row, 1, itemDate);
    }
    
    // Load sub-units
    m_subUnitsTable->setRowCount(0);
    if (m_subUnitRepo && p.id() > 0) {
        auto subUnits = m_subUnitRepo->getByProductId(p.id());
        for (const auto& su : subUnits) {
            int row = m_subUnitsTable->rowCount();
            m_subUnitsTable->insertRow(row);
            
            auto* nameItem = new QTableWidgetItem(su.name());
            nameItem->setData(Qt::UserRole, su.id());  // Store sub-unit ID
            m_subUnitsTable->setItem(row, 0, nameItem);
            m_subUnitsTable->setItem(row, 1, new QTableWidgetItem(su.barcode()));
            m_subUnitsTable->setItem(row, 2, new QTableWidgetItem(QString::number(su.quantityPerUnit(), 'f', 2)));
            m_subUnitsTable->setItem(row, 3, new QTableWidgetItem(QString::number(su.costPrice(), 'f', 2)));
            m_subUnitsTable->setItem(row, 4, new QTableWidgetItem(QString::number(su.salePrice(), 'f', 2)));
            
            auto* deleteBtn = new QPushButton;
            deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 16));
            deleteBtn->setObjectName("dangerBtn");
            deleteBtn->setMaximumWidth(32);
            connect(deleteBtn, &QPushButton::clicked, [this, row]() {
                m_subUnitsTable->removeRow(row);
            });
            m_subUnitsTable->setCellWidget(row, 5, deleteBtn);
        }
    }
}

Product ProductDialog::getProduct() const {
    Product p;
    p.setId(m_productId);
    p.setName(m_nameEdit->text().trimmed());
    p.setBarcode(m_barcodeEdit->text().trimmed());
    p.setCategoryId(m_catCombo->currentData().toInt());
    p.setPrice(m_priceSpin->value());
    p.setCostPrice(m_costPriceSpin->value());
    p.setHasTax(m_hasTaxCheck->isChecked());
    p.setType(m_weightedRadio->isChecked() ? Product::Type::Weighted : Product::Type::Solid);
    p.setPluBarcode(m_pluBarcodeEdit->text().trimmed());
    p.setStockQty(m_stockQtySpin->value());
    p.setLowStockThreshold(m_safetyMarginSpin->value());
    p.setStatus(static_cast<Product::Status>(m_statusCombo->currentData().toInt()));
    if (m_manufactureDateEdit->isEnabled() && m_manufactureDateEdit->date().isValid())
        p.setManufactureDate(m_manufactureDateEdit->date());
    if (m_expiryDateEdit->isEnabled() && m_expiryDateEdit->date().isValid())
        p.setExpiryDate(m_expiryDateEdit->date());
    p.setImagePath(m_imagePath);
    return p;
}

void ProductDialog::saveSubUnits(int productId) {
    if (!m_subUnitRepo || productId <= 0) return;
    
    // Delete existing sub-units first (simple approach - can be optimized)
    auto existing = m_subUnitRepo->getByProductId(productId);
    for (const auto& su : existing) {
        m_subUnitRepo->remove(su.id());
    }
    
    // Save new sub-units from table
    for (int row = 0; row < m_subUnitsTable->rowCount(); ++row) {
        ProductSubUnit su;
        su.setProductId(productId);
        su.setName(m_subUnitsTable->item(row, 0)->text());
        su.setBarcode(m_subUnitsTable->item(row, 1)->text());
        su.setQuantityPerUnit(m_subUnitsTable->item(row, 2)->text().toDouble());
        su.setCostPrice(m_subUnitsTable->item(row, 3)->text().toDouble());
        su.setSalePrice(m_subUnitsTable->item(row, 4)->text().toDouble());
        m_subUnitRepo->save(su);
    }
}

void ProductDialog::validate() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Product name is required.");
        m_nameEdit->setFocus();
        return;
    }
    if (m_catCombo->currentData().toInt() == 0) {
        QMessageBox::warning(this, "Validation", "Please select a category.");
        m_catCombo->setFocus();
        return;
    }
    accept();
}

void ProductDialog::onSelectImage() {
    QString file = QFileDialog::getOpenFileName(this, "Select Product Image", QString(),
                                                 "Images (*.png *.jpg *.jpeg *.bmp)");
    if (file.isEmpty()) return;

    QString saved = resizeAndSaveImage(file);
    if (saved.isEmpty()) {
        QMessageBox::warning(this, "Image Error", "Failed to load or save image.");
        return;
    }

    m_imagePath = saved;
    QPixmap img(saved);
    m_imagePreview->setPixmap(img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_clearImageBtn->setEnabled(true);
}

void ProductDialog::onClearImage() {
    m_imagePath.clear();
    m_imagePreview->clear();
    m_imagePreview->setText("📷\n\nNo Image");
    m_clearImageBtn->setEnabled(false);
}

QString ProductDialog::resizeAndSaveImage(const QString& sourcePath) {
    QPixmap original(sourcePath);
    if (original.isNull()) return QString();

    // Resize to 400×400 with aspect ratio
    QPixmap resized = original.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Generate unique filename with UUID
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString fileName = QString("%1.jpg").arg(uuid);
    QString destPath = QDir(productImagesDir()).filePath(fileName);

    if (!resized.save(destPath, "JPG", 85)) return QString();
    return destPath;
}

QString ProductDialog::productImagesDir() {
    // Use ProgramData/DeliHub/images/products pattern (same as attachments)
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString imagesPath = QDir(basePath).filePath("DeliHub/images/products");
    
    QDir dir;
    if (!dir.exists(imagesPath)) {
        dir.mkpath(imagesPath);
    }
    
    return imagesPath;
}
