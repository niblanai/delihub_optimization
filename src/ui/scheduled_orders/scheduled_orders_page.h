#ifndef SCHEDULED_ORDERS_PAGE_H
#define SCHEDULED_ORDERS_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include "data/irepositories.h"
#include "ui/translatable_page.h"
#include "services/scheduling_service.h"

class ScheduledOrdersPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit ScheduledOrdersPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

    // Called from MainWindow to show order edit dialog after conversion
    static ScheduledOrdersPage* instance() { return s_instance; }

signals:
    void orderCreated(int orderId);   // ask MainWindow to switch to Orders and open edit

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onSelectionChanged();
    void onConvertToOrder(int row);
    void onReminderTimer();

private:
    void setupUi();
    void loadCustomers();
    void loadProducts();
    void loadScheduledOrders();
    void showDueTodayBanner(const QList<ScheduledOrder>& dueList);
    void startReminderTimer();
    void checkUpcomingOrders();

    // Repositories & service
    IScheduledOrderRepository* m_repo         = nullptr;
    ICustomerRepository*       m_customerRepo = nullptr;
    IProductRepository*        m_productRepo  = nullptr;
    IOrderRepository*          m_orderRepo    = nullptr;
    SchedulingService*         m_service      = nullptr;

    // Reminder timer
    QTimer* m_reminderTimer = nullptr;

    // Cached data
    QList<Customer>    m_customers;
    QList<Product>     m_products;
    QMap<int, QString> m_customerMap;
    QList<ScheduledOrder> m_orders;

    // UI
    QFrame*      m_dueTodayBanner = nullptr;
    QLabel*      m_dueTodayLabel  = nullptr;
    QTableWidget* m_table         = nullptr;
    QPushButton* m_addBtn         = nullptr;
    QPushButton* m_editBtn        = nullptr;
    QPushButton* m_deleteBtn      = nullptr;
    QLabel*      m_statusLabel    = nullptr;

    static ScheduledOrdersPage* s_instance;
};

#endif // SCHEDULED_ORDERS_PAGE_H
