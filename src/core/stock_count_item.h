#ifndef STOCK_COUNT_ITEM_H
#define STOCK_COUNT_ITEM_H

class StockCountItem {
public:
    StockCountItem() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int stockCountId() const { return m_stockCountId; }
    void setStockCountId(int scid) { m_stockCountId = scid; }

    int productId() const { return m_productId; }
    void setProductId(int pid) { m_productId = pid; }

    int systemQty() const { return m_systemQty; }
    void setSystemQty(int qty) { m_systemQty = qty; }

    int actualQty() const { return m_actualQty; }
    void setActualQty(int qty) { m_actualQty = qty; }

    int difference() const { return m_difference; }
    void setDifference(int diff) { m_difference = diff; }

    // Helper
    void calculateDifference() { m_difference = m_actualQty - m_systemQty; }

private:
    int m_id           = 0;
    int m_stockCountId = 0;
    int m_productId    = 0;
    int m_systemQty    = 0;
    int m_actualQty    = 0;
    int m_difference   = 0;
};

#endif // STOCK_COUNT_ITEM_H
