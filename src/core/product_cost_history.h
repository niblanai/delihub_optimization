#ifndef PRODUCT_COST_HISTORY_H
#define PRODUCT_COST_HISTORY_H

#include <QString>
#include <QDate>

class ProductCostHistory {
public:
    ProductCostHistory() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int productId() const { return m_productId; }
    void setProductId(int pid) { m_productId = pid; }

    double costPrice() const { return m_costPrice; }
    void setCostPrice(double price) { m_costPrice = price; }

    QDate effectiveDate() const { return m_effectiveDate; }
    void setEffectiveDate(const QDate& d) { m_effectiveDate = d; }

    int purchaseInvoiceId() const { return m_purchaseInvoiceId; }
    void setPurchaseInvoiceId(int iid) { m_purchaseInvoiceId = iid; }

private:
    int    m_id                  = 0;
    int    m_productId           = 0;
    double m_costPrice           = 0.0;
    QDate  m_effectiveDate;
    int    m_purchaseInvoiceId   = 0;
};

#endif // PRODUCT_COST_HISTORY_H
