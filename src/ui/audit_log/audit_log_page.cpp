#include "audit_log_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "services/archive_manager.h"
#include "ui/svg_icon_helper.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include "services/session_manager.h"
#include "services/auth_service.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "core/product.h"
#include "core/customer.h"
#include "core/order.h"
#include "core/phone.h"
#include "core/address.h"
#include "core/order_item.h"
#include "ui/tr_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFrame>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QJsonArray>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFrame>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

// ─────────────────────────────────────────────────────────────────────────────
AuditLogPage::AuditLogPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_auditRepo    = new SQLiteAuditLogRepository;
        m_orderRepo    = new SQLiteOrderRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_productRepo  = new SQLiteProductRepository;
        m_userRepo     = new SQLiteUserRepository;
        m_roleRepo     = new SQLiteRoleRepository;
    } else {
        m_auditRepo    = new AccessAuditLogRepository;
        m_orderRepo    = new AccessOrderRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_productRepo  = new AccessProductRepository;
        m_userRepo     = new AccessUserRepository;
        m_roleRepo     = new AccessRoleRepository;
    }

    setupUi();
    // Data loaded on first navigateTo(), not in constructor
}

// ─────────────────────────────────────────────────────────────────────────────
void AuditLogPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    tbLayout->setSpacing(10);

    auto* title = new QLabel("Audit Log");
    title->setObjectName("pageTitle");
    title->setVisible(false);

    m_refreshBtn = new QPushButton(LangManager::instance().t("Refresh"));
    m_refreshBtn->setObjectName("secondaryBtn");
    m_refreshBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("refresh-svgrepo-com.svg"), 
                                               QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_refreshBtn->setIconSize(QSize(18, 18));

    m_undoBtn = new QPushButton(LangManager::instance().t("↩️  Undo Selected"));
    m_undoBtn->setObjectName("dangerBtn");
    m_undoBtn->setEnabled(false);
    m_undoBtn->setToolTip("Undo the selected operation (requires dual authentication)");

    tbLayout->addWidget(title);
    tbLayout->addStretch();
    tbLayout->addWidget(m_refreshBtn);
    tbLayout->addWidget(m_undoBtn);
    root->addWidget(toolbar);

    // ── Filters ───────────────────────────────────────────────────────────────
    auto* filterFrame = new QFrame;
    filterFrame->setObjectName("filterBar");
    // background controlled by QSS theme via #filterBar rule
    auto* filterLayout = new QHBoxLayout(filterFrame);
    filterLayout->setContentsMargins(16, 8, 16, 8);
    filterLayout->setSpacing(10);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search details...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMaximumWidth(200);
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);

    m_userFilterCombo = new QComboBox;
    m_userFilterCombo->addItem("All Users", "");
    m_userFilterCombo->setMinimumWidth(130);

    m_actionFilterCombo = new QComboBox;
    m_actionFilterCombo->addItem("All Actions", "");
    for (const QString& a : {"Create","Update","Delete","Login","Logout","Backup","Restore"})
        m_actionFilterCombo->addItem(a, a);
    m_actionFilterCombo->setMinimumWidth(120);

    m_entityFilterCombo = new QComboBox;
    m_entityFilterCombo->addItem("All Entities", "");
    for (const QString& e : {"Order","Customer","Product","User","Coupon","Return","ScheduledOrder"})
        m_entityFilterCombo->addItem(e, e);
    m_entityFilterCombo->setMinimumWidth(120);

    m_fromDateEdit = new QDateEdit(QDate::currentDate().addDays(-30));
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setMaximumWidth(120);

    auto* toLabel = new QLabel("→");
    toLabel->setStyleSheet("color:#94A3B8;");

    m_toDateEdit = new QDateEdit(QDate::currentDate());
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setMaximumWidth(120);

    m_filterBtn = new QPushButton(LangManager::instance().t("Apply"));
    m_filterBtn->setObjectName("primaryBtn");
    m_filterBtn->setFixedHeight(30);

    m_clearFilterBtn = new QPushButton(LangManager::instance().t("Clear"));
    m_clearFilterBtn->setObjectName("secondaryBtn");
    m_clearFilterBtn->setFixedHeight(30);

    filterLayout->addWidget(m_searchEdit);
    filterLayout->addWidget(new QLabel(LangManager::instance().t("User:")));
    filterLayout->addWidget(m_userFilterCombo);
    filterLayout->addWidget(new QLabel(LangManager::instance().t("Action:")));
    filterLayout->addWidget(m_actionFilterCombo);
    filterLayout->addWidget(new QLabel(LangManager::instance().t("Entity:")));
    filterLayout->addWidget(m_entityFilterCombo);
    filterLayout->addWidget(new QLabel(LangManager::instance().t("From:")));
    filterLayout->addWidget(m_fromDateEdit);
    filterLayout->addWidget(toLabel);
    filterLayout->addWidget(m_toDateEdit);
    filterLayout->addWidget(m_filterBtn);
    filterLayout->addWidget(m_clearFilterBtn);
    filterLayout->addStretch();
    root->addWidget(filterFrame);

    // ── Table ─────────────────────────────────────────────────────────────────
    m_table = new QTableWidget;
    m_table->setObjectName("dataTable");
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("ID"),
        LangManager::instance().t("Timestamp"),
        LangManager::instance().t("User"),
        LangManager::instance().t("Action"),
        LangManager::instance().t("Entity"),
        LangManager::instance().t("Entity ID"),
        LangManager::instance().t("Details")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 50);   // ID
    m_table->setColumnWidth(1, 150);  // Timestamp
    m_table->setColumnWidth(2, 110);  // User
    m_table->setColumnWidth(3, 80);   // Action
    m_table->setColumnWidth(4, 110);  // Entity
    m_table->setColumnWidth(5, 70);   // Entity ID
    // Column 6 (Details) is Stretch — fills remaining width
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    // ── Status bar ────────────────────────────────────────────────────────────
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 4, 16, 4);
    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    auto* undoNote = new QLabel("ℹ️  Undo requires authentication from both the operation owner and an admin.");
    undoNote->setObjectName("hintLabel");
    statusLayout->addWidget(undoNote);
    root->addWidget(statusBar);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_filterBtn,      &QPushButton::clicked, this, &AuditLogPage::onFilter);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, &AuditLogPage::onClearFilters);
    connect(m_refreshBtn,     &QPushButton::clicked, this, &AuditLogPage::refresh);
    connect(m_undoBtn,        &QPushButton::clicked, this, &AuditLogPage::onUndoEntry);
    connect(m_searchEdit,     &QLineEdit::returnPressed, this, &AuditLogPage::onFilter);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_table->currentRow();
        bool canUndo = false;
        if (row >= 0) {
            // Use the id stored in UserRole (column 0) to find the correct
            // AuditLogEntry regardless of how the table is sorted.
            auto* idItem = m_table->item(row, 0);
            if (idItem) {
                int entryId = idItem->data(Qt::UserRole).toInt();
                for (const auto& e : m_entries) {
                    if (e.id == entryId) {
                        canUndo = (e.action == "Create" || e.action == "Update" || e.action == "Delete");
                        break;
                    }
                }
            }
        }
        m_undoBtn->setEnabled(canUndo && SessionManager::instance().isAdmin());
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void AuditLogPage::refresh() {
    // Reload user list for filter combo
    m_userFilterCombo->blockSignals(true);
    QString currentUser = m_userFilterCombo->currentData().toString();
    m_userFilterCombo->clear();
    m_userFilterCombo->addItem("All Users", "");
    const auto& users = m_userRepo->getAll();
    for (const auto& u : users)
        m_userFilterCombo->addItem(u.name().isEmpty() ? u.username() : u.name(), u.username());
    int ui = m_userFilterCombo->findData(currentUser);
    if (ui >= 0) m_userFilterCombo->setCurrentIndex(ui);
    m_userFilterCombo->blockSignals(false);

    loadEntries();
}

void AuditLogPage::loadEntries() {
    QDateTime from = QDateTime(m_fromDateEdit->date(), QTime(0,0,0));
    QDateTime to   = QDateTime(m_toDateEdit->date(),   QTime(23,59,59));

    QList<AuditLogEntry> all = m_auditRepo->getEntriesDateRange(from, to);

    // Apply additional filters
    QString userFilter   = m_userFilterCombo->currentData().toString();
    QString actionFilter = m_actionFilterCombo->currentData().toString();
    QString entityFilter = m_entityFilterCombo->currentData().toString();
    QString searchText   = m_searchEdit->text().trimmed().toLower();

    m_entries.clear();
    for (const auto& e : all) {
        if (!userFilter.isEmpty()   && e.username != userFilter)   continue;
        if (!actionFilter.isEmpty() && e.action   != actionFilter) continue;
        if (!entityFilter.isEmpty() && e.entityType != entityFilter) continue;
        if (!searchText.isEmpty()   && !e.details.toLower().contains(searchText)
            && !e.entityType.toLower().contains(searchText)) continue;
        m_entries.append(e);
    }

    // Populate table
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    // Action → color map
    auto actionColor = [](const QString& a) -> QString {
        if (a == "Create")  return "#4ADE80";   // green
        if (a == "Update")  return "#38BDF8";   // blue
        if (a == "Delete")  return "#F87171";   // red
        if (a == "Login")   return "#A78BFA";   // purple
        if (a == "Logout")  return "#94A3B8";   // gray
        if (a == "Backup")  return "#FB923C";   // orange
        return "#E2E8F0";
    };

    for (const auto& e : m_entries) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto cell = [&](const QString& txt, Qt::Alignment al = Qt::AlignVCenter | Qt::AlignLeft) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(al);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };

        auto* actionItem = cell(e.action, Qt::AlignCenter);
        actionItem->setForeground(QBrush(QColor(actionColor(e.action))));

        // Store the entry's unique id in UserRole so we can look up the correct
        // AuditLogEntry even after the user sorts the table (currentRow() would
        // give the visual position, not the m_entries index — causing wrong entity).
        auto* idItem = cell(QString::number(e.id), Qt::AlignCenter);
        idItem->setData(Qt::UserRole, e.id);   // <-- key fix
        m_table->setItem(row, 0, idItem);
        m_table->setItem(row, 1, cell(e.timestamp.toString("yyyy-MM-dd  hh:mm:ss"), Qt::AlignCenter));
        m_table->setItem(row, 2, cell(e.username.isEmpty() ? "—" : e.username, Qt::AlignCenter));
        m_table->setItem(row, 3, actionItem);
        m_table->setItem(row, 4, cell(e.entityType, Qt::AlignCenter));
        m_table->setItem(row, 5, cell(e.entityId > 0 ? QString::number(e.entityId) : "—", Qt::AlignCenter));
        m_table->setItem(row, 6, cell(e.details));
    }

    m_table->setSortingEnabled(true);
    m_statusLabel->setText(QString("%1 record(s) displayed").arg(m_entries.size()));
    m_undoBtn->setEnabled(false);
}

void AuditLogPage::onFilter() {
    loadEntries();
}

void AuditLogPage::onClearFilters() {
    m_searchEdit->clear();
    m_userFilterCombo->setCurrentIndex(0);
    m_actionFilterCombo->setCurrentIndex(0);
    m_entityFilterCombo->setCurrentIndex(0);
    m_fromDateEdit->setDate(QDate::currentDate().addDays(-30));
    m_toDateEdit->setDate(QDate::currentDate());
    loadEntries();
}

// ── Dual-auth confirmation ────────────────────────────────────────────────────
bool AuditLogPage::confirmUndoAuth(const AuditLogEntry& entry) {
    const QString currentAdminUsername = SessionManager::instance().currentUser().username();
    const QString ownerUsername = (entry.username.isEmpty() || entry.username == "—")
                                  ? currentAdminUsername
                                  : entry.username;
    const bool sameUser = (ownerUsername == currentAdminUsername);

    QDialog dlg(this);
    dlg.setWindowTitle("Confirm Undo — Authentication Required");
    dlg.setMinimumWidth(460);
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    // ── Description ───────────────────────────────────────────────────────────
    auto* desc = new QLabel(QString(
        "<b>You are about to undo:</b><br>"
        "Action: <b>%1</b>  |  Entity: <b>%2 #%3</b><br>"
        "Originally by: <b>%4</b>  @  %5<br><br>"
        "%6")
        .arg(entry.action,
             entry.entityType,
             entry.entityId > 0 ? QString::number(entry.entityId) : "?",
             ownerUsername,
             entry.timestamp.toString("yyyy-MM-dd hh:mm"),
             sameUser
               ? "You performed this action. Enter <b>your password</b> once to confirm."
               : "Enter the <b>operation owner's password</b> AND the password of <b>any Administrator account</b>."));
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    layout->addWidget(desc);

    // ── Owner password (only when owner != current admin) ─────────────────────
    QLineEdit* ownerPass = nullptr;
    if (!sameUser) {
        auto* ownerGroup = new QGroupBox(QString("Operation Owner Password  (%1)").arg(ownerUsername));
        auto* ownerForm  = new QFormLayout(ownerGroup);
        ownerPass = new QLineEdit;
        ownerPass->setEchoMode(QLineEdit::Password);
        ownerPass->setPlaceholderText(QString("Password for '%1'...").arg(ownerUsername));
        ownerForm->addRow("Password:", ownerPass);
        layout->addWidget(ownerGroup);
    }

    // ── Admin section — username + password (any admin account) ───────────────
    auto* adminGroup = new QGroupBox(
        sameUser ? QString("Your Password  (%1)").arg(currentAdminUsername)
                 : "Administrator Password  (any account with Admin role)");
    auto* adminForm = new QFormLayout(adminGroup);

    // When owner != admin: show username field so any admin account can be entered
    QLineEdit* adminUserEdit = nullptr;
    if (!sameUser) {
        adminUserEdit = new QLineEdit;
        adminUserEdit->setPlaceholderText("Admin username...");
        adminUserEdit->setText(currentAdminUsername);
        adminForm->addRow("Admin Username:", adminUserEdit);
    }

    auto* adminPass = new QLineEdit;
    adminPass->setEchoMode(QLineEdit::Password);
    adminPass->setPlaceholderText("Password...");
    adminForm->addRow("Password:", adminPass);
    layout->addWidget(adminGroup);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("Confirm Undo");
    btns->button(QDialogButtonBox::Ok)->setObjectName("dangerBtn");
    layout->addWidget(btns);

    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&]() {
        AuthService auth(m_userRepo);

        // ── 1. Verify admin ────────────────────────────────────────────────────
        if (adminPass->text().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Password is required.");
            adminPass->setFocus();
            return;
        }

        QString adminUsernameToCheck = sameUser
            ? currentAdminUsername
            : (adminUserEdit ? adminUserEdit->text().trimmed() : currentAdminUsername);

        if (adminUsernameToCheck.isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Admin username is required.");
            if (adminUserEdit) adminUserEdit->setFocus();
            return;
        }

        User adminUser = auth.authenticate(adminUsernameToCheck, adminPass->text());
        if (adminUser.id() == 0) {
            QMessageBox::critical(&dlg, "Authentication Failed",
                QString("Incorrect username or password for '%1'.").arg(adminUsernameToCheck));
            adminPass->clear();
            adminPass->setFocus();
            return;
        }

        // Verify the admin account has canManageUsers permission
        if (!sameUser) {
            Role adminRole = m_roleRepo->getById(adminUser.roleId());
            if (!adminRole.canManageUsers) {
                QMessageBox::critical(&dlg, "Access Denied",
                    QString("User '%1' does not have Administrator privileges.")
                        .arg(adminUsernameToCheck));
                adminPass->clear();
                adminPass->setFocus();
                return;
            }
        }

        // ── 2. Verify operation owner (when different from admin) ──────────────
        if (!sameUser) {
            if (!ownerPass || ownerPass->text().isEmpty()) {
                QMessageBox::warning(&dlg, "Validation",
                    QString("Password for operation owner '%1' is required.").arg(ownerUsername));
                if (ownerPass) ownerPass->setFocus();
                return;
            }
            User owner = auth.authenticate(ownerUsername, ownerPass->text());
            if (owner.id() == 0) {
                QMessageBox::critical(&dlg, "Authentication Failed",
                    QString("Incorrect password for operation owner '%1'.").arg(ownerUsername));
                ownerPass->clear();
                ownerPass->setFocus();
                return;
            }
        }

        dlg.accept();
    });

    QTimer::singleShot(0, &dlg, [ownerPass, adminPass]() {
        if (ownerPass) ownerPass->setFocus();
        else           adminPass->setFocus();
    });

    return dlg.exec() == QDialog::Accepted;
}

// ── Perform the actual undo ───────────────────────────────────────────────────
bool AuditLogPage::performUndo(const AuditLogEntry& entry) {
    QJsonDocument doc  = QJsonDocument::fromJson(entry.details.toUtf8());
    QJsonObject   root = doc.object();
    bool    undoOk     = false;
    QString undoDetail;

    // ── Helper: read Order items from snapshot ─────────────────────────────────
    auto readOrderItems = [](const QJsonObject& snap) -> QList<OrderItem> {
        QList<OrderItem> items;
        for (const auto& v : snap["items"].toArray()) {
            QJsonObject io = v.toObject();
            OrderItem item;
            item.productId   = io["productId"].toInt();
            item.productName = io["productName"].toString();
            item.quantity    = io["qty"].toInt();
            item.unitPrice   = io["unitPrice"].toDouble();
            items.append(item);
        }
        return items;
    };

    // ── Helper: build Product from snapshot ────────────────────────────────────
    auto buildProduct = [](const QJsonObject& s) -> Product {
        Product p;
        p.setId(s["id"].toInt());
        p.setName(s["name"].toString());
        p.setBarcode(s["barcode"].toString());
        p.setPrice(s["price"].toDouble());
        p.setCategoryId(s["categoryId"].toInt());
        p.setStatus(Product::stringToStatus(s["status"].toString()));
        if (!s["mfgDate"].toString().isEmpty())
            p.setManufactureDate(QDate::fromString(s["mfgDate"].toString(), "yyyy-MM-dd"));
        if (!s["expDate"].toString().isEmpty())
            p.setExpiryDate(QDate::fromString(s["expDate"].toString(), "yyyy-MM-dd"));
        p.setStockQty(s["stockQty"].toInt());
        p.setLowStockThreshold(s["lowStockThreshold"].toInt(5));
        return p;
    };

    // ── Helper: build Customer from snapshot ───────────────────────────────────
    auto buildCustomer = [](const QJsonObject& s) -> Customer {
        Customer c;
        c.setId(s["id"].toInt());
        c.setName(s["name"].toString());
        c.setNotes(s["notes"].toString());
        c.setRegionId(s["regionId"].toInt());
        c.setDistanceKm(s["distanceKm"].toDouble());
        c.setStatus(Customer::stringToStatus(s["status"].toString()));
        c.setPreferredPaymentMethod(s["preferredPayment"].toString());
        QList<Phone> phones;
        for (const auto& v : s["phones"].toArray())
            phones.append(Phone{v.toString()});
        c.setPhones(phones);
        QList<Address> addrs;
        for (const auto& v : s["addresses"].toArray())
            addrs.append(Address{v.toString()});
        c.setAddresses(addrs);
        return c;
    };

    // ── Helper: build Order from snapshot ─────────────────────────────────────
    auto buildOrder = [&readOrderItems](const QJsonObject& s) -> Order {
        Order o;
        o.setId(s["id"].toInt());
        o.setCustomerId(s["customerId"].toInt());
        o.setCustomerName(s["customerName"].toString());
        o.setStatus(s["status"].toString().isEmpty() ? "Pending" : s["status"].toString());
        o.setCancelReason(s["cancelReason"].toString());
        o.setDeliveryFee(s["deliveryFee"].toDouble());
        o.setDiscountAmount(s["discountAmount"].toDouble());
        o.setDiscountReason(s["discountReason"].toString());
        o.setPaymentMethod(s["paymentMethod"].toString().isEmpty()
                               ? "Cash" : s["paymentMethod"].toString());
        o.setPaymentOtherDetail(s["paymentOtherDetail"].toString());
        o.setDriverId(s["driverId"].toInt());
        o.setDriverName(s["driverName"].toString());
        if (!s["dateTime"].toString().isEmpty())
            o.setDateTime(QDateTime::fromString(s["dateTime"].toString(), Qt::ISODate));
        o.setItems(readOrderItems(s));
        o.calculateTotals();
        return o;
    };

    // ── DELETE undo: restore from DeletedRecordsArchive ───────────────────────
    // The archiveId stored in the audit entry details is the single source of
    // truth — we do NOT reconstruct from a "before" snapshot anymore.
    if (entry.action == "Delete") {
        int archiveId = root["archiveId"].toInt(-1);
        if (archiveId < 0) {
            // Legacy entries (before Task 11) had no archiveId — fall back to
            // snapshot-based restore for backward compat, but warn the user.
            if (!root.contains("before")) {
                QMessageBox::warning(this, "Cannot Undo",
                    QString("No archive reference found for this Delete entry.\n"
                            "%1 #%2 cannot be restored automatically.\n\n"
                            "(This entry predates the soft-delete system.)")
                        .arg(entry.entityType).arg(entry.entityId));
                return false;
            }
            // ── Legacy fallback: rebuild from "before" snapshot ────────────────
            QJsonObject snap = root["before"].toObject();

            auto buildProduct = [](const QJsonObject& s) -> Product {
                Product p;
                p.setId(s["id"].toInt()); p.setName(s["name"].toString());
                p.setBarcode(s["barcode"].toString()); p.setPrice(s["price"].toDouble());
                p.setCategoryId(s["categoryId"].toInt());
                p.setStatus(Product::stringToStatus(s["status"].toString()));
                if (!s["mfgDate"].toString().isEmpty())
                    p.setManufactureDate(QDate::fromString(s["mfgDate"].toString(), "yyyy-MM-dd"));
                if (!s["expDate"].toString().isEmpty())
                    p.setExpiryDate(QDate::fromString(s["expDate"].toString(), "yyyy-MM-dd"));
                p.setStockQty(s["stockQty"].toInt());
                p.setLowStockThreshold(s["lowStockThreshold"].toInt(5));
                return p;
            };
            auto buildCustomer = [](const QJsonObject& s) -> Customer {
                Customer c; c.setId(s["id"].toInt()); c.setName(s["name"].toString());
                c.setNotes(s["notes"].toString()); c.setRegionId(s["regionId"].toInt());
                c.setDistanceKm(s["distanceKm"].toDouble());
                c.setStatus(Customer::stringToStatus(s["status"].toString()));
                c.setPreferredPaymentMethod(s["preferredPayment"].toString());
                QList<Phone> phones;
                for (const auto& v : s["phones"].toArray()) phones.append(Phone{v.toString()});
                c.setPhones(phones);
                QList<Address> addrs;
                for (const auto& v : s["addresses"].toArray()) addrs.append(Address{v.toString()});
                c.setAddresses(addrs);
                return c;
            };

            if (entry.entityType == "Product") {
                Product p = buildProduct(snap);
                undoOk = m_productRepo->save(p);
                undoDetail = QString("Restored Product '%1' (legacy snapshot)").arg(p.name());
            } else if (entry.entityType == "Customer") {
                Customer c = buildCustomer(snap);
                undoOk = m_customerRepo->save(c);
                undoDetail = QString("Restored Customer '%1' (legacy snapshot)").arg(c.name());
            } else {
                QMessageBox::warning(this, "Cannot Undo",
                    QString("Legacy undo not supported for '%1'.").arg(entry.entityType));
                return false;
            }
            if (!undoOk) {
                QMessageBox::critical(this, "Undo Failed",
                    QString("Failed to restore %1 #%2 from legacy snapshot.")
                        .arg(entry.entityType).arg(entry.entityId));
                return false;
            }
        } else {
            // ── Normal path: restore via ArchiveManager ────────────────────────
            QString restoreErr;
            undoOk = ArchiveManager::instance().restore(archiveId, &restoreErr);
            if (!undoOk) {
                QMessageBox::critical(this, "Undo Failed", restoreErr);
                return false;
            }
            undoDetail = QString("Restored %1 #%2 from archive #%3")
                             .arg(entry.entityType).arg(entry.entityId).arg(archiveId);
        }

    // ── CREATE undo: delete the created record ─────────────────────────────────
    } else if (entry.action == "Create") {
        if (entry.entityType == "Product")
            undoOk = m_productRepo->remove(entry.entityId);
        else if (entry.entityType == "Customer")
            undoOk = m_customerRepo->remove(entry.entityId);
        else if (entry.entityType == "Order")
            undoOk = m_orderRepo->remove(entry.entityId);
        else {
            QMessageBox::warning(this, "Cannot Undo",
                QString("Automatic undo for '%1' create is not supported.")
                    .arg(entry.entityType));
            return false;
        }

        if (!undoOk) {
            QMessageBox::warning(this, "Undo Failed",
                QString("Could not delete %1 #%2.\n"
                        "It may already be deleted or linked to other records.")
                    .arg(entry.entityType).arg(entry.entityId));
            return false;
        }
        undoDetail = QString("Deleted %1 #%2 (undid Create)")
                     .arg(entry.entityType).arg(entry.entityId);

    // ── UPDATE undo: revert to "before" snapshot ───────────────────────────────
    } else if (entry.action == "Update") {
        if (!root.contains("before")) {
            QMessageBox::warning(this, "Cannot Undo",
                QString("No before-snapshot found for this Update on %1 #%2.\n"
                        "Cannot revert to previous state.")
                    .arg(entry.entityType).arg(entry.entityId));
            return false;
        }
        QJsonObject snap = root["before"].toObject();

        if (entry.entityType == "Product") {
            Product p = buildProduct(snap);
            undoOk     = m_productRepo->save(p);
            undoDetail = QString("Reverted Product '%1' to previous state").arg(p.name());

        } else if (entry.entityType == "Customer") {
            Customer c = buildCustomer(snap);
            undoOk     = m_customerRepo->save(c);
            undoDetail = QString("Reverted Customer '%1' to previous state").arg(c.name());

        } else if (entry.entityType == "Order") {
            Order o = buildOrder(snap);
            undoOk     = m_orderRepo->save(o);
            undoDetail = QString("Reverted Order #%1 to previous state").arg(o.id());

        } else {
            QMessageBox::warning(this, "Cannot Undo",
                QString("Automatic undo for '%1' update is not supported.")
                    .arg(entry.entityType));
            return false;
        }

        if (!undoOk) {
            QMessageBox::critical(this, "Undo Failed",
                QString("Failed to revert %1 #%2.")
                    .arg(entry.entityType).arg(entry.entityId));
            return false;
        }

    } else {
        QMessageBox::warning(this, "Not Undoable",
            QString("Action type '%1' cannot be undone automatically.").arg(entry.action));
        return false;
    }

    // ── Write Undo entry to audit log ─────────────────────────────────────────
    AuditLogEntry undoEntry;
    undoEntry.userId     = SessionManager::instance().currentUser().id();
    undoEntry.username   = SessionManager::instance().currentUser().username();
    undoEntry.action     = "Undo";
    undoEntry.entityType = entry.entityType;
    undoEntry.entityId   = entry.entityId;
    undoEntry.details    = QString("Undid [%1] on %2 #%3 (audit entry #%4). %5")
                           .arg(entry.action, entry.entityType)
                           .arg(entry.entityId).arg(entry.id)
                           .arg(undoDetail);
    undoEntry.timestamp  = QDateTime::currentDateTime();
    m_auditRepo->addEntry(undoEntry);
    Logger::instance().info(undoEntry.details);

    return true;
}

void AuditLogPage::onUndoEntry() {
    int row = m_table->currentRow();
    if (row < 0) return;

    // Use UserRole id stored in column 0 to find the correct entry after sorting.
    // Using currentRow() directly as an index into m_entries is wrong when the
    // table is sorted because currentRow() is the visual/display position, not
    // the original load order that m_entries uses.
    auto* idItem = m_table->item(row, 0);
    if (!idItem) return;
    int entryId = idItem->data(Qt::UserRole).toInt();

    AuditLogEntry entry;
    bool found = false;
    for (const auto& e : m_entries) {
        if (e.id == entryId) { entry = e; found = true; break; }
    }
    if (!found) return;

    if (!confirmUndoAuth(entry)) return;

    if (performUndo(entry)) {
        QMessageBox::information(this, "Undo Complete",
            QString("%1 on %2 #%3 has been successfully undone.")
                .arg(entry.action, entry.entityType).arg(entry.entityId));

        // Refresh audit log itself
        refresh();

        // Signal MainWindow to refresh the affected data page
        emit dataRestored(entry.entityType);
    }
}

void AuditLogPage::retranslateUi() { retranslateWidget(this); }
