#include "promotion_dialog.h"
#include "services/lang_manager.h"
#include "infra/database_connection_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>

PromotionDialog::PromotionDialog(IPromotionRepository* promotionRepo,
                                 IProductRepository* productRepo,
                                 ICategoryRepository* categoryRepo,
                                 QWidget* parent)
    : QDialog(parent)
    , m_promotionRepo(promotionRepo)
    , m_productRepo(productRepo)
    , m_categoryRepo(categoryRepo)
{
    setupUi();
    retranslateUi();
}

void PromotionDialog::setupUi() {
    setMinimumWidth(500);
    
    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    // Title
    m_titleEdit = new QLineEdit(this);
    formLayout->addRow("Title:", m_titleEdit);

    // Description
    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setMaximumHeight(80);
    formLayout->addRow("Description:", m_descriptionEdit);

    // Type
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Percentage", static_cast<int>(Promotion::Type::Percentage));
    m_typeCombo->addItem("Fixed Amount", static_cast<int>(Promotion::Type::FixedAmount));
    m_typeCombo->addItem("Buy X Get Y", static_cast<int>(Promotion::Type::BuyXGetY));
    m_typeCombo->addItem("Free Shipping", static_cast<int>(Promotion::Type::FreeShipping));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PromotionDialog::onTypeChanged);
    formLayout->addRow("Type:", m_typeCombo);

    // Discount Value
    m_discountSpin = new QDoubleSpinBox(this);
    m_discountSpin->setRange(0, 999999);
    m_discountSpin->setDecimals(2);
    formLayout->addRow("Discount:", m_discountSpin);

    // Start Date
    m_startDateEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_startDateEdit->setCalendarPopup(true);
    formLayout->addRow("Start Date:", m_startDateEdit);

    // End Date
    m_endDateEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), this);
    m_endDateEdit->setCalendarPopup(true);
    formLayout->addRow("End Date:", m_endDateEdit);

    // Status
    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItem("Active", static_cast<int>(Promotion::Status::Active));
    m_statusCombo->addItem("Inactive", static_cast<int>(Promotion::Status::Inactive));
    m_statusCombo->addItem("Scheduled", static_cast<int>(Promotion::Status::Scheduled));
    m_statusCombo->addItem("Expired", static_cast<int>(Promotion::Status::Expired));
    formLayout->addRow("Status:", m_statusCombo);

    // Image Path
    auto* imageLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(this);
    m_btnBrowseImage = new QPushButton("Browse...", this);
    connect(m_btnBrowseImage, &QPushButton::clicked, this, &PromotionDialog::onBrowseImage);
    imageLayout->addWidget(m_imagePathEdit);
    imageLayout->addWidget(m_btnBrowseImage);
    formLayout->addRow("Image:", imageLayout);

    // Product (optional)
    m_productCombo = new QComboBox(this);
    m_productCombo->addItem("(All Products)", QVariant(0));
    if (m_productRepo) {
        QList<Product> products = m_productRepo->getAll();
        for (const Product& p : products) {
            m_productCombo->addItem(p.name(), QVariant(p.id()));
        }
    }
    formLayout->addRow("Product:", m_productCombo);

    // Category (optional)
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem("(All Categories)", QVariant(0));
    if (m_categoryRepo) {
        QList<Category> categories = m_categoryRepo->getAll();
        for (const Category& c : categories) {
            m_categoryCombo->addItem(c.name, QVariant(c.id));
        }
    }
    formLayout->addRow("Category:", m_categoryCombo);

    // Min Purchase Amount
    m_minPurchaseSpin = new QDoubleSpinBox(this);
    m_minPurchaseSpin->setRange(0, 999999);
    m_minPurchaseSpin->setDecimals(2);
    formLayout->addRow("Min Purchase:", m_minPurchaseSpin);

    mainLayout->addLayout(formLayout);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    m_btnSave = new QPushButton("Save", this);
    m_btnCancel = new QPushButton("Cancel", this);
    
    connect(m_btnSave, &QPushButton::clicked, this, &PromotionDialog::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &PromotionDialog::onCancel);
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCancel);
    
    mainLayout->addLayout(btnLayout);
}

void PromotionDialog::setPromotion(const Promotion& promotion) {
    m_promotion = promotion;
    
    m_titleEdit->setText(promotion.title);
    m_descriptionEdit->setPlainText(promotion.description);
    m_typeCombo->setCurrentIndex(m_typeCombo->findData(static_cast<int>(promotion.type)));
    m_discountSpin->setValue(promotion.discountValue);
    m_startDateEdit->setDateTime(promotion.startDate);
    m_endDateEdit->setDateTime(promotion.endDate);
    m_statusCombo->setCurrentIndex(m_statusCombo->findData(static_cast<int>(promotion.status)));
    m_imagePathEdit->setText(promotion.imagePath);
    
    if (promotion.productId > 0) {
        m_productCombo->setCurrentIndex(m_productCombo->findData(promotion.productId));
    }
    if (promotion.categoryId > 0) {
        m_categoryCombo->setCurrentIndex(m_categoryCombo->findData(promotion.categoryId));
    }
    
    m_minPurchaseSpin->setValue(promotion.minPurchaseAmount);
}

Promotion PromotionDialog::getPromotion() const {
    return m_promotion;
}

void PromotionDialog::onSave() {
    if (m_titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Title is required");
        return;
    }
    
    m_promotion.title = m_titleEdit->text().trimmed();
    m_promotion.description = m_descriptionEdit->toPlainText().trimmed();
    m_promotion.type = static_cast<Promotion::Type>(m_typeCombo->currentData().toInt());
    m_promotion.discountValue = m_discountSpin->value();
    m_promotion.startDate = m_startDateEdit->dateTime();
    m_promotion.endDate = m_endDateEdit->dateTime();
    m_promotion.status = static_cast<Promotion::Status>(m_statusCombo->currentData().toInt());
    m_promotion.imagePath = m_imagePathEdit->text().trimmed();
    m_promotion.productId = m_productCombo->currentData().toInt();
    m_promotion.categoryId = m_categoryCombo->currentData().toInt();
    m_promotion.minPurchaseAmount = m_minPurchaseSpin->value();
    
    if (m_promotionRepo->save(m_promotion)) {
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save promotion");
    }
}

void PromotionDialog::onCancel() {
    reject();
}

void PromotionDialog::onTypeChanged(int index) {
    Q_UNUSED(index);
    Promotion::Type type = static_cast<Promotion::Type>(m_typeCombo->currentData().toInt());
    
    if (type == Promotion::Type::Percentage) {
        m_discountSpin->setSuffix("%");
        m_discountSpin->setRange(0, 100);
    } else {
        m_discountSpin->setSuffix("");
        m_discountSpin->setRange(0, 999999);
    }
}

void PromotionDialog::onBrowseImage() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select Promotion Image",
                                                   "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!fileName.isEmpty()) {
        m_imagePathEdit->setText(fileName);
    }
}

void PromotionDialog::retranslateUi() {
    setWindowTitle(LangManager::tr(m_promotion.id == 0 ? "add_promotion" : "edit_promotion"));
    m_btnSave->setText(LangManager::tr("save"));
    m_btnCancel->setText(LangManager::tr("cancel"));
}
