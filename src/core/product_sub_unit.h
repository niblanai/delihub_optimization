#ifndef PRODUCT_SUB_UNIT_H
#define PRODUCT_SUB_UNIT_H

#include <QString>

/**
 * @brief Sub-unit of a product (وحدة فرعية)
 * 
 * Example: Product "Coca Cola" has:
 * - Main unit: Case (24 bottles) - main barcode, main price
 * - Sub-unit: Single bottle - sub-barcode, quantity=24, sub-prices
 */
class ProductSubUnit {
public:
    ProductSubUnit() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int productId() const { return m_productId; }
    void setProductId(int id) { m_productId = id; }

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString barcode() const { return m_barcode; }
    void setBarcode(const QString& b) { m_barcode = b; }

    // How many sub-units in one main unit (e.g., 24 bottles per case)
    double quantityPerUnit() const { return m_quantityPerUnit; }
    void setQuantityPerUnit(double qty) { m_quantityPerUnit = qty; }

    double costPrice() const { return m_costPrice; }
    void setCostPrice(double cost) { m_costPrice = cost; }

    double salePrice() const { return m_salePrice; }
    void setSalePrice(double price) { m_salePrice = price; }

private:
    int     m_id               = 0;
    int     m_productId        = 0;
    QString m_name             = "وحدة فرعية";  // e.g., "Bottle", "Piece"
    QString m_barcode;
    double  m_quantityPerUnit  = 1.0;
    double  m_costPrice        = 0.0;
    double  m_salePrice        = 0.0;
};

#endif // PRODUCT_SUB_UNIT_H
