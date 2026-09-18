#include "scheduling_service.h"
#include "notification_service.h"
#include "infra/logger.h"
#include <QDate>

SchedulingService::SchedulingService(IScheduledOrderRepository* repo,
                                     ICustomerRepository*       customerRepo)
    : m_repo(repo), m_customerRepo(customerRepo) {}

QList<ScheduledOrder> SchedulingService::checkDueToday() {
    m_dueToday.clear();

    int todayDow = QDate::currentDate().dayOfWeek();

    const QList<ScheduledOrder> all = m_repo->getAll();
    for (const auto& s : all) {
        // Task 5: handle both recurring and one-time orders
        bool dueToday = s.isRecurring
            ? s.isScheduledFor(todayDow)
            : s.isDueToday();

        if (!dueToday) continue;
        m_dueToday.append(s);

        QString custName = s.customerName;
        if (custName.isEmpty() && m_customerRepo) {
            Customer c = m_customerRepo->getById(s.customerId);
            custName = c.name();
        }

        QString timeStr = s.time.isValid()
                              ? s.time.toString("hh:mm")
                              : "any time";

        NotificationService::instance().dispatch(
            NotificationEvent::ScheduledOrderDueToday,
            "Scheduled Order Due Today",
            QString("Customer: %1  |  Time: %2").arg(custName, timeStr));

        Logger::instance().info(
            QString("Scheduled order due today — customer: %1, time: %2")
                .arg(custName, timeStr));
    }

    Logger::instance().info(
        QString("Scheduling check complete — %1 order(s) due today").arg(m_dueToday.size()));

    return m_dueToday;
}

QList<ScheduledOrder> SchedulingService::dueTodayOrders() const {
    return m_dueToday;
}

QList<ScheduledOrder> SchedulingService::getAll() const {
    return m_repo->getAll();
}

ScheduledOrder SchedulingService::getById(int id) const {
    return m_repo->getById(id);
}

bool SchedulingService::save(ScheduledOrder& order) {
    return m_repo->save(order);
}

bool SchedulingService::remove(int id) {
    return m_repo->remove(id);
}

QList<ScheduledOrder> SchedulingService::getByCustomerId(int customerId) const {
    return m_repo->getByCustomerId(customerId);
}
