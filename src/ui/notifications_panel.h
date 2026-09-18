#ifndef NOTIFICATIONS_PANEL_H
#define NOTIFICATIONS_PANEL_H

#include <QFrame>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include "services/notification_service.h"

// Floating panel that shows all pending notifications
class NotificationsPanel : public QFrame {
    Q_OBJECT
public:
    explicit NotificationsPanel(QWidget* parent = nullptr);
    void refresh();
    int unreadCount() const;

signals:
    void unreadCountChanged(int count);

private:
    void setupUi();
    QListWidget* m_list          = nullptr;
    QPushButton* m_clearBtn      = nullptr;
    QLabel*      m_emptyLabel    = nullptr;
};

#endif // NOTIFICATIONS_PANEL_H
