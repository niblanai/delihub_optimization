#ifndef CUSTOMER_DIALOG_H
#define CUSTOMER_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>
#include "core/customer.h"
#include "core/region.h"
#include "core/product.h"

class CustomerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomerDialog(const QList<Region>& regions,
                            const QList<Product>& allProducts,
                            QWidget* parent = nullptr);

    void setCustomer(const Customer& customer);
    Customer getCustomer() const;

    // Pass all existing customers so validate() can detect duplicates.
    // Call this before exec() when adding a new customer.
    void setExistingCustomers(const QList<Customer>& customers) { m_existingCustomers = customers; }

private slots:
    void onAddPhone();
    void onRemovePhone();
    void onAddAddress();
    void onRemoveAddress();
    void onToggleFavorite(QListWidgetItem* item);
    void onRegionChanged(int comboIndex);   // Task 1: auto-fill distance
    void validate();

private:
    void setupUi(const QList<Region>& regions, const QList<Product>& allProducts);

    QList<Region> m_regions;   // Task 1: keep for auto-fill
    QList<Customer> m_existingCustomers; // for duplicate detection

    // Basic info tab
    QLineEdit*      m_nameEdit       = nullptr;
    QDoubleSpinBox* m_distanceSpin   = nullptr;
    QComboBox*      m_regionCombo    = nullptr;
    QComboBox*      m_statusCombo    = nullptr;
    QDoubleSpinBox* m_debtSpin       = nullptr;
    QTextEdit*      m_notesEdit      = nullptr;
    QComboBox*      m_paymentPrefCombo  = nullptr;
    QLineEdit*      m_paymentOtherEdit  = nullptr;
    QLabel*         m_paymentOtherLbl   = nullptr;

    // Phones tab
    QListWidget*    m_phoneList      = nullptr;
    QLineEdit*      m_phoneInput     = nullptr;
    QPushButton*    m_addPhoneBtn    = nullptr;
    QPushButton*    m_removePhoneBtn = nullptr;

    // Addresses tab
    QListWidget*    m_addrList       = nullptr;
    QLineEdit*      m_addrInput      = nullptr;
    QPushButton*    m_addAddrBtn     = nullptr;
    QPushButton*    m_removeAddrBtn  = nullptr;

    // Favorites tab
    QListWidget*    m_favList        = nullptr;

    QDialogButtonBox* m_buttons      = nullptr;

    int m_customerId = 0;
};

#endif // CUSTOMER_DIALOG_H
