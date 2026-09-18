#ifndef STOCK_COUNT_H
#define STOCK_COUNT_H

#include <QString>
#include <QDate>

class StockCount {
public:
    enum class Status { Draft, Confirmed };

    StockCount() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QDate date() const { return m_date; }
    void setDate(const QDate& d) { m_date = d; }

    int userId() const { return m_userId; }
    void setUserId(int uid) { m_userId = uid; }

    Status status() const { return m_status; }
    void setStatus(Status s) { m_status = s; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& n) { m_notes = n; }

    // Helpers
    bool isDraft() const { return m_status == Status::Draft; }
    bool isConfirmed() const { return m_status == Status::Confirmed; }

    static QString statusToString(Status s) {
        return (s == Status::Confirmed) ? "Confirmed" : "Draft";
    }
    static Status stringToStatus(const QString& s) {
        return (s.compare("Confirmed", Qt::CaseInsensitive) == 0) ? Status::Confirmed : Status::Draft;
    }

private:
    int     m_id     = 0;
    QDate   m_date;
    int     m_userId = 0;
    Status  m_status = Status::Draft;
    QString m_notes;
};

#endif // STOCK_COUNT_H
