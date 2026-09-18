#ifndef COUPON_H
#define COUPON_H

#include <QString>
#include <QDate>

class Coupon {
public:
    enum class Type { Percentage, FixedAmount };

    Coupon() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString code() const { return m_code; }
    void setCode(const QString& c) { m_code = c.toUpper().trimmed(); }

    QString description() const { return m_description; }
    void setDescription(const QString& d) { m_description = d; }

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    double value() const { return m_value; }   // % or fixed amount
    void setValue(double v) { m_value = v; }

    QDate expiryDate() const { return m_expiryDate; }
    void setExpiryDate(const QDate& d) { m_expiryDate = d; }

    int maxUses() const { return m_maxUses; }    // 0 = unlimited
    void setMaxUses(int n) { m_maxUses = n; }

    int usedCount() const { return m_usedCount; }
    void setUsedCount(int n) { m_usedCount = n; }

    bool isActive() const { return m_active; }
    void setActive(bool a) { m_active = a; }

    bool isValid() const {
        if (!m_active) return false;
        if (m_expiryDate.isValid() && m_expiryDate < QDate::currentDate()) return false;
        if (m_maxUses > 0 && m_usedCount >= m_maxUses) return false;
        return true;
    }

    // Calculate discount amount for a given order subtotal
    double calculateDiscount(double subtotal) const {
        if (!isValid()) return 0.0;
        if (m_type == Type::Percentage)
            return subtotal * (m_value / 100.0);
        return qMin(m_value, subtotal);   // fixed amount, can't exceed subtotal
    }

    static QString typeToString(Type t) { return t == Type::Percentage ? "Percentage" : "FixedAmount"; }
    static Type stringToType(const QString& s) {
        return s == "Percentage" ? Type::Percentage : Type::FixedAmount;
    }

private:
    int     m_id          = 0;
    QString m_code;
    QString m_description;
    Type    m_type        = Type::FixedAmount;
    double  m_value       = 0.0;
    QDate   m_expiryDate;
    int     m_maxUses     = 0;
    int     m_usedCount   = 0;
    bool    m_active      = true;
};

#endif // COUPON_H
