#ifndef PURCHASE_INVOICE_ITEM_H
#define PURCHASE_INVOICE_ITEM_H

#include <QString>

class PurchaseInvoiceItem {
public:
    PurchaseInvoiceItem() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int invoiceId() const { return m_invoiceId; }
    void setInvoiceId(int iid) { m_invoiceId = iid; }

    int productId() const { return m_productId; }
    void setProductId(int pid) { m_productId = pid; }

    int quantity() const { return m_quantity; }
    void setQuantity(int qty) { m_quantity = qty; }

    double unitCost() const { return m_unitCost; }
    void setUnitCost(double cost) { m_unitCost = cost; }

    double totalCost() const { return m_totalCost; }
    void setTotalCost(double total) { m_totalCost = total; }

    // Helper
    void calculateTotalCost() { m_totalCost = m_quantity * m_unitCost; }

private:
    int    m_id         = 0;
    int    m_invoiceId  = 0;
    int    m_productId  = 0;
    int    m_quantity   = 0;
    double m_unitCost   = 0.0;
    double m_totalCost  = 0.0;
};

#endif // PURCHASE_INVOICE_ITEM_H
