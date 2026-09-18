#include "pos_payment_dialog.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "infra/database_connection_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QListWidget>
#include <QGroupBox>
#include <QLineEdit>

PosPaymentDialog::PosPaymentDialog(double totalAmount, QWidget* parent)
    : QDialog(parent), m_totalAmount(totalAmount)
{
    setupUi();
    setWindowTitle(LangManager::instance().t("Payment"));
    setModal(true);
    setMinimumWidth(500);
}

void PosPaymentDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    
    // Title
    auto* titleLbl = new QLabel(LangManager::instance().t("Complete Payment"));
    titleLbl->setStyleSheet("font-size: 18px; font-weight: bold;");
    titleLbl->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLbl);
    
    // Form
    auto* form = new QFormLayout;
    form->setSpacing(15);
    
    // Total Amount (read-only)
    m_totalLbl = new QLabel(QString::number(m_totalAmount, 'f', 2));
    m_totalLbl->setStyleSheet("font-size: 20px; font-weight: bold; color: #10B981;");
    form->addRow(LangManager::instance().t("Total Amount:"), m_totalLbl);
    
    // Payment Method
    m_paymentCombo = new QComboBox;
    m_paymentCombo->addItem(LangManager::instance().t("Cash"), "Cash");
    m_paymentCombo->addItem(LangManager::instance().t("Card"), "Card");
    m_paymentCombo->addItem(LangManager::instance().t("Other"), "Other");
    connect(m_paymentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PosPaymentDialog::onPaymentMethodChanged);
    form->addRow(LangManager::instance().t("Payment Method:"), m_paymentCombo);
    
    // Cash Received (only for Cash payment)
    m_cashReceivedSpin = new QDoubleSpinBox;
    m_cashReceivedSpin->setRange(0, 999999);
    m_cashReceivedSpin->setDecimals(2);
    m_cashReceivedSpin->setValue(m_totalAmount);
    m_cashReceivedSpin->setSuffix(" " + LangManager::instance().t("ج.م"));
    m_cashReceivedSpin->setStyleSheet("font-size: 16px;");
    connect(m_cashReceivedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PosPaymentDialog::onCashReceivedChanged);
    form->addRow(LangManager::instance().t("Cash Received:"), m_cashReceivedSpin);
    
    // Change
    m_changeLbl = new QLabel("0.00");
    m_changeLbl->setStyleSheet("font-size: 18px; font-weight: bold; color: #3B82F6;");
    form->addRow(LangManager::instance().t("Change:"), m_changeLbl);
    
    mainLayout->addLayout(form);
    
    // Customer Selection
    auto* customerBox = new QGroupBox(LangManager::instance().t("Customer (Optional)"));
    auto* customerLayout = new QVBoxLayout(customerBox);
    
    m_customerNameLbl = new QLabel(LangManager::instance().t("Walk-in Customer"));
    m_customerNameLbl->setStyleSheet("font-size: 14px; color: #6B7280;");
    
    m_customerBtn = new QPushButton(LangManager::instance().t("Select Customer"));
    m_customerBtn->setObjectName("secondaryBtn");
    connect(m_customerBtn, &QPushButton::clicked, this, &PosPaymentDialog::onSelectCustomer);
    
    customerLayout->addWidget(m_customerNameLbl);
    customerLayout->addWidget(m_customerBtn);
    mainLayout->addWidget(customerBox);
    
    // Buttons
    auto* btnBox = new QDialogButtonBox;
    m_confirmBtn = new QPushButton(LangManager::instance().t("💳 CONFIRM PAYMENT"));
    m_confirmBtn->setObjectName("primaryBtn");
    m_confirmBtn->setMinimumHeight(50);
    m_confirmBtn->setStyleSheet("font-size: 16px; font-weight: bold;");
    connect(m_confirmBtn, &QPushButton::clicked, this, &PosPaymentDialog::onConfirm);
    
    auto* cancelBtn = new QPushButton(LangManager::instance().t("Cancel"));
    cancelBtn->setObjectName("secondaryBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    btnBox->addButton(m_confirmBtn, QDialogButtonBox::AcceptRole);
    btnBox->addButton(cancelBtn, QDialogButtonBox::RejectRole);
    mainLayout->addWidget(btnBox);
    
    // Initial state
    updateChange();
}

void PosPaymentDialog::onPaymentMethodChanged(int index) {
    m_paymentMethod = m_paymentCombo->currentData().toString();
    
    // Show/hide cash fields
    bool isCash = (m_paymentMethod == "Cash");
    m_cashReceivedSpin->setVisible(isCash);
    m_changeLbl->parentWidget()->setVisible(isCash);
    
    if (!isCash) {
        m_cashReceivedSpin->setValue(m_totalAmount);
    }
}

void PosPaymentDialog::onCashReceivedChanged(double value) {
    updateChange();
}

void PosPaymentDialog::updateChange() {
    double change = m_cashReceivedSpin->value() - m_totalAmount;
    m_changeLbl->setText(QString::number(qMax(0.0, change), 'f', 2) + " " + 
                         LangManager::instance().t("ج.م"));
    
    // Enable/disable confirm based on sufficient cash
    if (m_paymentMethod == "Cash") {
        m_confirmBtn->setEnabled(m_cashReceivedSpin->value() >= m_totalAmount);
    }
}

void PosPaymentDialog::onSelectCustomer() {
    // Load customers
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    ICustomerRepository* repo = useSqlite 
        ? static_cast<ICustomerRepository*>(new SQLiteCustomerRepository)
        : static_cast<ICustomerRepository*>(new AccessCustomerRepository);
    
    auto customers = repo->getAll();
    delete repo;
    
    if (customers.isEmpty()) {
        QMessageBox::information(this, LangManager::instance().t("No Customers"),
            LangManager::instance().t("No customers found. The order will be marked as Walk-in customer."));
        return;
    }
    
    // Simple selection dialog
    QDialog dlg(this);
    dlg.setWindowTitle(LangManager::instance().t("Select Customer"));
    dlg.setMinimumSize(400, 500);
    
    auto* layout = new QVBoxLayout(&dlg);
    
    auto* searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(LangManager::instance().t("Search by name or phone..."));
    layout->addWidget(searchEdit);
    
    auto* listWidget = new QListWidget;
    layout->addWidget(listWidget);
    
    // Populate list
    auto populateList = [&](const QString& filter = "") {
        listWidget->clear();
        for (const auto& c : customers) {
            QString phoneStr = c.phones().isEmpty() ? "" : c.phones().first().number;
            
            if (filter.isEmpty() || 
                c.name().contains(filter, Qt::CaseInsensitive) ||
                phoneStr.contains(filter, Qt::CaseInsensitive)) {
                
                auto* item = new QListWidgetItem(
                    QString("%1 - %2").arg(c.name(), phoneStr));
                item->setData(Qt::UserRole, c.id());
                listWidget->addItem(item);
            }
        }
    };
    
    populateList();
    
    connect(searchEdit, &QLineEdit::textChanged, [&](const QString& text) {
        populateList(text);
    });
    
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btnBox);
    
    if (dlg.exec() == QDialog::Accepted && listWidget->currentItem()) {
        int customerId = listWidget->currentItem()->data(Qt::UserRole).toInt();
        
        // Find customer
        for (const auto& c : customers) {
            if (c.id() == customerId) {
                m_selectedCustomer = c;
                m_customerNameLbl->setText(QString("✓ %1").arg(c.name()));
                m_customerNameLbl->setStyleSheet("font-size: 14px; color: #10B981; font-weight: bold;");
                break;
            }
        }
    }
}

void PosPaymentDialog::onConfirm() {
    // Validation
    if (m_paymentMethod == "Cash" && m_cashReceivedSpin->value() < m_totalAmount) {
        QMessageBox::warning(this, LangManager::instance().t("Insufficient Cash"),
            LangManager::instance().t("Cash received is less than the total amount."));
        return;
    }
    
    accept();
}

QString PosPaymentDialog::paymentMethod() const {
    return m_paymentMethod;
}

double PosPaymentDialog::cashReceived() const {
    return m_cashReceivedSpin->value();
}

double PosPaymentDialog::change() const {
    return qMax(0.0, m_cashReceivedSpin->value() - m_totalAmount);
}

int PosPaymentDialog::selectedCustomerId() const {
    return m_selectedCustomer.id();
}

QString PosPaymentDialog::selectedCustomerName() const {
    return m_selectedCustomer.id() > 0 ? m_selectedCustomer.name() 
                                        : LangManager::instance().t("Walk-in Customer");
}
