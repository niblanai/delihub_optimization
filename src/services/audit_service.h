#ifndef AUDIT_SERVICE_H
#define AUDIT_SERVICE_H

#include "core/audit_log_entry.h"
#include "data/irepositories.h"
#include <QString>
#include <QDateTime>

// Lightweight singleton for writing audit log entries from anywhere in the app.
// Usage: AuditService::instance().log("Create", "Order", orderId, "details...");
class AuditService {
public:
    static AuditService& instance();

    // Must be called once after DB is open (e.g. in main.cpp after login)
    void init(IAuditLogRepository* repo);

    // Log an action. userId/username are pulled from SessionManager automatically.
    void log(const QString& action,
             const QString& entityType,
             int            entityId   = 0,
             const QString& details    = QString());

    // Convenience wrappers
    void logCreate(const QString& entity, int id, const QString& details = {});
    void logUpdate(const QString& entity, int id, const QString& details = {});
    void logDelete(const QString& entity, int id, const QString& details = {});
    void logLogin (const QString& username);
    void logLogout(const QString& username);

private:
    AuditService() = default;
    IAuditLogRepository* m_repo = nullptr;
};

#endif // AUDIT_SERVICE_H
