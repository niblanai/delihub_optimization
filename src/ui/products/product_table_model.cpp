#include "product_table_model.h"
#include "services/lang_manager.h"
#include "infra/config_manager.h"
#include <QColor>
#include <QBrush>
#include <QFont>

ProductTableModel::ProductTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int ProductTableModel::rowCount(const QModelIndex&)  const { return m_products.size(); }
int ProductTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant ProductTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_products.size()) return {};
    const Product& p = m_products.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColId:          return p.id();
        case ColName:        return p.name();
        case ColBarcode:     return p.barcode().isEmpty() ? "—" : p.barcode();
        case ColCategory:    return m_categoryNames.value(p.categoryId(), "Unassigned");
        case ColPrice:       return QString("%1 %2")
                                .arg(QString::number(p.price(), 'f', 2),
                                     ConfigManager::instance().currencySymbol());
        case ColStock:       return p.stockQty();
        case ColThreshold:   return p.lowStockThreshold();
        case ColStatus:      return Product::statusToString(p.status());
        case ColManufacture: return p.manufactureDate().isValid()
                                ? p.manufactureDate().toString("yyyy-MM-dd") : "—";
        case ColExpiry:      return p.expiryDate().isValid()
                                ? p.expiryDate().toString("yyyy-MM-dd") : "—";
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColPrice: case ColStatus: case ColBarcode:
        case ColManufacture: case ColExpiry: case ColStock: case ColThreshold:
            return int(Qt::AlignCenter);
        }
    }

    // Highlight expired products in red
    if (role == Qt::ForegroundRole && index.column() == ColExpiry) {
        if (p.expiryDate().isValid() && p.expiryDate() < QDate::currentDate())
            return QColor("#F87171");
    }

    // Highlight low-stock rows with a red background tint
    if (role == Qt::BackgroundRole && p.isLowStock() && p.lowStockThreshold() > 0) {
        return QColor("#3D1515"); // dark red tint — visible in both dark and light themes
    }
    if (role == Qt::ForegroundRole && p.isLowStock() && p.lowStockThreshold() > 0) {
        // Tint the Stock column text bright red so it stands out
        if (index.column() == ColStock)
            return QColor("#F87171");
    }

    if (role == Qt::UserRole) return p.id();

    // Qt::UserRole+1 — raw sortable value for QSortFilterProxyModel
    if (role == Qt::UserRole + 1) {
        switch (index.column()) {
        case ColId:          return p.id();
        case ColName:        return p.name().toLower();
        case ColBarcode:     return p.barcode().toLower();
        case ColCategory:    return m_categoryNames.value(p.categoryId(), "").toLower();
        case ColPrice:       return p.price();
        case ColStock:       return p.stockQty();
        case ColThreshold:   return p.lowStockThreshold();
        case ColStatus:      return Product::statusToString(p.status()).toLower();
        case ColManufacture: return p.manufactureDate().isValid()
                                ? (double)p.manufactureDate().toJulianDay() : 0.0;
        case ColExpiry:      return p.expiryDate().isValid()
                                ? (double)p.expiryDate().toJulianDay() : 0.0;
        }
    }

    return {};
}

QVariant ProductTableModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    auto& L = LangManager::instance();
    switch (section) {
    case ColId:          return "#";
    case ColName:        return L.t("Product Name");
    case ColBarcode:     return L.t("Barcode");
    case ColCategory:    return L.t("Category");
    case ColPrice:       return L.t("Price");
    case ColStock:       return L.t("Quantity");
    case ColThreshold:   return L.t("Safety Margin");
    case ColStatus:      return L.t("Status");
    case ColManufacture: return L.t("Manufacture Date");
    case ColExpiry:      return L.t("Expiry Date");
    }
    return {};
}

Qt::ItemFlags ProductTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ProductTableModel::setProducts(const QList<Product>& products) {
    beginResetModel(); m_products = products; endResetModel();
}

void ProductTableModel::setCategoryMap(const QMap<int, QString>& map) {
    beginResetModel(); m_categoryNames = map; endResetModel();
}

void ProductTableModel::addProduct(const Product& p) {
    beginInsertRows({}, m_products.size(), m_products.size());
    m_products.append(p); endInsertRows();
}

void ProductTableModel::updateProduct(int row, const Product& p) {
    if (row < 0 || row >= m_products.size()) return;
    m_products[row] = p;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void ProductTableModel::removeProduct(int row) {
    if (row < 0 || row >= m_products.size()) return;
    beginRemoveRows({}, row, row); m_products.removeAt(row); endRemoveRows();
}

const Product& ProductTableModel::productAt(int row) const { return m_products.at(row); }
