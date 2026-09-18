#pragma once
#include <QSortFilterProxyModel>
#include <QCollator>
#include <QLocale>
#include <QVariant>

// ─────────────────────────────────────────────────────────────────────────────
// ArabicSortProxy
//
// QSortFilterProxyModel that:
//  • Reads Qt::UserRole+1 for sort values (raw numerics / lower-case strings)
//  • Uses QCollator with Arabic locale so Arabic text sorts alphabetically
//    before/after Latin characters correctly, not by Unicode code point.
//  • Falls back to standard numeric comparison for int/double variants.
// ─────────────────────────────────────────────────────────────────────────────
class ArabicSortProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ArabicSortProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        // Arabic locale collation: صح before كلمة etc.
        m_collator.setLocale(QLocale(QLocale::Arabic, QLocale::UnitedArabEmirates));
        m_collator.setCaseSensitivity(Qt::CaseInsensitive);
        m_collator.setNumericMode(true);   // "10" > "9" etc.
        setDynamicSortFilter(true);
        setSortRole(Qt::UserRole + 1);
    }

protected:
    bool lessThan(const QModelIndex& left,
                  const QModelIndex& right) const override
    {
        QVariant lv = sourceModel()->data(left,  Qt::UserRole + 1);
        QVariant rv = sourceModel()->data(right, Qt::UserRole + 1);

        // Numeric types — direct comparison
        if (lv.typeId() == QMetaType::Double || lv.typeId() == QMetaType::Int ||
            lv.typeId() == QMetaType::LongLong) {
            return lv.toDouble() < rv.toDouble();
        }

        // QString — use Arabic-aware collator
        if (lv.typeId() == QMetaType::QString) {
            QString ls = lv.toString();
            QString rs = rv.toString();

            // Empty strings go to the end regardless of sort order
            if (ls.isEmpty() && !rs.isEmpty()) return false;
            if (!ls.isEmpty() && rs.isEmpty()) return true;

            return m_collator.compare(ls, rs) < 0;
        }

        // Fallback
        return QSortFilterProxyModel::lessThan(left, right);
    }

private:
    QCollator m_collator;
};
