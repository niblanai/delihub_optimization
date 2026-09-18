#include "notification_service.h"

NotificationService& NotificationService::instance() {
    static NotificationService inst;
    return inst;
}

int NotificationService::subscribe(NotificationHandler handler) {
    int handle = m_nextHandle++;
    m_subscribers.append({handle, handler});
    return handle;
}

void NotificationService::unsubscribe(int handle) {
    for (int i = 0; i < m_subscribers.size(); ++i) {
        if (m_subscribers[i].handle == handle) {
            m_subscribers.removeAt(i);
            return;
        }
    }
}

void NotificationService::dispatch(const Notification& n) {
    m_pending.append(n);
    for (const auto& entry : m_subscribers)
        entry.handler(n);
}

void NotificationService::dispatch(NotificationEvent event,
                               const QString& title,
                               const QString& message) {
    dispatch(Notification{event, title, message});
}

QList<Notification> NotificationService::pendingNotifications() const {
    return m_pending;
}

void NotificationService::clearPending() {
    m_pending.clear();
}
