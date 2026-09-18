#include "order_table_model.h"
#include "services/lang_manager.h"
#include "infra/config_manager.h"
#include <QColor>
#include <QBrush>
#include <QFont>

OrderTableModel::OrderTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int OrderTableModel::rowCount(const QModelIndex&) const { return m_orders.size(); }
int OrderTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant OrderTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_orders.size()) return {};

    const Order&  o   = m_orders.at(index.row());
    const QString sym = ConfigManager::instance().currencySymbol();
    const QString status = o.status().isEmpty() ? "Pending" : o.status();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColId:
            return o.id();
        case ColInvoice:
            return o.invoiceNumber() > 0
                ? QString("INV-%1").arg(o.invoiceNumber(), 4, 10, QChar('0'))
                : QString("INV-%1").arg(o.id(), 4, 10, QChar('0'));
        case ColCustomer:
            return o.customerName().isEmpty()
                ? m_customerNames.value(o.customerId(), QString("ID %1").arg(o.customerId()))
                : o.customerName();
        case ColDate:
            return o.dateTime().toString("yyyy-MM-dd  hh:mm");
        case ColStatus:
            return status;
        case ColPayment: {
            QString pm = o.paymentMethod().isEmpty() ? "Cash" : o.paymentMethod();
            if (pm == "Other" && !o.paymentOtherDetail().isEmpty())
                pm = o.paymentOtherDetail();
            return pm;
        }
        case ColItems:
            return o.items().size();
        case ColGrandTotal:
            return QString("%1 %2").arg(QString::number(o.grandTotal(), 'f', 2), sym);
        }
    }

    // ── Status badge — foreground color ──────────────────────────────────────
    if (role == Qt::ForegroundRole && index.column() == ColStatus) {
        if (status == "Delivered")         return QColor("#22C55E");
        if (status == "Cancelled")         return QColor("#EF4444");
        if (status == "Out for Delivery")  return QColor("#3B82F6");
        return QColor("#F59E0B"); // Pending
    }

    // ── Status badge — background tint ───────────────────────────────────────
    if (role == Qt::BackgroundRole && index.column() == ColStatus) {
        if (status == "Delivered")         return QColor("#14532D");   // dark green
        if (status == "Cancelled")         return QColor("#450A0A");   // dark red
        if (status == "Out for Delivery")  return QColor("#1E3A8A");   // dark blue
        if (status == "Pending")           return QColor("#451A03");   // dark amber
        return QVariant();
    }

    // ── Payment color ─────────────────────────────────────────────────────────
    if (role == Qt::ForegroundRole && index.column() == ColPayment) {
        const QString pm = o.paymentMethod();
        if (pm == "Visa")  return QColor("#60A5FA");
        if (pm == "Cash")  return QColor("#4ADE80");
        return QColor("#FB923C"); // Other
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColInvoice: case ColStatus: case ColPayment:
        case ColItems: case ColGrandTotal:
            return int(Qt::AlignCenter);
        }
    }

    if (role == Qt::FontRole && index.column() == ColStatus) {
        QFont f; f.setBold(true); f.setPointSize(9); return f;
    }

    if (role == Qt::UserRole) return o.id();

    // Qt::UserRole+1 — raw sortable value
    if (role == Qt::UserRole + 1) {
        switch (index.column()) {
        case ColId:         return o.id();
        case ColInvoice:    return o.invoiceNumber() > 0 ? o.invoiceNumber() : o.id();
        case ColCustomer:   return (o.customerName().isEmpty()
                                ? m_customerNames.value(o.customerId(), "")
                                : o.customerName()).toLower();
        case ColDate:       return o.dateTime().isValid()
                                ? (double)o.dateTime().toMSecsSinceEpoch() : 0.0;
        case ColStatus:     return o.status().toLower();
        case ColPayment:    return o.paymentMethod().toLower();
        case ColItems:      return o.items().size();
        case ColGrandTotal: return o.grandTotal();
        }
    }

    return {};
}

QVariant OrderTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    auto& L = LangManager::instance();
    switch (section) {
    case ColId:         return "#";
    case ColInvoice:    return L.t("Invoice");
    case ColCustomer:   return L.t("Customer");
    case ColDate:       return L.t("Date & Time");
    case ColStatus:     return L.t("Status");
    case ColPayment:    return L.t("Payment");
    case ColItems:      return L.t("Items");
    case ColGrandTotal: return L.t("Total");
    }
    return {};
}

Qt::ItemFlags OrderTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void OrderTableModel::setOrders(const QList<Order>& orders) {
    beginResetModel(); m_orders = orders; endResetModel();
}

void OrderTableModel::setCustomerMap(const QMap<int, QString>& map) {
    m_customerNames = map;
    if (!m_orders.isEmpty())
        emit dataChanged(index(0, ColCustomer), index(m_orders.size()-1, ColCustomer));
}

void OrderTableModel::addOrder(const Order& order) {
    beginInsertRows({}, 0, 0); m_orders.prepend(order); endInsertRows();
}

void OrderTableModel::updateOrder(int row, const Order& order) {
    if (row < 0 || row >= m_orders.size()) return;
    m_orders[row] = order;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void OrderTableModel::removeOrder(int row) {
    if (row < 0 || row >= m_orders.size()) return;
    beginRemoveRows({}, row, row); m_orders.removeAt(row); endRemoveRows();
}

const Order& OrderTableModel::orderAt(int row) const { return m_orders.at(row); }
