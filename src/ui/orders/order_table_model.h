#ifndef ORDER_TABLE_MODEL_H
#define ORDER_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include "core/order.h"

class OrderTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColId = 0, ColInvoice, ColCustomer, ColDate, ColStatus,
        ColPayment, ColItems, ColGrandTotal, ColCount
    };

    explicit OrderTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setOrders(const QList<Order>& orders);
    void setCustomerMap(const QMap<int, QString>& map);
    void addOrder(const Order& order);
    void updateOrder(int row, const Order& order);
    void removeOrder(int row);
    const Order& orderAt(int row) const;

private:
    QList<Order>       m_orders;
    QMap<int, QString> m_customerNames;
};

#endif // ORDER_TABLE_MODEL_H
