#ifndef PROMOTION_TABLE_MODEL_H
#define PROMOTION_TABLE_MODEL_H

#include <QAbstractTableModel>
#include "core/promotion.h"

class PromotionTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PromotionTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setPromotions(const QList<Promotion>& promotions);
    Promotion getPromotion(int row) const;
    void clear();

private:
    QString typeToString(Promotion::Type type) const;
    QString statusToString(Promotion::Status status) const;

    QList<Promotion> m_promotions;
};

#endif // PROMOTION_TABLE_MODEL_H
