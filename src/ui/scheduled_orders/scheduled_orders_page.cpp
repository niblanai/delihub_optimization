#include "scheduled_orders_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "scheduled_order_dialog.h"
#include "services/archive_manager.h"
#include "services/audit_service.h"
#include "services/lang_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "ui/tr_helper.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "services/notification_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFrame>
#include <QTimer>
#include <QCoreApplication>
#include <QFile>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

ScheduledOrdersPage* ScheduledOrdersPage::s_instance = nullptr;

// ── Day-of-week label helper ─────────────────────────────────────────────────
static QString weekdaysToLabel(const QString& weekdays) {
    static const QMap<QString, QString> names = {
        {"1","Mon"},{"2","Tue"},{"3","Wed"},{"4","Thu"},
        {"5","Fri"},{"6","Sat"},{"7","Sun"}
    };
    QStringList parts;
    for (const QString& d : weekdays.split(',', Qt::SkipEmptyParts))
        parts << names.value(d.trimmed(), d);
    return parts.join("  ");
}
// ────────────────────────────────────────────────────────────────────────────
ScheduledOrdersPage::ScheduledOrdersPage(QWidget* parent)
    : QWidget(parent)
{
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_repo         = new SQLiteScheduledOrderRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_productRepo  = new SQLiteProductRepository;
        m_orderRepo    = new SQLiteOrderRepository;
    } else {
        m_repo         = new AccessScheduledOrderRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_productRepo  = new AccessProductRepository;
        m_orderRepo    = new AccessOrderRepository;
    }

    m_service = new SchedulingService(m_repo, m_customerRepo);
    s_instance = this;

    setupUi();
    // Data loaded by MainWindow::navigateTo — no singleShot needed
    startReminderTimer();
}

// ─────────────────────────────────────────────────────────────────────────────
void ScheduledOrdersPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Due-Today Banner (hidden until there are due orders) ─────────────────
    m_dueTodayBanner = new QFrame;
    m_dueTodayBanner->setObjectName("schedDueBanner");   // Task 6: unique name to avoid conflict
    // Fix visual bug: global QSS paints windowBg on child QLabels, causing misaligned green patch.
    // Descendant selector forces all child widgets transparent so the frame colour shows through.
    m_dueTodayBanner->setStyleSheet(
        "QFrame#schedDueBanner { background-color:#065F46; border-bottom:2px solid #34D399; }"
        "QFrame#schedDueBanner * { background:transparent; color:#D1FAE5; }");
    m_dueTodayBanner->setFixedHeight(44);
    m_dueTodayBanner->setVisible(false);

    auto* bannerLayout = new QHBoxLayout(m_dueTodayBanner);
    bannerLayout->setContentsMargins(16, 0, 16, 0);
    bannerLayout->setSpacing(8);
    auto* bellIcon = new QLabel("🔔");
    bellIcon->setFixedWidth(24);
    m_dueTodayLabel = new QLabel;
    m_dueTodayLabel->setStyleSheet("font-size:12px;");
    bannerLayout->addWidget(bellIcon);
    bannerLayout->addWidget(m_dueTodayLabel, 1);
    root->addWidget(m_dueTodayBanner);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    tbLayout->setSpacing(10);

    auto* title = new QLabel("Scheduled Orders");
    title->setObjectName("pageTitle");
    title->setVisible(false);

    m_addBtn    = new QPushButton(LangManager::instance().t("＋ New Schedule"));
    m_addBtn->setObjectName("primaryBtn");
    
    m_editBtn   = new QPushButton(LangManager::instance().t("Edit"));
    m_editBtn->setObjectName("secondaryBtn");
    m_editBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                            QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editBtn->setIconSize(QSize(18, 18));
    m_editBtn->setEnabled(false);
    
    m_deleteBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                              QColor("#FFFFFF"), 18));
    m_deleteBtn->setIconSize(QSize(18, 18));
    m_deleteBtn->setEnabled(false);

    tbLayout->addWidget(title);
    tbLayout->addStretch();
    tbLayout->addWidget(m_addBtn);
    tbLayout->addWidget(m_editBtn);
    tbLayout->addWidget(m_deleteBtn);
    root->addWidget(toolbar);

    // ── Table — 5 columns with Convert button ────────────────────────────────
    m_table = new QTableWidget;
    m_table->setObjectName("dataTable");
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("Customer"),
        LangManager::instance().t("Schedule"),
        LangManager::instance().t("Time"),
        LangManager::instance().t("Product"),
        LangManager::instance().t("Action")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(4, 120);
    m_table->verticalHeader()->setDefaultSectionSize(44);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    // ── Status bar ───────────────────────────────────────────────────────────
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 4, 16, 4);
    m_statusLabel = new QLabel("0 recurring orders");
    m_statusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(m_statusLabel);
    root->addWidget(statusBar);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_addBtn,    &QPushButton::clicked, this, &ScheduledOrdersPage::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &ScheduledOrdersPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ScheduledOrdersPage::onDelete);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ScheduledOrdersPage::onSelectionChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int){ onEdit(); });
}

// ─────────────────────────────────────────────────────────────────────────────
void ScheduledOrdersPage::refresh() {
    loadCustomers();
    loadProducts();
    loadScheduledOrders();

    // Run the due-today check and show banner if needed
    QList<ScheduledOrder> due = m_service->checkDueToday();
    showDueTodayBanner(due);

    // Issue 2: re-run upcoming orders check on every refresh so
    // precise singleShot timers are (re)armed when the page is navigated to
    checkUpcomingOrders();
}

void ScheduledOrdersPage::loadCustomers() {
    m_customers = m_customerRepo->getAll();
    m_customerMap.clear();
    for (const auto& c : m_customers)
        m_customerMap[c.id()] = c.name();
}

void ScheduledOrdersPage::loadProducts() {
    m_products = m_productRepo->getAll();
}

void ScheduledOrdersPage::loadScheduledOrders() {
    m_orders = m_service->getAll();

    // Fill in customer names if missing
    for (auto& s : m_orders)
        if (s.customerName.isEmpty())
            s.customerName = m_customerMap.value(s.customerId, QString("ID %1").arg(s.customerId));

    m_table->setSortingEnabled(false);   // prevent row scrambling during populate
    m_table->setRowCount(0);
    for (const auto& s : m_orders) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto cell = [&](const QString& text, Qt::Alignment align = Qt::AlignVCenter | Qt::AlignLeft) {
            auto* it = new QTableWidgetItem(text);
            it->setTextAlignment(align);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        // Build product summary string: "Bread ×2, Milk ×1"
        QStringList prodParts;
        for (const auto& item : s.items)
            prodParts << QString("%1 ×%2").arg(item.productName.isEmpty()
                                                   ? QString("P%1").arg(item.productId)
                                                   : item.productName,
                                               QString::number(item.quantity));

        m_table->setItem(row, 0, cell(s.customerName));
        // Task 5: show date for one-time orders, weekdays for recurring
        QString scheduleLabel = s.isRecurring
            ? weekdaysToLabel(s.weekdays)
            : (s.oneTimeDate.isValid()
                ? "📅 " + s.oneTimeDate.toString("yyyy-MM-dd")
                : "One-time");
        m_table->setItem(row, 1, cell(scheduleLabel, Qt::AlignCenter));
        m_table->setItem(row, 2, cell(s.time.toString("hh:mm AP"), Qt::AlignCenter));
        m_table->setItem(row, 3, cell(prodParts.join(",  ")));

        // ── Convert-to-Order button ───────────────────────────────────────────
        auto* convertBtn = new QPushButton("🛒 Convert");
        convertBtn->setObjectName("primaryBtn");
        convertBtn->setFixedHeight(30);
        convertBtn->setFixedWidth(106);
        convertBtn->setContentsMargins(0, 0, 0, 0);
        convertBtn->setToolTip("Convert to new Order");
        int capturedRow = row;
        connect(convertBtn, &QPushButton::clicked, this, [this, capturedRow](){
            onConvertToOrder(capturedRow);
        });
        m_table->setCellWidget(row, 4, convertBtn);
    }

    m_statusLabel->setText(QString("%1 recurring order(s)").arg(m_orders.size()));
    m_table->setSortingEnabled(true);
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
}

void ScheduledOrdersPage::showDueTodayBanner(const QList<ScheduledOrder>& dueList) {
    if (dueList.isEmpty()) {
        m_dueTodayBanner->setVisible(false);
        return;
    }

    QStringList names;
    for (const auto& s : dueList) {
        // Issue 3: show customer name (not ID), no "@", show AM/PM
        QString name = s.customerName.isEmpty()
                           ? m_customerMap.value(s.customerId, QString("Customer %1").arg(s.customerId))
                           : s.customerName;
        QString timeStr = s.time.isValid()
            ? s.time.toString("hh:mm AP")   // e.g. "09:00 AM"
            : "";
        names << (timeStr.isEmpty() ? name : QString("%1 at %2").arg(name, timeStr));
    }

    m_dueTodayLabel->setText(
        QString("<b>%1 order(s) due today:</b>  %2")
            .arg(dueList.size())
            .arg(names.join("   ·   ")));
    m_dueTodayBanner->setVisible(true);
}

// ─────────────────────────────────────────────────────────────────────────────
void ScheduledOrdersPage::onAdd() {
    ScheduledOrderDialog dlg(m_customers, m_products, this);
    if (dlg.exec() != QDialog::Accepted) return;

    ScheduledOrder order = dlg.getScheduledOrder();
    if (!m_service->save(order)) {
        QMessageBox::critical(this, "Error", "Failed to save the scheduled order.");
        return;
    }
    Logger::instance().info(QString("Scheduled order created for customer ID=%1").arg(order.customerId));
    refresh();
}

void ScheduledOrdersPage::onEdit() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_orders.size()) return;

    ScheduledOrder order = m_orders.at(row);

    ScheduledOrderDialog dlg(m_customers, m_products, this);
    dlg.setScheduledOrder(order);
    if (dlg.exec() != QDialog::Accepted) return;

    ScheduledOrder updated = dlg.getScheduledOrder();
    if (!m_service->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update the scheduled order.");
        return;
    }
    Logger::instance().info(QString("Scheduled order updated ID=%1").arg(updated.id));
    refresh();
}

void ScheduledOrdersPage::onDelete() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_orders.size()) return;

    const ScheduledOrder& order = m_orders.at(row);
    QString custName = order.customerName.isEmpty()
                           ? m_customerMap.value(order.customerId, "?")
                           : order.customerName;

    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete recurring order for \"%1\"?").arg(custName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // Task 11: soft-delete the scheduled order
    QString archErr;
    int archiveId = ArchiveManager::instance().softDelete(
        "ScheduledOrders", "Id", order.id, &archErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive scheduled order before deletion:\n" + archErr);
        return;
    }
    // ScheduledOrderItems have ON DELETE CASCADE so they're removed automatically
    AuditService::instance().logDelete("ScheduledOrder", order.id,
        QString("{\"archiveId\":%1,\"customer\":\"%2\"}")
            .arg(archiveId).arg(custName));
    Logger::instance().info(QString("Scheduled order deleted ID=%1").arg(order.id));
    refresh();
}

void ScheduledOrdersPage::onSelectionChanged() {
    bool has = m_table->currentRow() >= 0;
    m_editBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
}


void ScheduledOrdersPage::retranslateUi() { retranslateWidget(this); }

// ─────────────────────────────────────────────────────────────────────────────
// Convert Scheduled Order → New Order
// ─────────────────────────────────────────────────────────────────────────────
void ScheduledOrdersPage::onConvertToOrder(int row) {
    if (row < 0 || row >= m_orders.size()) return;
    const ScheduledOrder& so = m_orders.at(row);

    // Build a new Order from the scheduled order
    Order order;
    order.setCustomerId(so.customerId);
    order.setCustomerName(so.customerName.isEmpty()
        ? m_customerMap.value(so.customerId) : so.customerName);
    order.setDateTime(QDateTime::currentDateTime());
    order.setStatus("Pending");
    order.setDeliveryFee(0.0);

    // Copy items
    QList<OrderItem> items;
    for (const auto& si : so.items) {
        OrderItem item;
        item.productId   = si.productId;
        item.productName = si.productName;
        item.quantity    = si.quantity;
        // Fill price from product repo
        for (const auto& p : m_products)
            if (p.id() == si.productId) { item.unitPrice = p.price(); break; }
        items.append(item);
    }
    order.setItems(items);
    order.calculateTotals();

    // Save as new order
    if (!m_orderRepo->save(order)) {
        QMessageBox::critical(this, "Error", "Failed to create order from schedule.");
        return;
    }

    // CORRECTION: only remove one-time schedules after Convert.
    // Recurring schedules stay active so they fire again next week.
    if (!so.isRecurring) {
        m_service->remove(so.id);
    }

    Logger::instance().info(QString("Scheduled order converted to Order #%1 for %2 (recurring=%3)")
        .arg(order.id()).arg(order.customerName()).arg(so.isRecurring ? "yes" : "no"));

    QMessageBox::information(this, "Order Created",
        QString("Order #%1 created for %2.\n"
                "Go to Orders tab to complete delivery details.")
            .arg(order.id()).arg(order.customerName()));

    // For one-time: remove the row from the table immediately.
    // For recurring: keep the row — it fires again next occurrence.
    if (!so.isRecurring) {
        m_orders.removeAt(row);
        m_table->removeRow(row);
        m_statusLabel->setText(QString("%1 recurring order(s)").arg(m_orders.size()));
    }

    // Signal MainWindow to switch to Orders tab and open edit dialog
    emit orderCreated(order.id());
}

// ─────────────────────────────────────────────────────────────────────────────
// Reminder Timer — checks for upcoming scheduled orders
// ─────────────────────────────────────────────────────────────────────────────
void ScheduledOrdersPage::startReminderTimer() {
    if (m_reminderTimer) return;
    m_reminderTimer = new QTimer(this);
    connect(m_reminderTimer, &QTimer::timeout, this, &ScheduledOrdersPage::onReminderTimer);

    int intervalMin = ConfigManager::instance().value("Reminders/CheckIntervalMinutes", "5").toInt();
    if (intervalMin < 1) intervalMin = 5;
    m_reminderTimer->start(intervalMin * 60 * 1000);

    // Issue 2: also fire immediately after event loop starts so we don't
    // wait up to 5 minutes for the first check on app startup
    QTimer::singleShot(0, this, &ScheduledOrdersPage::onReminderTimer);
}

void ScheduledOrdersPage::onReminderTimer() {
    Logger::instance().info(
        QString("[ReminderTimer] Fired at %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    checkUpcomingOrders();
}

void ScheduledOrdersPage::checkUpcomingOrders() {
    int todayDow = QDate::currentDate().dayOfWeek();
    QTime now    = QTime::currentTime();

    Logger::instance().info(
        QString("[CheckUpcoming] Running at %1 — todayDow=%2 — %3 schedules in repo")
            .arg(now.toString("hh:mm:ss")).arg(todayDow).arg(m_repo->getAll().size()));

    // Ensure product cache is populated (may be empty if page never navigated to)
    if (m_products.isEmpty())
        m_products = m_productRepo->getAll();

    QList<ScheduledOrder> all = m_repo->getAll();
    for (const auto& s : all) {
        // ── Determine if this order is due today ──────────────────────────────
        bool dueToday = false;
        if (s.isRecurring) {
            dueToday = s.isScheduledFor(todayDow);
        } else {
            dueToday = s.isDueToday();
        }

        Logger::instance().info(
            QString("[CheckUpcoming] Evaluating ID=%1 isRecurring=%2 weekdays='%3' "
                    "oneTimeDate=%4 time=%5 → dueToday=%6")
                .arg(s.id).arg(s.isRecurring).arg(s.weekdays)
                .arg(s.oneTimeDate.isValid() ? s.oneTimeDate.toString("yyyy-MM-dd") : "N/A")
                .arg(s.time.toString("hh:mm"))
                .arg(dueToday ? "YES" : "no"));

        if (!dueToday) continue;
        if (!s.time.isValid()) {
            Logger::instance().warn(QString("[CheckUpcoming] ID=%1: time invalid, skipping").arg(s.id));
            continue;
        }

        // Per-schedule: how many minutes before to remind
        int remindBefore = s.remindMinutesBefore > 0 ? s.remindMinutesBefore
            : ConfigManager::instance().value("Reminders/MinutesBefore", "60").toInt();

        int secToOrder = now.secsTo(s.time);

        // BUG FIX 1: also fire auto-create for recently-passed orders (within the last hour)
        bool inFutureWindow = (secToOrder >= 0 && secToOrder <= remindBefore * 60);
        bool recentlyPassed = (secToOrder < 0 && secToOrder >= -3600);

        if (!inFutureWindow && !recentlyPassed) continue;

        // Deduplication key
        int repeatSec = s.remindRepeatInterval * 60;
        QString baseKey = QString("Reminded_%1_%2").arg(s.id).arg(QDate::currentDate().toString("yyyyMMdd"));

        if (repeatSec > 0 && inFutureWindow) {
            int slot = secToOrder / repeatSec;
            baseKey  = QString("Reminded_%1_%2_slot%3")
                       .arg(s.id).arg(QDate::currentDate().toString("yyyyMMdd")).arg(slot);
        }

        if (ConfigManager::instance().value(baseKey, "0") == "1") continue;

        // For autoCreate orders that are in the future window, also skip if
        // a precise singleShot has already been armed for this schedule today.
        if (s.autoCreateOrder && inFutureWindow) {
            QString armedKey = QString("ArmedFor_%1_%2")
                .arg(s.id).arg(QDate::currentDate().toString("yyyyMMdd"));
            if (ConfigManager::instance().value(armedKey, "0") == "1") continue;
        }

        QString name = s.customerName.isEmpty()
            ? m_customerMap.value(s.customerId, "?") : s.customerName;

        Logger::instance().info(
            QString("[CheckUpcoming] Schedule ID=%1 (%2): dueToday=%3 secToOrder=%4 "
                    "inFutureWindow=%5 recentlyPassed=%6 autoCreate=%7 isRecurring=%8 weekdays='%9'")
                .arg(s.id).arg(name).arg(dueToday).arg(secToOrder)
                .arg(inFutureWindow).arg(recentlyPassed)
                .arg(s.autoCreateOrder).arg(s.isRecurring).arg(s.weekdays));

        // ── Auto-create order if enabled ──────────────────────────────────────
        if (s.autoCreateOrder) {
            if (secToOrder > 0) {
                // Arm a precise singleShot to fire at the exact delivery second.
                // Use a separate "ArmedFor_" key to block the periodic timer from
                // also creating the order, while the lambda uses a different check
                // so it does NOT bail out from the same key.
                ScheduledOrder captured = s;
                QString capturedName    = name;
                QString capturedKey     = baseKey;   // "Reminded_<id>_<date>"
                QString armedKey        = QString("ArmedFor_%1_%2")
                    .arg(s.id).arg(QDate::currentDate().toString("yyyyMMdd"));

                // Mark "armed" so the periodic timer won't fire a duplicate
                ConfigManager::instance().setOverride(armedKey, "1");

                Logger::instance().info(
                    QString("[AutoCreate] Arming precise timer for schedule ID=%1 (%2) "
                            "in %3 seconds — isRecurring=%4 weekdays='%5'")
                        .arg(s.id).arg(capturedName).arg(secToOrder)
                        .arg(s.isRecurring ? "yes" : "no").arg(s.weekdays));

                QTimer::singleShot(secToOrder * 1000, this, [this, captured, capturedName, capturedKey, armedKey]() {
                    // Check the "created" dedup key (not the armed key)
                    if (ConfigManager::instance().value(capturedKey, "0") == "1") {
                        Logger::instance().info(
                            QString("[AutoCreate] singleShot fired for ID=%1 but dedup key already set — skipping")
                                .arg(captured.id));
                        return;
                    }
                    // Re-check the schedule still exists (may have been cancelled/converted)
                    ScheduledOrder current = m_repo->getById(captured.id);
                    if (current.id == 0) {
                        Logger::instance().info(
                            QString("[AutoCreate] singleShot fired for ID=%1 but schedule no longer exists — skipping")
                                .arg(captured.id));
                        return;
                    }

                    if (m_products.isEmpty())
                        m_products = m_productRepo->getAll();

                    Order order;
                    order.setCustomerId(captured.customerId);
                    order.setCustomerName(capturedName);
                    order.setDateTime(QDateTime::currentDateTime());
                    order.setStatus("Pending");
                    order.setDeliveryFee(0.0);

                    QList<OrderItem> items;
                    for (const auto& si : captured.items) {
                        OrderItem item;
                        item.productId   = si.productId;
                        item.productName = si.productName;
                        item.quantity    = si.quantity;
                        for (const auto& p : m_products)
                            if (p.id() == si.productId) { item.unitPrice = p.price(); break; }
                        items.append(item);
                    }
                    order.setItems(items);
                    order.calculateTotals();

                    if (m_orderRepo->save(order)) {
                        // Mark dedup AFTER successful creation
                        ConfigManager::instance().setOverride(capturedKey, "1");

                        Logger::instance().info(
                            QString("[AutoCreate] Created order #%1 for '%2' from schedule ID=%2 (isRecurring=%3)")
                                .arg(order.id()).arg(capturedName).arg(captured.id)
                                .arg(captured.isRecurring ? "yes" : "no"));

                        if (!captured.isRecurring) {
                            m_service->remove(captured.id);
                            QTimer::singleShot(0, this, &ScheduledOrdersPage::refresh);
                        }

                        NotificationService::instance().dispatch(
                            NotificationEvent::ScheduledOrderDueToday,
                            "🤖 Scheduled Order Auto-Created",
                            QString("Order #%1 for %2 created at scheduled time.")
                                .arg(order.id()).arg(capturedName));

                        emit orderCreated(order.id());
                    } else {
                        Logger::instance().error(
                            QString("[AutoCreate] Failed to save order for schedule ID=%1")
                                .arg(captured.id));
                    }
                });
            } else {
                // Time already passed (recentlyPassed) — create immediately
                Logger::instance().info(
                    QString("[AutoCreate] Catch-up: schedule ID=%1 (%2) already past due by %3 sec")
                        .arg(s.id).arg(name).arg(-secToOrder));
                Order order;
                order.setCustomerId(s.customerId);
                order.setCustomerName(name);
                order.setDateTime(QDateTime::currentDateTime());
                order.setStatus("Pending");
                order.setDeliveryFee(0.0);

                QList<OrderItem> items;
                for (const auto& si : s.items) {
                    OrderItem item;
                    item.productId   = si.productId;
                    item.productName = si.productName;
                    item.quantity    = si.quantity;
                    for (const auto& p : m_products)
                        if (p.id() == si.productId) { item.unitPrice = p.price(); break; }
                    items.append(item);
                }
                order.setItems(items);
                order.calculateTotals();

                if (m_orderRepo->save(order)) {
                    Logger::instance().info(
                        QString("Auto-created (catch-up) order #%1 from schedule ID=%2").arg(order.id()).arg(s.id));

                    if (!s.isRecurring) {
                        m_service->remove(s.id);
                        QTimer::singleShot(0, this, &ScheduledOrdersPage::refresh);
                    }
                    ConfigManager::instance().setOverride(baseKey, "1");

                    NotificationService::instance().dispatch(
                        NotificationEvent::ScheduledOrderDueToday,
                        "🤖 Scheduled Order Auto-Created",
                        QString("Order #%1 for %2 was automatically created.\n"
                                "Please go to Orders tab to complete delivery details.")
                            .arg(order.id()).arg(name));

                    emit orderCreated(order.id());
                }
            }
        } else {
            // Manual reminder — Task 6: updated notification text with language support
            bool isArabic = (LangManager::instance().current() == AppLang::Arabic);
            QString notifMsg = isArabic
                ? QString("الطلب %1 للعميل %2 يقترب موعد تسليمه")
                    .arg(s.id).arg(name)
                : QString("Order %1 for customer %2 is approaching its due time")
                    .arg(s.id).arg(name);
            NotificationService::instance().dispatch(
                NotificationEvent::ScheduledOrderDueToday,
                isArabic ? "⏰ تنبيه طلب مجدول" : "⏰ Upcoming Scheduled Order",
                notifMsg);

            ConfigManager::instance().setOverride(baseKey, "1");
        }

        Logger::instance().info(
            QString("Reminder fired for scheduled order ID=%1 (%2)").arg(s.id).arg(name));
    }
}

