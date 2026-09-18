#ifndef ORDER_H
#define ORDER_H

#include "order_item.h"
#include <QDateTime>
#include <QList>
#include <QStringList>

class Order {
public:
    // ── Status ────────────────────────────────────────────────────────────────
    static QStringList defaultStatuses() {
        return {"Pending", "Out for Delivery", "Delivered", "Cancelled"};
    }

    Order() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    // Human-readable sequential invoice number (separate from DB id)
    int invoiceNumber() const { return m_invoiceNumber; }
    void setInvoiceNumber(int n) { m_invoiceNumber = n; }

    // Invoice barcode for scanning
    QString invoiceBarcode() const { return m_invoiceBarcode; }
    void setInvoiceBarcode(const QString& bc) { m_invoiceBarcode = bc; }

    int customerId() const { return m_customerId; }
    void setCustomerId(int id) { m_customerId = id; }

    QString customerName() const { return m_customerName; }
    void setCustomerName(const QString& name) { m_customerName = name; }
    
    // ── Register Session ──────────────────────────────────────────────────────
    int registerSessionId() const { return m_registerSessionId; }
    void setRegisterSessionId(int id) { m_registerSessionId = id; }

    QDateTime dateTime() const { return m_dateTime; }
    void setDateTime(const QDateTime& dt) { m_dateTime = dt; }

    double deliveryFee() const { return m_deliveryFee; }
    void setDeliveryFee(double fee) {
        m_deliveryFee = fee;
        calculateTotals();
    }

    double subtotal() const { return m_subtotal; }
    double grandTotal() const { return m_grandTotal; }

    QString status() const { return m_status; }
    void setStatus(const QString& s) { m_status = s; }

    QString cancelReason() const { return m_cancelReason; }
    void setCancelReason(const QString& r) { m_cancelReason = r; }

    int driverId() const { return m_driverId; }
    void setDriverId(int id) { m_driverId = id; }

    QString driverName() const { return m_driverName; }
    void setDriverName(const QString& n) { m_driverName = n; }

    QString driverPhone() const { return m_driverPhone; }
    void setDriverPhone(const QString& p) { m_driverPhone = p; }

    // ── Discount ──────────────────────────────────────────────────────────────
    double discountAmount() const { return m_discountAmount; }
    void setDiscountAmount(double v) { m_discountAmount = v; calculateTotals(); }

    QString discountReason() const { return m_discountReason; }
    void setDiscountReason(const QString& r) { m_discountReason = r; }

    // ── Payment Method ────────────────────────────────────────────────────────
    QString paymentMethod() const { return m_paymentMethod; }
    void setPaymentMethod(const QString& m) { m_paymentMethod = m; }

    // Free-text field used when paymentMethod == "Other"
    QString paymentOtherDetail() const { return m_paymentOtherDetail; }
    void setPaymentOtherDetail(const QString& v) { m_paymentOtherDetail = v; }

    static QStringList paymentMethods() {
        return {"Cash", "Visa", "Other"};
    }

    QList<OrderItem> items() const { return m_items; }
    void setItems(const QList<OrderItem>& items) {
        m_items = items;
        calculateTotals();
    }
    void addItem(const OrderItem& item) {
        m_items.append(item);
        calculateTotals();
    }

    void calculateTotals() {
        m_subtotal = 0.0;
        for (const auto& item : m_items)
            m_subtotal += item.totalPrice();
        m_grandTotal = m_subtotal + m_deliveryFee - m_discountAmount;
        if (m_grandTotal < 0) m_grandTotal = 0;
    }

private:
    int     m_id           = 0;
    int     m_invoiceNumber = 0;  // sequential human-readable number
    QString m_invoiceBarcode;     // Barcode for scanning invoice
    int     m_customerId   = 0;
    QString m_customerName;
    int     m_registerSessionId = 0; // POS session link
    QDateTime m_dateTime;
    double  m_deliveryFee  = 0.0;
    double  m_subtotal     = 0.0;
    double  m_grandTotal   = 0.0;
    QString m_status       = "Pending";
    QString m_cancelReason;
    int     m_driverId     = 0;
    QString m_driverName;
    QString m_driverPhone;
    double  m_discountAmount = 0.0;
    QString m_discountReason;
    QString m_paymentMethod  = "Cash";
    QString m_paymentOtherDetail; // filled when m_paymentMethod == "Other"
    QList<OrderItem> m_items;
};

#endif // ORDER_H
