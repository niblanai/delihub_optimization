#ifndef ORDERS_PAGE_H
#define ORDERS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QLabel>
#include <QTabWidget>
#include "ui/orders/order_table_model.h"
#include "ui/translatable_page.h"
#include "ui/pagination_bar.h"
#include "data/irepositories.h"
#include "services/order_service.h"
#include "core/delivery_driver.h"
#include "core/region.h"

class OrdersPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit OrdersPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;
    void openOrderForEdit(int orderId);  // called after scheduled order convert

private slots:
    // All Orders tab
    void onSearch();
    void onDateFilterToggled(bool checked);
    void onAddOrder();
    void onEditOrder();
    void onDeleteOrder();
    void onOrderSelectionChanged();
    void onPrintInvoice();

    // History tab
    void onCustomerFilterChanged(int index);

private:
    void setupUi();
    void loadCustomers();
    void loadProducts();
    void loadOrders();
    void loadHistory(int customerId);
    void populateHistoryTable(const QList<Order>& orders);
    void createStockMovementsForOrder(const Order& order);  // Phase 1 Task 15

    // Repositories & service
    IOrderRepository*          m_orderRepo          = nullptr;
    ICustomerRepository*       m_customerRepo       = nullptr;
    IProductRepository*        m_productRepo        = nullptr;
    IDeliveryDriverRepository* m_driverRepo         = nullptr;
    IRegionRepository*         m_regionRepo         = nullptr;
    ICouponRepository*         m_couponRepo         = nullptr;  // Task 12
    IStockMovementRepository*  m_stockMovementRepo  = nullptr;  // Phase 1 Task 15
    OrderService*              m_orderService       = nullptr;

    // Cached data
    QList<Customer>         m_customers;
    QList<Product>          m_products;
    QList<DeliveryDriver>   m_drivers;
    QMap<int, QString>      m_customerMap;
    QMap<int, double>       m_regionFeeMap;  // Task 2: regionId → deliveryFee

    // ── All Orders Tab ────────────────────────────────────────────────────────
    QLineEdit*       m_searchEdit       = nullptr;
    QCheckBox*       m_dateFilterCheck  = nullptr;
    QDateEdit*       m_dateFromEdit     = nullptr;
    QDateEdit*       m_dateToEdit       = nullptr;
    QPushButton*     m_addOrderBtn      = nullptr;
    QPushButton*     m_editOrderBtn     = nullptr;
    QPushButton*     m_deleteOrderBtn   = nullptr;
    QPushButton*     m_invoiceBtn       = nullptr;
    QTableView*      m_ordersTableView  = nullptr;
    QLabel*          m_statusLabel      = nullptr;
    OrderTableModel* m_ordersModel      = nullptr;
    PaginationBar*   m_ordersPagination = nullptr;

    // ── History Tab ───────────────────────────────────────────────────────────
    QLineEdit*       m_historySearchEdit    = nullptr;  // live search by name/phone/address
    QTableWidget*    m_historyTable         = nullptr;
    QLabel*          m_historySummaryLabel  = nullptr;
};

#endif // ORDERS_PAGE_H
