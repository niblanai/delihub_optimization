#ifndef NOTIFICATION_SERVICE_H
#define NOTIFICATION_SERVICE_H

#include <QString>
#include <QList>
#include <functional>

// ── Event types the service can emit ────────────────────────────────────────
enum class NotificationEvent {
    ScheduledOrderDueToday,   // payload: customer name + time
    LowStock,                 // payload: product name
    CustomerInactive,         // payload: customer name
};

struct Notification {
    NotificationEvent event;
    QString           title;
    QString           message;
};

// ── Observer (subscriber) typedef ────────────────────────────────────────────
using NotificationHandler = std::function<void(const Notification&)>;

// ── Service ──────────────────────────────────────────────────────────────────
// Pure Observer pattern: producers call emit(), consumers call subscribe().
// The service knows nothing about Qt widgets — it just holds a list of handlers.
class NotificationService {
public:
    static NotificationService& instance();

    // Subscribe to all notifications (returns a handle to unsubscribe later)
    int subscribe(NotificationHandler handler);
    void unsubscribe(int handle);

    // Emit a notification to all current subscribers
    void dispatch(const Notification& n);
    void dispatch(NotificationEvent event, const QString& title, const QString& message);

    // Convenience: fetch notifications emitted since last clearPending()
    QList<Notification> pendingNotifications() const;
    void                clearPending();

private:
    NotificationService() = default;

    struct Entry {
        int                 handle;
        NotificationHandler handler;
    };

    QList<Entry>         m_subscribers;
    QList<Notification>  m_pending;
    int                  m_nextHandle = 1;
};

#endif // NOTIFICATION_SERVICE_H
