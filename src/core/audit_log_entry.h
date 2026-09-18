#ifndef AUDIT_LOG_ENTRY_H
#define AUDIT_LOG_ENTRY_H

#include <QString>
#include <QDateTime>

struct AuditLogEntry {
    int id = 0;
    int userId = 0;
    QString username; // Cached for easy display without join
    QString action;   // e.g. "Create", "Update", "Delete", "Backup", "Restore"
    QString entityType; // e.g. "Customer", "Product", "Order"
    int entityId = 0;
    QString details;
    QDateTime timestamp;
};

#endif // AUDIT_LOG_ENTRY_H
