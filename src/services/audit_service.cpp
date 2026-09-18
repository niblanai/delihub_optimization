#include "audit_service.h"
#include "services/session_manager.h"
#include "infra/logger.h"

AuditService& AuditService::instance() {
    static AuditService inst;
    return inst;
}

void AuditService::init(IAuditLogRepository* repo) {
    m_repo = repo;
}

void AuditService::log(const QString& action,
                        const QString& entityType,
                        int            entityId,
                        const QString& details)
{
    if (!m_repo) return;

    AuditLogEntry e;
    e.userId     = SessionManager::instance().currentUser().id();
    e.username   = SessionManager::instance().currentUser().username();
    e.action     = action;
    e.entityType = entityType;
    e.entityId   = entityId;
    e.details    = details;
    e.timestamp  = QDateTime::currentDateTime();

    if (!m_repo->addEntry(e))
        Logger::instance().warn(QString("AuditService: failed to write entry [%1 %2 #%3]")
            .arg(action, entityType).arg(entityId));
}

void AuditService::logCreate(const QString& entity, int id, const QString& details) {
    log("Create", entity, id, details);
}
void AuditService::logUpdate(const QString& entity, int id, const QString& details) {
    log("Update", entity, id, details);
}
void AuditService::logDelete(const QString& entity, int id, const QString& details) {
    log("Delete", entity, id, details);
}
void AuditService::logLogin(const QString& username) {
    // For login, userId may not be set yet — write directly
    if (!m_repo) return;
    AuditLogEntry e;
    e.username   = username;
    e.action     = "Login";
    e.entityType = "Session";
    e.details    = QString("User '%1' logged in").arg(username);
    e.timestamp  = QDateTime::currentDateTime();
    m_repo->addEntry(e);
}
void AuditService::logLogout(const QString& username) {
    if (!m_repo) return;
    AuditLogEntry e;
    e.userId     = SessionManager::instance().currentUser().id();
    e.username   = username;
    e.action     = "Logout";
    e.entityType = "Session";
    e.details    = QString("User '%1' logged out").arg(username);
    e.timestamp  = QDateTime::currentDateTime();
    m_repo->addEntry(e);
}
