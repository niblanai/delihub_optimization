#ifndef SCHEDULED_ORDER_DIALOG_H
#define SCHEDULED_ORDER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QTimeEdit>
#include <QDateEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QStackedWidget>
#include "core/scheduled_order.h"
#include "core/customer.h"
#include "core/product.h"

class ScheduledOrderDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScheduledOrderDialog(const QList<Customer>& customers,
                                  const QList<Product>&  products,
                                  QWidget* parent = nullptr);

    void           setScheduledOrder(const ScheduledOrder& order);
    ScheduledOrder getScheduledOrder() const;

private slots:
    void onAddItem();
    void onRemoveItem();
    void onRepeatToggled(bool checked);
    void onCustomerSearchChanged(const QString& text);
    void onCustomerSelected(QListWidgetItem* item);
    void validate();

private:
    void setupUi();
    void rebuildItemRow(int row, int productId = 0, int qty = 1);

    QList<Customer> m_customers;
    QList<Product>  m_products;
    int             m_orderId    = 0;
    int             m_customerId = 0;   // selected customer id

    // Header — customer search
    QLineEdit*        m_customerSearchEdit = nullptr;
    QListWidget*      m_customerPopup      = nullptr;
    QLabel*           m_customerSelectedLbl= nullptr;

    // Keep combo for backward compat with getScheduledOrder/setScheduledOrder
    QComboBox*        m_customerCombo = nullptr;

    QTimeEdit*        m_timeEdit      = nullptr;

    // Repeat toggle
    QCheckBox*        m_repeatToggle  = nullptr;
    QStackedWidget*   m_modeStack     = nullptr;

    // Weekday checkboxes
    QCheckBox*        m_dayChecks[7]  = {};

    // One-time date picker
    QDateEdit*        m_oneDateEdit   = nullptr;

    // Reminder settings
    QSpinBox*         m_remindMinSpin      = nullptr;
    QSpinBox*         m_remindRepeatSpin   = nullptr;
    QCheckBox*        m_autoCreateChk     = nullptr;

    // Items
    QTableWidget*     m_itemsTable    = nullptr;
    QPushButton*      m_addItemBtn    = nullptr;
    QPushButton*      m_removeItemBtn = nullptr;

    QDialogButtonBox* m_buttons       = nullptr;
};

#endif // SCHEDULED_ORDER_DIALOG_H
