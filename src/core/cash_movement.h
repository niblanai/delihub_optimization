#ifndef CASH_MOVEMENT_H
#define CASH_MOVEMENT_H

#include <QString>
#include <QDateTime>

class CashMovement {
public:
    enum class Type { CashIn, CashOut };

    CashMovement() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int registerSessionId() const { return m_registerSessionId; }
    void setRegisterSessionId(int sid) { m_registerSessionId = sid; }

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    double amount() const { return m_amount; }
    void setAmount(double a) { m_amount = a; }

    QString reason() const { return m_reason; }
    void setReason(const QString& r) { m_reason = r; }

    QDateTime dateTime() const { return m_dateTime; }
    void setDateTime(const QDateTime& dt) { m_dateTime = dt; }

    int userId() const { return m_userId; }
    void setUserId(int uid) { m_userId = uid; }

    // Helpers
    bool isCashIn() const { return m_type == Type::CashIn; }
    bool isCashOut() const { return m_type == Type::CashOut; }

    static QString typeToString(Type t) {
        return (t == Type::CashOut) ? "CashOut" : "CashIn";
    }
    static Type stringToType(const QString& s) {
        return (s.compare("CashOut", Qt::CaseInsensitive) == 0) ? Type::CashOut : Type::CashIn;
    }

private:
    int       m_id                = 0;
    int       m_registerSessionId = 0;
    Type      m_type              = Type::CashIn;
    double    m_amount            = 0.0;
    QString   m_reason;
    QDateTime m_dateTime;
    int       m_userId            = 0;
};

#endif // CASH_MOVEMENT_H
