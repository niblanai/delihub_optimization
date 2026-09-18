#ifndef EXPENSE_CATEGORY_H
#define EXPENSE_CATEGORY_H

#include <QString>

class ExpenseCategory {
public:
    ExpenseCategory() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString description() const { return m_description; }
    void setDescription(const QString& d) { m_description = d; }

private:
    int     m_id = 0;
    QString m_name;
    QString m_description;
};

#endif // EXPENSE_CATEGORY_H
