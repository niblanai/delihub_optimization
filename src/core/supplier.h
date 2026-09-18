#ifndef SUPPLIER_H
#define SUPPLIER_H

#include <QString>

class Supplier {
public:
    Supplier() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString contactName() const { return m_contactName; }
    void setContactName(const QString& cn) { m_contactName = cn; }

    QString phone() const { return m_phone; }
    void setPhone(const QString& p) { m_phone = p; }

    QString email() const { return m_email; }
    void setEmail(const QString& e) { m_email = e; }

    QString address() const { return m_address; }
    void setAddress(const QString& a) { m_address = a; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& n) { m_notes = n; }

    bool active() const { return m_active; }
    void setActive(bool a) { m_active = a; }

private:
    int     m_id          = 0;
    QString m_name;
    QString m_contactName;
    QString m_phone;
    QString m_email;
    QString m_address;
    QString m_notes;
    bool    m_active      = true;
};

#endif // SUPPLIER_H
