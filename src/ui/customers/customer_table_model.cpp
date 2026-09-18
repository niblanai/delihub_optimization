#include "customer_table_model.h"
#include "services/lang_manager.h"
#include <QColor>

CustomerTableModel::CustomerTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int CustomerTableModel::rowCount(const QModelIndex&) const { return m_customers.size(); }
int CustomerTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant CustomerTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_customers.size()) return {};
    const Customer& c = m_customers.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColId:       return c.id();
        case ColName:     return c.name();
        case ColPhone:    return c.phones().isEmpty() ? QString() : c.phones().first().number;
        case ColRegion:   return m_regionNames.value(c.regionId(),
                              c.regionId() > 0 ? QString("#%1").arg(c.regionId()) : "—");
        case ColDistance: return QString::number(c.distanceKm(), 'f', 1) + " km";
        case ColStatus:   return Customer::statusToString(c.status());
        case ColDebt:     return c.debt() > 0 ? QString::number(c.debt(), 'f', 2) : QString("—");
        case ColAddress:  return c.addresses().isEmpty() ? QString() : c.addresses().first().text;
        case ColNotes:    return c.notes();
        }
    }

    // ── Hover tooltips: show all extras when a customer has more than one ─────
    if (role == Qt::ToolTipRole) {
        if (index.column() == ColPhone && c.phones().size() > 1) {
            QStringList all;
            for (const auto& ph : c.phones()) all << ph.number;
            return QString("All phones:\n") + all.join("\n");
        }
        if (index.column() == ColAddress && c.addresses().size() > 1) {
            QStringList all;
            for (const auto& ad : c.addresses()) all << ad.text;
            return QString("All addresses:\n") + all.join("\n");
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColDistance: case ColStatus: case ColDebt: return int(Qt::AlignCenter);
        }
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == ColStatus) {
            switch (c.status()) {
            case Customer::Status::Active:    return QColor("#4ADE80");  // green
            case Customer::Status::Hesitant:  return QColor("#FCD34D");  // amber — Task 9
            case Customer::Status::Inactive:  return QColor("#FB923C");  // orange
            case Customer::Status::Suspended: return QColor("#F87171");  // red
            }
        }
        if (index.column() == ColDebt && c.debt() > 0) {
            return QColor("#F87171");  // red for debt
        }
    }

    if (role == Qt::UserRole) return c.id();

    // Qt::UserRole+1 — raw sortable value
    if (role == Qt::UserRole + 1) {
        switch (index.column()) {
        case ColId:       return c.id();
        case ColName:     return c.name().toLower();
        case ColPhone:    return c.phones().isEmpty() ? QString() : c.phones().first().number;
        case ColRegion:   return m_regionNames.value(c.regionId(), "").toLower();
        case ColDistance: return c.distanceKm();
        case ColStatus:   return Customer::statusToString(c.status()).toLower();
        case ColDebt:     return c.debt();
        case ColAddress:  return c.addresses().isEmpty() ? QString() : c.addresses().first().text.toLower();
        case ColNotes:    return c.notes().toLower();
        }
    }

    return {};
}

QVariant CustomerTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    auto& L = LangManager::instance();
    switch (section) {
    case ColId:       return "#";
    case ColName:     return L.t("Name");
    case ColPhone:    return L.t("Phone");
    case ColRegion:   return L.t("Region");
    case ColDistance: return L.t("Distance");
    case ColStatus:   return L.t("Status");
    case ColDebt:     return L.t("Debt");
    case ColAddress:  return L.t("Address");
    case ColNotes:    return L.t("Notes");
    }
    return {};
}

Qt::ItemFlags CustomerTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void CustomerTableModel::setCustomers(const QList<Customer>& customers) {
    beginResetModel(); m_customers = customers; endResetModel();
}

void CustomerTableModel::setRegionMap(const QMap<int, QString>& map) {
    m_regionNames = map;
    if (!m_customers.isEmpty())
        emit dataChanged(index(0, ColRegion),
                         index(m_customers.size() - 1, ColRegion),
                         {Qt::DisplayRole});
}

void CustomerTableModel::addCustomer(const Customer& c) {
    beginInsertRows({}, m_customers.size(), m_customers.size());
    m_customers.append(c); endInsertRows();
}

void CustomerTableModel::updateCustomer(int row, const Customer& c) {
    if (row < 0 || row >= m_customers.size()) return;
    m_customers[row] = c;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void CustomerTableModel::removeCustomer(int row) {
    if (row < 0 || row >= m_customers.size()) return;
    beginRemoveRows({}, row, row); m_customers.removeAt(row); endRemoveRows();
}

const Customer& CustomerTableModel::customerAt(int row) const { return m_customers.at(row); }
