#ifndef REGISTER_SESSION_H
#define REGISTER_SESSION_H

#include <QString>
#include <QDateTime>

class RegisterSession {
public:
    enum class Status { Open, Closed };

    RegisterSession() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int userId() const { return m_userId; }
    void setUserId(int uid) { m_userId = uid; }

    double openingCash() const { return m_openingCash; }
    void setOpeningCash(double amount) { m_openingCash = amount; }

    double closingCash() const { return m_closingCash; }
    void setClosingCash(double amount) { m_closingCash = amount; }

    QDateTime openedAt() const { return m_openedAt; }
    void setOpenedAt(const QDateTime& dt) { m_openedAt = dt; }

    QDateTime closedAt() const { return m_closedAt; }
    void setClosedAt(const QDateTime& dt) { m_closedAt = dt; }

    Status status() const { return m_status; }
    void setStatus(Status s) { m_status = s; }

    // Helpers
    bool isOpen() const { return m_status == Status::Open; }
    double difference() const { return m_closingCash - m_openingCash; }

    static QString statusToString(Status s) {
        return (s == Status::Closed) ? "Closed" : "Open";
    }
    static Status stringToStatus(const QString& s) {
        return (s.compare("Closed", Qt::CaseInsensitive) == 0) ? Status::Closed : Status::Open;
    }

private:
    int       m_id          = 0;
    int       m_userId      = 0;
    double    m_openingCash = 0.0;
    double    m_closingCash = 0.0;
    QDateTime m_openedAt;
    QDateTime m_closedAt;
    Status    m_status      = Status::Open;
};

#endif // REGISTER_SESSION_H
