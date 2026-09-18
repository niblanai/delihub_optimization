#ifndef DELIVERY_DRIVER_H
#define DELIVERY_DRIVER_H

#include <QString>

class DeliveryDriver {
public:
    DeliveryDriver() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    QString phone() const { return m_phone; }
    void setPhone(const QString& p) { m_phone = p; }

    QString nationalId() const { return m_nationalId; }
    void setNationalId(const QString& n) { m_nationalId = n; }

    bool active() const { return m_active; }
    void setActive(bool a) { m_active = a; }

private:
    int     m_id       = 0;
    QString m_name;
    QString m_phone;
    QString m_nationalId;
    bool    m_active   = true;
};

#endif // DELIVERY_DRIVER_H
