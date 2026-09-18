#ifndef EXPENSE_H
#define EXPENSE_H

#include <QString>
#include <QDate>

class Expense {
public:
    Expense() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int categoryId() const { return m_categoryId; }
    void setCategoryId(int cid) { m_categoryId = cid; }

    double amount() const { return m_amount; }
    void setAmount(double a) { m_amount = a; }

    QString description() const { return m_description; }
    void setDescription(const QString& d) { m_description = d; }

    QDate date() const { return m_date; }
    void setDate(const QDate& d) { m_date = d; }

    int userId() const { return m_userId; }
    void setUserId(int uid) { m_userId = uid; }

    int registerSessionId() const { return m_registerSessionId; }
    void setRegisterSessionId(int sid) { m_registerSessionId = sid; }

    QString paymentMethod() const { return m_paymentMethod; }
    void setPaymentMethod(const QString& method) { m_paymentMethod = method; }

private:
    int     m_id                = 0;
    int     m_categoryId        = 0;
    double  m_amount            = 0.0;
    QString m_description;
    QDate   m_date;
    int     m_userId            = 0;
    int     m_registerSessionId = 0;
    QString m_paymentMethod     = "Cash";  // Cash, Card, or custom
};

#endif // EXPENSE_H
