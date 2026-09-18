#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QFrame>
#include "data/irepositories.h"
#include "ui/dashboard/charts_widget.h"
#include "ui/translatable_page.h"
#include "services/order_service.h"
#include "services/scheduling_service.h"

class DashboardPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);
    ~DashboardPage() override;  // Task 8: unsubscribe from NotificationService
    void refresh();
    void retranslateUi() override;

    // Task 8: called by MainWindow or NotificationService when a scheduled
    // order notification fires; shows the due-today banner.
    void showScheduledOrderBanner(const QList<ScheduledOrder>& dueList);

private:
    void setupUi();
    void loadKPIs();
    void loadRecentOrders();
    void loadDueToday();
    void loadCustomerStatusTables();   // Task 10: populate inactive/hesitant tables
    void updateConnectionStatus();     // Update online/offline indicator

private slots:
    void checkConnectionStatus();      // Periodic connection check

private:
    IOrderRepository*            m_orderRepo     = nullptr;
    ICustomerRepository*         m_customerRepo  = nullptr;
    IProductRepository*          m_productRepo   = nullptr;
    IScheduledOrderRepository*   m_schedRepo     = nullptr;
    IPurchaseInvoiceRepository*  m_purchaseRepo  = nullptr;  // NEW: for profit calculation
    OrderService*                m_orderService  = nullptr;

    // Task 8: scheduling service for due-today check + notification subscription
    SchedulingService*           m_schedService  = nullptr;
    int                          m_notifHandle   = 0;  // NotificationService subscription handle

    // Task 8: due-today banner (mirrors the one on ScheduledOrdersPage)
    QFrame* m_dueTodayBanner = nullptr;
    QLabel* m_dueTodayLabel  = nullptr;
    
    // Connection status indicator (for cloud branches)
    QLabel* m_connectionStatusLabel = nullptr;
    QLabel* m_connectionStatusIcon = nullptr;
    QTimer* m_statusCheckTimer = nullptr;

    // KPI cards — Row 1
    QLabel* m_kpiOrderCount    = nullptr;  // Today's orders
    QLabel* m_kpiSalesTotal    = nullptr;  // Today's revenue
    QLabel* m_kpiPending       = nullptr;  // Pending orders
    QLabel* m_kpiCancelled     = nullptr;  // Cancelled orders (all time / today)
    // KPI cards — Row 2
    QLabel* m_kpiCustCount     = nullptr;  // Total customers
    QLabel* m_kpiBestProduct   = nullptr;  // Best selling product
    QLabel* m_kpiLowStock      = nullptr;  // Low-stock products
    QLabel* m_kpiDueToday      = nullptr;  // Scheduled due today
    // KPI cards — Row 3
    QLabel* m_kpiTopCustomer   = nullptr;  // Top customer by orders
    QLabel* m_kpiDeliveryFees  = nullptr;  // Today's delivery fees
    QLabel* m_kpiMonthRevenue  = nullptr;  // This month revenue
    QLabel* m_kpiExpiredCount  = nullptr;  // Expired products
    QLabel* m_kpiTodayProfit   = nullptr;  // NEW: Today's profit (sales - costs)

    // Tables
    QTableWidget* m_recentOrdersTable    = nullptr;
    QTableWidget* m_cancelledOrdersTable = nullptr;
    QTableWidget* m_dueTodayTable        = nullptr;
    QTableWidget* m_inactiveTable        = nullptr;   // Task 10
    QTableWidget* m_hesitantTable        = nullptr;   // Task 10
    ChartsWidget* m_chartsWidget         = nullptr;
};

#endif // DASHBOARD_PAGE_H
