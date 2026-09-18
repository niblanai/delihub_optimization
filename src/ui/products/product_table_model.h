#ifndef PRODUCT_TABLE_MODEL_H
#define PRODUCT_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include "core/product.h"

class ProductTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColId = 0, ColName, ColBarcode, ColCategory, ColPrice,
        ColStock, ColThreshold, ColStatus, ColManufacture, ColExpiry, ColCount
    };

    explicit ProductTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setProducts(const QList<Product>& products);
    void setCategoryMap(const QMap<int, QString>& map);
    void addProduct(const Product& p);
    void updateProduct(int row, const Product& p);
    void removeProduct(int row);
    const Product& productAt(int row) const;

private:
    QList<Product>     m_products;
    QMap<int, QString> m_categoryNames;
};

#endif // PRODUCT_TABLE_MODEL_H
