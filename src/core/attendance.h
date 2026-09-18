#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include <QString>
#include <QDateTime>

class Attendance {
public:
    enum Type {
        CheckIn,
        CheckOut
    };

    Attendance() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int userId() const { return m_userId; }
    void setUserId(int userId) { m_userId = userId; }

    QString userName() const { return m_userName; }
    void setUserName(const QString& name) { m_userName = name; }

    QDateTime timestamp() const { return m_timestamp; }
    void setTimestamp(const QDateTime& timestamp) { m_timestamp = timestamp; }

    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

    QString typeString() const { 
        return m_type == CheckIn ? "IN" : "OUT"; 
    }
    
    void setTypeFromString(const QString& str) {
        m_type = (str == "IN") ? CheckIn : CheckOut;
    }

private:
    int m_id = 0;
    int m_userId = 0;
    QString m_userName;
    QDateTime m_timestamp;
    Type m_type = CheckIn;
};

#endif // ATTENDANCE_H
