#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "person.h"
#include "phone.h"
#include "address.h"
#include <QDateTime>

class Customer : public Person {
public:
    enum class Status {
        Active,
        Hesitant,   // Task 9: no order in last 7 days
        Inactive,
        Suspended
    };

    Customer() = default;
    Customer(int id, const QString& name) : Person(id, name) {}

    double distanceKm() const { return m_distanceKm; }
    void setDistanceKm(double distance) { m_distanceKm = distance; }

    int regionId() const { return m_regionId; }
    void setRegionId(int id) { m_regionId = id; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    // Preferred payment method (auto-fills OrderDialog)
    QString preferredPaymentMethod() const { return m_preferredPaymentMethod; }
    void setPreferredPaymentMethod(const QString& m) { m_preferredPaymentMethod = m; }

    QString preferredPaymentOther() const { return m_preferredPaymentOther; }
    void setPreferredPaymentOther(const QString& v) { m_preferredPaymentOther = v; }

    QDateTime firstContactDate() const { return m_firstContactDate; }
    void setFirstContactDate(const QDateTime& date) { m_firstContactDate = date; }

    QDateTime lastOrderDate() const { return m_lastOrderDate; }
    void setLastOrderDate(const QDateTime& date) { m_lastOrderDate = date; }

    Status status() const { return m_status; }
    void setStatus(Status status) { m_status = status; }

    QList<Phone> phones() const { return m_phones; }
    void setPhones(const QList<Phone>& phones) { m_phones = phones; }
    void addPhone(const Phone& phone) { m_phones.append(phone); }

    QList<Address> addresses() const { return m_addresses; }
    void setAddresses(const QList<Address>& addresses) { m_addresses = addresses; }
    void addAddress(const Address& address) { m_addresses.append(address); }

    QList<int> favoriteProductIds() const { return m_favoriteProductIds; }
    void setFavoriteProductIds(const QList<int>& ids) { m_favoriteProductIds = ids; }
    void addFavoriteProductId(int id) { m_favoriteProductIds.append(id); }

    // Debt tracking
    double debt() const { return m_debt; }
    void setDebt(double debt) { m_debt = debt; }

    static QString statusToString(Status status) {
        switch (status) {
            case Status::Active:    return "Active";
            case Status::Hesitant:  return "Hesitant";
            case Status::Inactive:  return "Inactive";
            case Status::Suspended: return "Suspended";
        }
        return "Active";
    }

    static Status stringToStatus(const QString& str) {
        if (str.compare("Hesitant",  Qt::CaseInsensitive) == 0) return Status::Hesitant;
        if (str.compare("Inactive",  Qt::CaseInsensitive) == 0) return Status::Inactive;
        if (str.compare("Suspended", Qt::CaseInsensitive) == 0) return Status::Suspended;
        return Status::Active;
    }

private:
    double m_distanceKm = 0.0;
    int m_regionId = 0;
    QString m_notes;
    QString m_preferredPaymentMethod; // "Cash", "Visa", "Other"
    QString m_preferredPaymentOther;  // filled when method == "Other"
    QDateTime m_firstContactDate;
    QDateTime m_lastOrderDate;
    Status m_status = Status::Active;
    QList<Phone> m_phones;
    QList<Address> m_addresses;
    QList<int> m_favoriteProductIds;
    double m_debt = 0.0;
};

#endif // CUSTOMER_H
