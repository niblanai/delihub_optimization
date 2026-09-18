#ifndef USER_H
#define USER_H

#include "person.h"

class User : public Person {
public:
    User() = default;
    User(int id, const QString& name) : Person(id, name) {}

    QString username() const { return m_username; }
    void setUsername(const QString& username) { m_username = username; }

    QString passwordHash() const { return m_passwordHash; }
    void setPasswordHash(const QString& hash) { m_passwordHash = hash; }

    QString passwordSalt() const { return m_passwordSalt; }
    void setPasswordSalt(const QString& salt) { m_passwordSalt = salt; }

    int roleId() const { return m_roleId; }
    void setRoleId(int id) { m_roleId = id; }

    QString phone() const { return m_phone; }
    void setPhone(const QString& phone) { m_phone = phone; }

    QString email() const { return m_email; }
    void setEmail(const QString& email) { m_email = email; }

    QString photoPath() const { return m_photoPath; }
    void setPhotoPath(const QString& path) { m_photoPath = path; }

    QString fingerprintBarcode() const { return m_fingerprintBarcode; }
    void setFingerprintBarcode(const QString& barcode) { m_fingerprintBarcode = barcode; }

    double hourlyRate() const { return m_hourlyRate; }
    void setHourlyRate(double rate) { m_hourlyRate = rate; }

    int weeklyOffDays() const { return m_weeklyOffDays; }
    void setWeeklyOffDays(int days) { m_weeklyOffDays = days; }

private:
    QString m_username;
    QString m_passwordHash;
    QString m_passwordSalt;
    int m_roleId = 0;
    QString m_phone;
    QString m_email;
    QString m_photoPath;
    QString m_fingerprintBarcode;
    double m_hourlyRate = 0.0;
    int m_weeklyOffDays = 1; // Default: 1 day off per week
};

#endif // USER_H
