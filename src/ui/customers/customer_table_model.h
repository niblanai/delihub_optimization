#ifndef CUSTOMER_TABLE_MODEL_H
#define CUSTOMER_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include "core/customer.h"

class CustomerTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColId = 0, ColName, ColPhone, ColRegion,
        ColDistance, ColStatus, ColDebt, ColAddress, ColNotes, ColCount
    };

    explicit CustomerTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setCustomers(const QList<Customer>& customers);
    void setRegionMap(const QMap<int, QString>& regionMap);   // NEW
    void addCustomer(const Customer& customer);
    void updateCustomer(int row, const Customer& customer);
    void removeCustomer(int row);
    const Customer& customerAt(int row) const;

private:
    QList<Customer>    m_customers;
    QMap<int, QString> m_regionNames;
};

#endif // CUSTOMER_TABLE_MODEL_H
