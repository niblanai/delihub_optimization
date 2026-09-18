#include "order_service.h"
#include "services/archive_manager.h"
#include "infra/logger.h"
#include <QDateTime>

OrderService::OrderService(IOrderRepository* orderRepo, ICustomerRepository* customerRepo,
                           IProductRepository* productRepo, IAuditLogRepository* auditRepo)
    : m_orderRepo(orderRepo), m_customerRepo(customerRepo),
      m_productRepo(productRepo), m_auditRepo(auditRepo) {}

bool OrderService::createOrder(Order& order, int userId) {
    order.calculateTotals();
    if (!m_orderRepo->save(order)) {
        Logger::instance().error("OrderService: failed to create order");
        return false;
    }
    logAction("Create", "Order", order.id(), userId);
    Logger::instance().info(QString("Order created ID=%1").arg(order.id()));
    return true;
}

bool OrderService::updateOrder(Order& order, int userId) {
    order.calculateTotals();
    if (!m_orderRepo->save(order)) {
        Logger::instance().error("OrderService: failed to update order");
        return false;
    }
    logAction("Update", "Order", order.id(), userId);
    Logger::instance().info(QString("Order updated ID=%1").arg(order.id()));
    return true;
}

int OrderService::deleteOrder(int orderId, int userId) {
    QString archErr;
    int archiveId = ArchiveManager::instance().softDelete("Orders", "Id", orderId, &archErr);
    if (archiveId < 0) {
        Logger::instance().error("OrderService: failed to archive order — " + archErr);
        return -1;
    }
    logAction("Delete", "Order", orderId, userId);
    Logger::instance().info(QString("Order archived/deleted ID=%1 archive=%2")
                                .arg(orderId).arg(archiveId));
    return archiveId;
}

QList<Order> OrderService::getAllOrders() const {
    return m_orderRepo->getAll();
}

QList<Order> OrderService::getOrdersByCustomer(int customerId) const {
    return m_orderRepo->getOrdersByCustomerId(customerId);
}

Order OrderService::getOrderById(int id) const {
    return m_orderRepo->getById(id);
}

QList<Order> OrderService::getTodaysOrders() const {
    QDateTime start = QDateTime::currentDateTime();
    start.setTime(QTime(0, 0, 0));
    QDateTime end = QDateTime::currentDateTime();
    end.setTime(QTime(23, 59, 59));
    return m_orderRepo->getOrdersDateRange(start, end);
}

double OrderService::getTodaysSalesTotal() const {
    double total = 0.0;
    for (const auto& o : getTodaysOrders()) {
        total += o.grandTotal();
    }
    return total;
}

double OrderService::getTodaysDeliveryFees() const {
    double total = 0.0;
    for (const auto& o : getTodaysOrders()) {
        total += o.deliveryFee();
    }
    return total;
}

int OrderService::getTodaysOrderCount() const {
    return getTodaysOrders().size();
}

QList<Customer> OrderService::getTopCustomers(int limit) const {
    QMap<int, int> orderCounts;
    QMap<int, double> orderTotals;
    for (const auto& o : m_orderRepo->getAll()) {
        orderCounts[o.customerId()]++;
        orderTotals[o.customerId()] += o.grandTotal();
    }

    QList<Customer> result;
    auto customers = m_customerRepo->getAll();
    std::sort(customers.begin(), customers.end(), [&](const Customer& a, const Customer& b) {
        return orderTotals[a.id()] > orderTotals[b.id()];
    });

    for (int i = 0; i < qMin(limit, customers.size()); ++i) {
        result.append(customers[i]);
    }
    return result;
}

Product OrderService::getBestSellingProduct() const {
    QMap<int, int> qtyMap;
    for (const auto& o : m_orderRepo->getAll()) {
        for (const auto& item : o.items()) {
            qtyMap[item.productId] += item.quantity;
        }
    }
    int bestId = 0;
    int bestQty = 0;
    for (auto it = qtyMap.begin(); it != qtyMap.end(); ++it) {
        if (it.value() > bestQty) {
            bestQty = it.value();
            bestId = it.key();
        }
    }
    return m_productRepo->getById(bestId);
}

void OrderService::logAction(const QString& action, const QString& entityType, int entityId, int userId) {
    if (!m_auditRepo) return;
    AuditLogEntry entry;
    entry.userId = userId;
    entry.action = action;
    entry.entityType = entityType;
    entry.entityId = entityId;
    entry.timestamp = QDateTime::currentDateTime();
    m_auditRepo->addEntry(entry);
}

// Phase 1 Note: Stock movement integration
// When an order is confirmed/delivered, the UI should:
// 1. Create StockMovement entries with movementType="Sale" for each order item
// 2. Decrease product stock quantities accordingly
// 3. Link movements to the order via referenceType="Order" and referenceId=orderId
// 
// This is typically done in the order dialog's save/confirm handler.
