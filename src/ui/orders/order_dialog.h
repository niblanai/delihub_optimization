#ifndef ORDER_DIALOG_H
#define ORDER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QComboBox>
#include "core/order.h"
#include "core/customer.h"
#include "core/product.h"
#include "core/delivery_driver.h"
#include "core/coupon.h"
#include "data/irepositories.h"
#include "services/invoice_parser.h"

class OrderDialog : public QDialog {
    Q_OBJECT
public:
    explicit OrderDialog(const QList<Customer>&      customers,
                         const QList<Product>&        products,
                         const QStringList&           statuses,
                         const QList<DeliveryDriver>& drivers = {},
                         const QMap<int,double>&      regionFeeMap = {},
                         ICouponRepository*           couponRepo   = nullptr,  // Task 12
                         QWidget* parent = nullptr);

    void    setOrder(const Order& order);
    Order   getOrder() const;
    int     appliedCouponId() const { return m_appliedCouponId; }  // Task 12: 0 if none

private slots:
    void onCustomerSearch(const QString& text);
    void onCustomerSelected(QListWidgetItem* item);
    void onAddItem();
    void onRemoveItem();
    void onUploadInvoice();                              // PDF import
    void onUploadXls();                                  // Excel import
    void onUploadInvoiceWithItems(const QList<InvoiceItem>& parsed); // shared flow
    void recalcTotals();
    void validate();
    void onStatusChanged(const QString& status);

protected:
    void keyPressEvent(QKeyEvent* event) override;  // Num+/Num- shortcuts

private:
    void setupUi();
    void rebuildItemRow(int row, int productId = 0, int qty = 1, double unitPrice = 0.0,
                        const QString& barcode = {});

    QList<Customer>       m_customers;
    QList<Product>        m_products;
    QList<DeliveryDriver> m_drivers;
    QStringList           m_statuses;
    QMap<int,double>      m_regionFeeMap;
    ICouponRepository*    m_couponRepo = nullptr;  // Task 12: not owned by dialog
    int                   m_appliedCouponId = 0;   // Task 12: track for incrementUsage on accept
    int                   m_orderId    = 0;
    int                   m_customerId = 0;

    // Customer search
    QLineEdit*   m_customerSearch = nullptr;
    QListWidget* m_customerList   = nullptr;
    QLabel*      m_customerLabel  = nullptr; // shows selected name

    // Header
    QDateTimeEdit*  m_dateTimeEdit    = nullptr;
    QDoubleSpinBox* m_deliveryFeeSpin = nullptr;
    QComboBox*      m_statusCombo     = nullptr;
    QLineEdit*      m_cancelReasonEdit= nullptr;
    QLabel*         m_cancelReasonLbl = nullptr;
    QComboBox*      m_driverCombo     = nullptr;
    QComboBox*      m_paymentCombo    = nullptr;
    QLineEdit*      m_paymentOtherEdit = nullptr;  // visible only when "Other" selected
    QLabel*         m_paymentOtherLbl  = nullptr;
    QDoubleSpinBox* m_discountSpin    = nullptr;
    QLineEdit*      m_discountReasonEdit = nullptr;

    // Task 12: Coupon activation widgets
    QPushButton*    m_activateCouponBtn  = nullptr;
    QWidget*        m_couponRow          = nullptr;  // hidden until button clicked
    QLineEdit*      m_couponCodeEdit     = nullptr;
    QPushButton*    m_applyCouponBtn     = nullptr;
    QLabel*         m_couponStatusLabel  = nullptr;

    // Items table
    QTableWidget* m_itemsTable      = nullptr;
    QPushButton*  m_addItemBtn       = nullptr;
    QPushButton*  m_removeItemBtn    = nullptr;
    QPushButton*  m_uploadInvoiceBtn = nullptr;  // PDF invoice import
    QPushButton*  m_uploadXlsBtn     = nullptr;  // Excel invoice import

    // Totals
    QLabel* m_subtotalLabel   = nullptr;
    QLabel* m_deliveryLabel   = nullptr;
    QLabel* m_grandTotalLabel = nullptr;

    QDialogButtonBox* m_buttons = nullptr;
};

#endif // ORDER_DIALOG_H
