#include "notifications_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

NotificationsPanel::NotificationsPanel(QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName("notificationsPanel");
    setStyleSheet(
        "QFrame#notificationsPanel { background:#1E293B; border:1px solid #334155;"
        " border-radius:10px; }");
    setMinimumWidth(320);
    setMaximumHeight(400);
    setupUi();
}

void NotificationsPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel("🔔 Notifications");
    title->setStyleSheet("font-weight:bold; font-size:14px; color:#E2E8F0;");
    m_clearBtn = new QPushButton("Clear All");
    m_clearBtn->setObjectName("secondaryBtn");
    m_clearBtn->setFixedHeight(24);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_clearBtn);
    root->addLayout(header);

    m_list = new QListWidget;
    m_list->setObjectName("dataTable");
    m_list->setStyleSheet(
        "QListWidget { background:#0F172A; border:none; border-radius:6px; }"
        "QListWidget::item { padding:8px; color:#E2E8F0; border-bottom:1px solid #1E293B; }"
        "QListWidget::item:selected { background:#0EA5E9; }");
    root->addWidget(m_list);

    m_emptyLabel = new QLabel("No notifications");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color:#64748B; font-size:12px; padding:20px;");
    root->addWidget(m_emptyLabel);

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        NotificationService::instance().clearPending();
        refresh();
    });
}

void NotificationsPanel::refresh() {
    m_list->clear();
    const auto& pending = NotificationService::instance().pendingNotifications();

    bool any = !pending.isEmpty();
    m_list->setVisible(any);
    m_emptyLabel->setVisible(!any);
    m_clearBtn->setEnabled(any);

    for (int i = pending.size() - 1; i >= 0; --i) {
        const auto& n = pending.at(i);
        QString icon;
        switch (n.event) {
        case NotificationEvent::ScheduledOrderDueToday: icon = "🔁 "; break;
        case NotificationEvent::LowStock:               icon = "⚠️ "; break;
        case NotificationEvent::CustomerInactive:       icon = "😴 "; break;
        default:                                         icon = "🔔 "; break;
        }
        auto* item = new QListWidgetItem(icon + n.title + "\n" + n.message);
        item->setToolTip(n.message);
        m_list->addItem(item);
    }

    emit unreadCountChanged(pending.size());
}

int NotificationsPanel::unreadCount() const {
    return NotificationService::instance().pendingNotifications().size();
}
