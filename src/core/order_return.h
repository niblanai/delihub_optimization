#ifndef ORDER_RETURN_H
#define ORDER_RETURN_H

#include "order_item.h"
#include <QString>
#include <QDateTime>
#include <QList>

struct OrderReturnItem {
    int     orderItemProductId = 0;
    QString productName;
    int     quantityReturned   = 0;
    double  unitPrice          = 0.0;
    double  totalRefund() const { return quantityReturned * unitPrice; }
};

class OrderReturn {
public:
    OrderReturn() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int orderId() const { return m_orderId; }
    void setOrderId(int id) { m_orderId = id; }

    int customerId() const { return m_customerId; }
    void setCustomerId(int id) { m_customerId = id; }

    QString customerName() const { return m_customerName; }
    void setCustomerName(const QString& n) { m_customerName = n; }

    QString reason() const { return m_reason; }
    void setReason(const QString& r) { m_reason = r; }

    QDateTime dateTime() const { return m_dateTime; }
    void setDateTime(const QDateTime& dt) { m_dateTime = dt; }

    bool isFullReturn() const { return m_isFullReturn; }
    void setFullReturn(bool f) { m_isFullReturn = f; }

    QList<OrderReturnItem> items() const { return m_items; }
    void setItems(const QList<OrderReturnItem>& items) { m_items = items; }

    double totalRefundAmount() const {
        double total = 0.0;
        for (const auto& item : m_items) total += item.totalRefund();
        return total;
    }

private:
    int     m_id             = 0;
    int     m_orderId        = 0;
    int     m_customerId     = 0;
    QString m_customerName;
    QString m_reason;
    QDateTime m_dateTime;
    bool    m_isFullReturn   = true;
    QList<OrderReturnItem> m_items;
};

#endif // ORDER_RETURN_H
