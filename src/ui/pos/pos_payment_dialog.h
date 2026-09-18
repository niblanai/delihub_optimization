#ifndef POS_PAYMENT_DIALOG_H
#define POS_PAYMENT_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "core/customer.h"

class PosPaymentDialog : public QDialog {
    Q_OBJECT
public:
    explicit PosPaymentDialog(double totalAmount, QWidget* parent = nullptr);
    
    QString paymentMethod() const;
    double cashReceived() const;
    double change() const;
    int selectedCustomerId() const;
    QString selectedCustomerName() const;

private slots:
    void onPaymentMethodChanged(int index);
    void onCashReceivedChanged(double value);
    void onSelectCustomer();
    void onConfirm();

private:
    void setupUi();
    void updateChange();
    
    double m_totalAmount = 0.0;
    QString m_paymentMethod = "Cash";
    Customer m_selectedCustomer;
    
    QComboBox* m_paymentCombo = nullptr;
    QLabel* m_totalLbl = nullptr;
    QDoubleSpinBox* m_cashReceivedSpin = nullptr;
    QLabel* m_changeLbl = nullptr;
    QPushButton* m_customerBtn = nullptr;
    QLabel* m_customerNameLbl = nullptr;
    QPushButton* m_confirmBtn = nullptr;
};

#endif // POS_PAYMENT_DIALOG_H
