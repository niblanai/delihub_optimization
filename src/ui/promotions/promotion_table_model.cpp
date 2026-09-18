#include "promotion_table_model.h"
#include "services/lang_manager.h"
#include <QBrush>
#include <QColor>

PromotionTableModel::PromotionTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int PromotionTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_promotions.size();
}

int PromotionTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 8; // Title, Type, DiscountValue, StartDate, EndDate, Status, Product, Category
}

QVariant PromotionTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_promotions.size())
        return QVariant();

    const Promotion& promo = m_promotions[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return promo.title;
        case 1: return typeToString(promo.type);
        case 2: 
            if (promo.type == Promotion::Type::Percentage)
                return QString::number(promo.discountValue) + "%";
            else
                return QString::number(promo.discountValue, 'f', 2);
        case 3: return promo.startDate.toString("yyyy-MM-dd");
        case 4: return promo.endDate.toString("yyyy-MM-dd");
        case 5: return statusToString(promo.status);
        case 6: return promo.productId > 0 ? QString::number(promo.productId) : "-";
        case 7: return promo.categoryId > 0 ? QString::number(promo.categoryId) : "-";
        }
    } else if (role == Qt::ForegroundRole) {
        // Color based on status
        if (index.column() == 5) {
            if (promo.status == Promotion::Status::Active) {
                return QBrush(QColor(34, 139, 34)); // Green
            } else if (promo.status == Promotion::Status::Inactive) {
                return QBrush(QColor(220, 20, 60)); // Red
            } else if (promo.status == Promotion::Status::Scheduled) {
                return QBrush(QColor(255, 165, 0)); // Orange
            }
        }
    }

    return QVariant();
}

QVariant PromotionTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
    case 0: return LangManager::tr("promotion_title");
    case 1: return LangManager::tr("promotion_type");
    case 2: return LangManager::tr("promotion_discount");
    case 3: return LangManager::tr("promotion_start_date");
    case 4: return LangManager::tr("promotion_end_date");
    case 5: return LangManager::tr("promotion_status");
    case 6: return LangManager::tr("promotion_product");
    case 7: return LangManager::tr("promotion_category");
    default: return QVariant();
    }
}

void PromotionTableModel::setPromotions(const QList<Promotion>& promotions) {
    beginResetModel();
    m_promotions = promotions;
    endResetModel();
}

Promotion PromotionTableModel::getPromotion(int row) const {
    if (row >= 0 && row < m_promotions.size())
        return m_promotions[row];
    return Promotion();
}

void PromotionTableModel::clear() {
    beginResetModel();
    m_promotions.clear();
    endResetModel();
}

QString PromotionTableModel::typeToString(Promotion::Type type) const {
    switch (type) {
    case Promotion::Type::Percentage: return LangManager::tr("promotion_type_percentage");
    case Promotion::Type::FixedAmount: return LangManager::tr("promotion_type_fixed");
    case Promotion::Type::BuyXGetY: return LangManager::tr("promotion_type_bogo");
    case Promotion::Type::FreeShipping: return LangManager::tr("promotion_type_freeship");
    default: return "";
    }
}

QString PromotionTableModel::statusToString(Promotion::Status status) const {
    switch (status) {
    case Promotion::Status::Active: return LangManager::tr("promotion_status_active");
    case Promotion::Status::Inactive: return LangManager::tr("promotion_status_inactive");
    case Promotion::Status::Scheduled: return LangManager::tr("promotion_status_scheduled");
    case Promotion::Status::Expired: return LangManager::tr("promotion_status_expired");
    default: return "";
    }
}
