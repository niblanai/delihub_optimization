#ifndef SCHEDULING_SERVICE_H
#define SCHEDULING_SERVICE_H

#include "core/scheduled_order.h"
#include "data/irepositories.h"
#include <QList>

class SchedulingService {
public:
    explicit SchedulingService(IScheduledOrderRepository* repo,
                               ICustomerRepository*       customerRepo = nullptr);

    // Called at startup (and on demand): checks which scheduled orders are due
    // today and fires NotificationService events for each one found.
    // Returns the list of due orders for display.
    QList<ScheduledOrder> checkDueToday();

    // Returns the same result without re-querying the DB (cached from last check)
    QList<ScheduledOrder> dueTodayOrders() const;

    // CRUD pass-through for the UI layer
    QList<ScheduledOrder> getAll() const;
    ScheduledOrder        getById(int id) const;
    bool save(ScheduledOrder& order);
    bool remove(int id);
    QList<ScheduledOrder> getByCustomerId(int customerId) const;

private:
    IScheduledOrderRepository* m_repo;
    ICustomerRepository*       m_customerRepo;
    QList<ScheduledOrder>      m_dueToday;
};

#endif // SCHEDULING_SERVICE_H
