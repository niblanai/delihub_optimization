#ifndef ORDER_SERVICE_H
#define ORDER_SERVICE_H

#include "core/order.h"
#include "core/customer.h"
#include "core/product.h"
#include "data/irepositories.h"
#include <QList>

class OrderService {
public:
    OrderService(IOrderRepository* orderRepo, ICustomerRepository* customerRepo,
                 IProductRepository* productRepo, IAuditLogRepository* auditRepo = nullptr);

    bool createOrder(Order& order, int userId = 0);
    bool updateOrder(Order& order, int userId = 0);
    // Returns archive id (>= 0) on success, -1 on failure
    int  deleteOrder(int orderId, int userId = 0);

    QList<Order> getAllOrders() const;
    QList<Order> getOrdersByCustomer(int customerId) const;
    Order getOrderById(int id) const;

    QList<Order> getTodaysOrders() const;
    double getTodaysSalesTotal() const;
    double getTodaysDeliveryFees() const;
    int getTodaysOrderCount() const;

    QList<Customer> getTopCustomers(int limit = 5) const;
    Product getBestSellingProduct() const;

private:
    void logAction(const QString& action, const QString& entityType, int entityId, int userId);

    IOrderRepository* m_orderRepo;
    ICustomerRepository* m_customerRepo;
    IProductRepository* m_productRepo;
    IAuditLogRepository* m_auditRepo;
};

#endif // ORDER_SERVICE_H
