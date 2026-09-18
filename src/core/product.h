#ifndef PRODUCT_H
#define PRODUCT_H

#include <QString>
#include <QDate>

class Product {
public:
    enum class Status { Active, Hidden };
    enum class Type { Solid, Weighted };  // جامد (solid) أو وزني (weighted/by weight)

    Product() = default;
    Product(int id, const QString& name, double price, int categoryId, Status status = Status::Active)
        : m_id(id), m_name(name), m_price(price), m_categoryId(categoryId), m_status(status) {}

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString barcode() const { return m_barcode; }
    void setBarcode(const QString& b) { m_barcode = b; }

    double price() const { return m_price; }
    void setPrice(double p) { m_price = p; }

    int categoryId() const { return m_categoryId; }
    void setCategoryId(int id) { m_categoryId = id; }

    Status status() const { return m_status; }
    void setStatus(Status s) { m_status = s; }

    QDate manufactureDate() const { return m_manufactureDate; }
    void setManufactureDate(const QDate& d) { m_manufactureDate = d; }

    QDate expiryDate() const { return m_expiryDate; }
    void setExpiryDate(const QDate& d) { m_expiryDate = d; }

    // ── Inventory ─────────────────────────────────────────────────────────────
    int stockQty() const { return m_stockQty; }
    void setStockQty(int qty) { m_stockQty = qty; }

    int lowStockThreshold() const { return m_lowStockThreshold; }
    void setLowStockThreshold(int t) { m_lowStockThreshold = t; }

    bool isLowStock() const { return m_stockQty <= m_lowStockThreshold && m_stockQty >= 0; }

    // ── Purchasing & Costing ──────────────────────────────────────────────────
    double costPrice() const { return m_costPrice; }
    void setCostPrice(double cost) { m_costPrice = cost; }

    QString unitLabel() const { return m_unitLabel; }
    void setUnitLabel(const QString& label) { m_unitLabel = label; }

    // ── Image ─────────────────────────────────────────────────────────────────
    QString imagePath() const { return m_imagePath; }
    void setImagePath(const QString& path) { m_imagePath = path; }

    // ── Tax ───────────────────────────────────────────────────────────────────
    bool hasTax() const { return m_hasTax; }
    void setHasTax(bool enabled) { m_hasTax = enabled; }
    
    // ── Product Type (جامد/وزني) ─────────────────────────────────────────────
    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }
    
    QString pluBarcode() const { return m_pluBarcode; }
    void setPluBarcode(const QString& plu) { m_pluBarcode = plu; }
    
    // Calculate price with tax (if tax is enabled)
    double priceWithTax(double taxRate) const {
        if (!m_hasTax || taxRate <= 0.0) return m_price;
        return m_price * (1.0 + taxRate / 100.0);
    }

    // Profit margin calculation
    double profitMargin() const { 
        if (m_costPrice > 0) 
            return ((m_price - m_costPrice) / m_costPrice) * 100.0; 
        return 0.0; 
    }

    static QString statusToString(Status s) {
        return (s == Status::Hidden) ? "Hidden" : "Active";
    }
    static Status stringToStatus(const QString& s) {
        return (s.compare("Hidden", Qt::CaseInsensitive) == 0) ? Status::Hidden : Status::Active;
    }
    
    static QString typeToString(Type t) {
        return (t == Type::Weighted) ? "Weighted" : "Solid";
    }
    static Type stringToType(const QString& s) {
        return (s.compare("Weighted", Qt::CaseInsensitive) == 0) ? Type::Weighted : Type::Solid;
    }

private:
    int     m_id         = 0;
    QString m_name;
    QString m_barcode;
    double  m_price      = 0.0;
    int     m_categoryId = 0;
    Status  m_status     = Status::Active;
    QDate   m_manufactureDate;
    QDate   m_expiryDate;
    int     m_stockQty           = 0;
    int     m_lowStockThreshold  = 5;
    double  m_costPrice          = 0.0;
    QString m_unitLabel          = "وحدة";
    QString m_imagePath;
    bool    m_hasTax             = false;
    Type    m_type               = Type::Solid;      // Default: solid product
    QString m_pluBarcode;                             // PLU barcode for weighted products
};

#endif // PRODUCT_H
