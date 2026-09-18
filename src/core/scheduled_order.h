#ifndef SCHEDULED_ORDER_H
#define SCHEDULED_ORDER_H

#include "order_item.h"
#include <QTime>
#include <QDate>
#include <QList>

struct ScheduledOrder {
    int id = 0;
    int customerId = 0;
    QString customerName; // Cached
    QString weekdays;    // Comma-separated list of integers (1 = Monday, 7 = Sunday) e.g., "1,3,5"
                         // Ignored when isRecurring == false
    QTime time;
    QList<OrderItem> items;

    // ── Task 5: Repeat toggle ──────────────────────────────────────────────────
    bool  isRecurring   = true;    // true = recurring on weekdays, false = one-time date
    QDate oneTimeDate;             // valid only when isRecurring == false

    // ── Per-schedule reminder settings ────────────────────────────────────────
    int  remindMinutesBefore  = 60;  // notify X minutes before delivery time
    int  remindRepeatInterval = 0;   // repeat reminder every N minutes (0 = once only)
    bool autoCreateOrder      = false; // automatically create pending order

    // Helper to check if scheduled for a specific day of week (recurring mode only)
    bool isScheduledFor(int dayOfWeek) const {
        if (!isRecurring) return false;
        QStringList days = weekdays.split(',', Qt::SkipEmptyParts);
        return days.contains(QString::number(dayOfWeek));
    }

    // Helper to check if this one-time order is due today
    bool isDueToday() const {
        if (isRecurring) return false;
        return oneTimeDate == QDate::currentDate();
    }
};

#endif // SCHEDULED_ORDER_H
