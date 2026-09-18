#include "customers_page.h"
#include "customer_dialog.h"
#include "services/archive_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include "services/session_manager.h"
#include "services/audit_service.h"
#include "ui/tr_helper.h"
#include "ui/arabic_sort_proxy.h"
#include "ui/pagination_bar.h"
#include <QSqlDatabase>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QSortFilterProxyModel>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileDialog>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QFile>
#include "xlsxdocument.h"
#include "xlsxformat.h"

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

CustomersPage::CustomersPage(QWidget* parent)
    : QWidget(parent)
{
    // Pick the right repository based on the active connection
    if (DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE")
    {
        m_repo        = new SQLiteCustomerRepository;
        m_regionRepo  = new SQLiteRegionRepository;
        m_productRepo = new SQLiteProductRepository;
        m_orderRepo   = new SQLiteOrderRepository;    // Task 9
    } else {
        m_repo        = new AccessCustomerRepository;
        m_regionRepo  = new AccessRegionRepository;
        m_productRepo = new AccessProductRepository;
        m_orderRepo   = new AccessOrderRepository;    // Task 9
    }

    // Data loaded by MainWindow::navigateTo — no singleShot needed
    setupUi();
    loadRegionsAndProducts();

    // Task 9: status automation timer — check every 2 minutes + immediately on startup
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(2 * 60 * 1000);   // 2 min — catches order saves promptly
    connect(m_statusTimer, &QTimer::timeout, this, &CustomersPage::runStatusAutomation);
    m_statusTimer->start();
    // Run once immediately after event loop starts
    QTimer::singleShot(500, this, &CustomersPage::runStatusAutomation);
}

void CustomersPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 10, 16, 10);
    toolbarLayout->setSpacing(10);

    auto* titleLabel = new QLabel;
    TR_LABEL(titleLabel, "Customers");
    titleLabel->setObjectName("pageTitle");
    titleLabel->setVisible(false);
    m_pageTitleLabel = titleLabel;

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search by name, phone, or address..."));
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMinimumWidth(300);
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);

    m_addBtn = new QPushButton;
    TR_BTN(m_addBtn, "＋ Add Customer");
    m_addBtn->setObjectName("primaryBtn");

    m_editBtn = new QPushButton;
    TR_BTN(m_editBtn, "Edit");
    m_editBtn->setObjectName("secondaryBtn");
    m_editBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                            QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editBtn->setIconSize(QSize(18, 18));
    m_editBtn->setEnabled(false);

    m_deleteBtn = new QPushButton;
    TR_BTN(m_deleteBtn, "Delete");
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                              QColor("#FFFFFF"), 18));
    m_deleteBtn->setIconSize(QSize(18, 18));
    m_deleteBtn->setEnabled(false);

    toolbarLayout->addWidget(titleLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_searchEdit);

    // Manage Regions button
    m_manageRegionsBtn = new QPushButton(LangManager::instance().t("Regions"));
    m_manageRegionsBtn->setObjectName("secondaryBtn");
    m_manageRegionsBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("category-svgrepo-com.svg"), 
                                                      QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_manageRegionsBtn->setIconSize(QSize(18, 18));
    toolbarLayout->addWidget(m_manageRegionsBtn);

    m_importBtn = new QPushButton(LangManager::instance().t("Import Data"));
    m_importBtn->setObjectName("secondaryBtn");
    m_importBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("import-svgrepo-com.svg"), 
                                              QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_importBtn->setIconSize(QSize(18, 18));
    toolbarLayout->addWidget(m_importBtn);

    toolbarLayout->addWidget(m_addBtn);
    toolbarLayout->addWidget(m_editBtn);
    toolbarLayout->addWidget(m_deleteBtn);

    root->addWidget(toolbar);

    // ── Table ──────────────────────────────────────────────────────────────
    m_model = new CustomerTableModel(this);
    m_model = new CustomerTableModel(this);
    auto* custProxy = new ArabicSortProxy(this);
    custProxy->setSourceModel(m_model);

    m_tableView = new QTableView;
    m_tableView->setObjectName("dataTable");
    m_tableView->setModel(custProxy);
    m_tableView->setSortingEnabled(true);
    m_tableView->horizontalHeader()->setSortIndicatorShown(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(CustomerTableModel::ColName, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(CustomerTableModel::ColAddress, QHeaderView::Stretch);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setColumnHidden(CustomerTableModel::ColId, true);

    root->addWidget(m_tableView, 1);

    // ── Pagination bar ─────────────────────────────────────────────────────
    m_pagination = new PaginationBar;
    root->addWidget(m_pagination);

    // ── Status bar ─────────────────────────────────────────────────────────
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 4, 16, 4);
    m_statusLabel = new QLabel("0 customers");
    m_statusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(m_statusLabel);
    root->addWidget(statusBar);

    // ── Connections ─────────────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CustomersPage::onSearch);
    connect(m_addBtn,    &QPushButton::clicked,   this, &CustomersPage::onAddCustomer);
    connect(m_editBtn,   &QPushButton::clicked,   this, &CustomersPage::onEditCustomer);
    connect(m_deleteBtn, &QPushButton::clicked,   this, &CustomersPage::onDeleteCustomer);
    connect(m_manageRegionsBtn, &QPushButton::clicked, this, &CustomersPage::onManageRegions);
    connect(m_importBtn,        &QPushButton::clicked, this, &CustomersPage::onImportCustomers);

    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CustomersPage::onSelectionChanged);

    connect(m_tableView, &QTableView::doubleClicked,
            this, [this](const QModelIndex&) { onEditCustomer(); });

    // Pagination
    connect(m_pagination, &PaginationBar::pageChanged,
            this, [this](int, int){ loadCustomers(); });
}

void CustomersPage::loadRegionsAndProducts() {
    m_regions  = m_regionRepo->getAll();
    m_products = m_productRepo->getAll();
}

void CustomersPage::loadCustomers() {
    QString keyword = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    int total = 0;
    QList<Customer> page;

    if (!keyword.isEmpty()) {
        // SEARCH: still load all matches, then paginate client-side over the
        // (much smaller) result set — search isn't the reported bottleneck.
        QList<Customer> results = m_repo->search(keyword);
        total = results.size();
        m_pagination->setTotalItems(total);
        page = results.mid(m_pagination->offset(), m_pagination->pageSize());
    } else {
        // TRUE server-side pagination: ask the database for exactly the
        // page currently on screen (25/50/100/200, whatever is selected)
        // instead of always pulling a fixed 200-row chunk and slicing it
        // client-side. That old approach is why the view never showed
        // anything past row 200 — flipping past it had nothing left to
        // slice. Every page flip now queries the DB directly for that page.
        total = m_repo->getTotalCount();
        m_pagination->setTotalItems(total);
        page = m_repo->getPage(m_pagination->offset(), m_pagination->pageSize());

        // ── Guard against empty result caused by a dropped DB connection ──
        if (page.isEmpty() && total > 0) {
            if (!QSqlDatabase::database().isOpen()) {
                Logger::instance().warn("CustomersPage: DB not open — attempting reconnect");
                DatabaseConnectionManager::instance().openConnection();
                page = m_repo->getPage(m_pagination->offset(), m_pagination->pageSize());
            }
            if (page.isEmpty()) {
                m_statusLabel->setText("⚠ Could not refresh — showing last known data");
                Logger::instance().warn("CustomersPage::loadCustomers: empty result kept (possible DB timeout)");
                return;
            }
        }
    }

    m_model->setCustomers(page);

    QMap<int, QString> regionMap;
    for (const auto& r : m_regions) regionMap[r.id] = r.name;
    m_model->setRegionMap(regionMap);

    m_statusLabel->setText(QString("%1 customer(s) total").arg(total));
}

void CustomersPage::refresh() {
    // Re-run status automation on every refresh so status reflects
    // any orders that were placed since the last check
    runStatusAutomation();
    // NOTE: regions/products are loaded lazily on dialog open — do NOT call
    // loadRegionsAndProducts() here because getAll() on 16k+ products is slow
    // and this function is called every 2 minutes by the status timer.
    loadCustomers();
}

void CustomersPage::onSearch(const QString& text) {
    Q_UNUSED(text)
    m_pagination->resetToFirst();
    loadCustomers();
}

void CustomersPage::onAddCustomer() {
    if (!SessionManager::instance().currentRole().canAddCustomer) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to add customers.");
        return;
    }
    // Lazy-load regions & products only when dialog is about to open
    // (avoids loading 16k+ products on every 2-minute refresh cycle)
    loadRegionsAndProducts();
    CustomerDialog dlg(m_regions, m_products, this);
    dlg.setExistingCustomers(m_repo->getAll());
    if (dlg.exec() != QDialog::Accepted) return;

    Customer c = dlg.getCustomer();
    if (!m_repo->save(c)) {
        QMessageBox::critical(this, "Error", "Failed to save customer.");
        return;
    }
    loadCustomers();
    Logger::instance().info("Customer added: " + c.name());
    AuditService::instance().logCreate("Customer", c.id(), c.name());
}

void CustomersPage::onEditCustomer() {
    if (!SessionManager::instance().currentRole().canEditCustomer) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to edit customers.");
        return;
    }
    QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    auto* proxy = qobject_cast<ArabicSortProxy*>(m_tableView->model());
    int row = proxy ? proxy->mapToSource(sel.first()).row() : sel.first().row();
    Customer original = m_model->customerAt(row);

    // Reload fresh from DB to get phones/addresses/favorites
    Customer full = m_repo->getById(original.id());

    // Lazy-load regions & products only when dialog is about to open
    loadRegionsAndProducts();
    CustomerDialog dlg(m_regions, m_products, this);
    dlg.setCustomer(full);
    if (dlg.exec() != QDialog::Accepted) return;

    Customer updated = dlg.getCustomer();
    if (!m_repo->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update customer.");
        return;
    }
    loadCustomers();
    Logger::instance().info("Customer updated: " + updated.name());

    // Snapshot of BEFORE state for undo
    auto makeCustomerSnap = [](const Customer& c) -> QJsonObject {
        QJsonObject s;
        s["id"]               = c.id();
        s["name"]             = c.name();
        s["notes"]            = c.notes();
        s["regionId"]         = c.regionId();
        s["distanceKm"]       = c.distanceKm();
        s["status"]           = Customer::statusToString(c.status());
        s["preferredPayment"] = c.preferredPaymentMethod();
        QJsonArray phonesArr;
        for (const auto& ph : c.phones()) phonesArr.append(ph.number);
        s["phones"] = phonesArr;
        QJsonArray addrArr;
        for (const auto& ad : c.addresses()) addrArr.append(ad.text);
        s["addresses"] = addrArr;
        return s;
    };
    QString details = QJsonDocument(QJsonObject{{"before", makeCustomerSnap(full)}})
                          .toJson(QJsonDocument::Compact);
    AuditService::instance().logUpdate("Customer", updated.id(), details);
}

void CustomersPage::onDeleteCustomer() {
    if (!SessionManager::instance().currentRole().canDeleteCustomer) {
        QMessageBox::warning(this, "Access Denied", "You don't have permission to delete customers.");
        return;
    }
    QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    auto* proxy2 = qobject_cast<ArabicSortProxy*>(m_tableView->model());
    int row = proxy2 ? proxy2->mapToSource(sel.first()).row() : sel.first().row();
    // Copy by value — avoids dangling reference after model mutation
    Customer c = m_model->customerAt(row);

    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete customer \"%1\"?\nThis will also remove their orders and scheduled orders.")
            .arg(c.name()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Reload full customer from DB before deleting — captures phones/addresses
    Customer fullC = m_repo->getById(c.id());

    // Task 11: soft-delete — archive first, then hard-delete, return archive id
    QString archiveErr;
    int archiveId = ArchiveManager::instance().softDelete("Customers", "Id", c.id(), &archiveErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive customer before deletion:\n" + archiveErr);
        return;
    }

    Logger::instance().info("Customer deleted: " + c.name());

    // Full JSON snapshot — phones and addresses stored as arrays
    QJsonObject snap;
    snap["id"]       = fullC.id();
    snap["name"]     = fullC.name();
    snap["notes"]    = fullC.notes();
    snap["regionId"] = fullC.regionId();
    snap["distanceKm"] = fullC.distanceKm();
    snap["status"]   = Customer::statusToString(fullC.status());
    snap["preferredPayment"] = fullC.preferredPaymentMethod();

    QJsonArray phonesArr;
    for (const auto& ph : fullC.phones()) phonesArr.append(ph.number);
    snap["phones"] = phonesArr;

    QJsonArray addrArr;
    for (const auto& ad : fullC.addresses()) addrArr.append(ad.text);
    snap["addresses"] = addrArr;

    QString details = QJsonDocument(QJsonObject{
        {"before", snap},
        {"archiveId", archiveId}
    }).toJson(QJsonDocument::Compact);
    AuditService::instance().logDelete("Customer", c.id(), details);

    m_model->removeCustomer(row);
    m_statusLabel->setText(QString::number(m_model->rowCount()) + " customer(s)");
}

void CustomersPage::onSelectionChanged() {
    bool hasSelection = !m_tableView->selectionModel()->selectedRows().isEmpty();
    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
}

// ── Task 1: helper to show Add/Edit region dialog ────────────────────────────
static bool showRegionEditDialog(Region& r, QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(r.id > 0 ? "Edit Region" : "Add Region");
    dlg.setMinimumWidth(360);
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);
    form->setSpacing(10);
    form->setContentsMargins(16, 16, 16, 16);

    auto* nameEdit = new QLineEdit(r.name);
    nameEdit->setPlaceholderText("e.g. Downtown");
    form->addRow("Name *", nameEdit);

    auto* distSpin = new QDoubleSpinBox;
    distSpin->setRange(0.0, 9999.9);
    distSpin->setDecimals(1);
    distSpin->setSuffix(" km");
    distSpin->setValue(r.distanceKm);
    form->addRow("Distance from shop", distSpin);

    auto* feeSpin = new QDoubleSpinBox;
    feeSpin->setRange(0.0, 99999.99);
    feeSpin->setDecimals(2);
    feeSpin->setValue(r.deliveryFee);
    form->addRow("Delivery Fee", feeSpin);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    form->addRow(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Region name is required.");
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;

    r.name        = nameEdit->text().trimmed();
    r.distanceKm  = distSpin->value();
    r.deliveryFee = feeSpin->value();
    return true;
}

// ── Task 9: Customer Status Automation ───────────────────────────────────────
// Rules (based on most recent order date from Orders table):
//   last order > 14 days ago  →  Inactive
//   last order > 7 days ago   →  Hesitant
//   otherwise                 →  Active
// Customers with status Suspended are never auto-changed.
// lastOrderDate on the Customer record is updated here too so it stays current.
void CustomersPage::runStatusAutomation() {
    if (!m_repo || !m_orderRepo) return;

    QDateTime now = QDateTime::currentDateTime();

    // Build a map: customerId → most-recent order dateTime (non-cancelled only)
    QMap<int, QDateTime> lastOrderMap;
    const QList<Order> allOrders = m_orderRepo->getAll();
    for (const auto& o : allOrders) {
        if (o.status() == "Cancelled") continue;
        if (!o.dateTime().isValid())   continue;
        auto& existing = lastOrderMap[o.customerId()];
        if (!existing.isValid() || o.dateTime() > existing)
            existing = o.dateTime();
    }

    const QList<Customer> customers = m_repo->getAll();
    int changed = 0;
    for (Customer c : customers) {
        if (c.status() == Customer::Status::Suspended) continue;  // never auto-touch

        QDateTime lastOrder = lastOrderMap.value(c.id());

        Customer::Status newStatus;
        if (!lastOrder.isValid()) {
            // No orders ever — treat as Inactive
            newStatus = Customer::Status::Inactive;
        } else {
            qint64 daysSince = lastOrder.daysTo(now);
            if      (daysSince >= 14) newStatus = Customer::Status::Inactive;
            else if (daysSince >= 7)  newStatus = Customer::Status::Hesitant;
            else                      newStatus = Customer::Status::Active;
        }

        bool statusChanged = (c.status() != newStatus);
        bool dateChanged   = lastOrder.isValid() && (c.lastOrderDate() != lastOrder);

        if (statusChanged || dateChanged) {
            c.setStatus(newStatus);
            if (lastOrder.isValid()) c.setLastOrderDate(lastOrder);
            m_repo->save(c);
            ++changed;
        }
    }

    if (changed > 0) {
        Logger::instance().info(
            QString("Status automation: updated %1 customer(s)").arg(changed));
        // Refresh the visible table if customers are currently loaded
        loadCustomers();
    }
}

void CustomersPage::onManageRegions() {
    // ── Inline Regions CRUD dialog ────────────────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Regions");
    dlg.setMinimumWidth(520);
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    // Table of existing regions
    auto* table = new QTableWidget(0, 3);
    table->setObjectName("dataTable");
    table->setHorizontalHeaderLabels({
        LangManager::instance().t("Name"),
        LangManager::instance().t("Distance"),
        LangManager::instance().t("Delivery Fee")
    });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    layout->addWidget(table);

    // Button row
    auto* btnRow = new QHBoxLayout;
    auto& Lr = LangManager::instance();
    auto* addBtn  = new QPushButton(Lr.t("＋ Add Customer"));
    addBtn->setObjectName("primaryBtn");
    auto* editBtn = new QPushButton(Lr.t("✏️ Edit"));
    editBtn->setObjectName("secondaryBtn");
    editBtn->setEnabled(false);
    auto* delBtn  = new QPushButton(Lr.t("Delete"));
    delBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                        QColor(ThemeManager::instance().tokens().textPrimary), 18));
    delBtn->setObjectName("dangerBtn");
    delBtn->setEnabled(false);
    auto* closeBtn = new QPushButton(Lr.t("Close"));
    closeBtn->setObjectName("secondaryBtn");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    auto reload = [&]() {
        m_regions = m_regionRepo->getAll();
        table->setRowCount(0);
        for (const auto& r : m_regions) {
            int row = table->rowCount();
            table->insertRow(row);
            auto mkCell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
                auto* it = new QTableWidgetItem(t);
                it->setTextAlignment(a);
                it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                return it;
            };
            table->setItem(row, 0, mkCell(r.name, Qt::AlignLeft | Qt::AlignVCenter));
            table->setItem(row, 1, mkCell(QString::number(r.distanceKm, 'f', 1) + " km"));
            table->setItem(row, 2, mkCell(QString::number(r.deliveryFee, 'f', 2)));
            // store region id in hidden Qt::UserRole on column 0
            table->item(row, 0)->setData(Qt::UserRole, r.id);
        }
        editBtn->setEnabled(false);
        delBtn->setEnabled(false);
    };
    reload();

    // Enable edit/delete when a row is selected
    QObject::connect(table, &QTableWidget::itemSelectionChanged, &dlg, [&]() {
        bool has = table->currentRow() >= 0;
        editBtn->setEnabled(has);
        delBtn->setEnabled(has);
    });
    // Double-click to edit
    QObject::connect(table, &QTableWidget::cellDoubleClicked, editBtn, &QPushButton::click);

    // Add region
    QObject::connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
        Region r;
        if (!showRegionEditDialog(r, &dlg)) return;
        if (m_regionRepo->save(r)) {
            reload();
            Logger::instance().info("Region added: " + r.name);
        } else {
            QMessageBox::warning(&dlg, "Error", "Failed to add region.");
        }
    });

    // Edit region
    QObject::connect(editBtn, &QPushButton::clicked, &dlg, [&]() {
        int row = table->currentRow();
        if (row < 0 || row >= m_regions.size()) return;
        Region r = m_regions.at(row);
        if (!showRegionEditDialog(r, &dlg)) return;
        if (m_regionRepo->save(r)) {
            reload();
            Logger::instance().info("Region updated: " + r.name);
        } else {
            QMessageBox::warning(&dlg, "Error", "Failed to update region.");
        }
    });

    // Delete region
    QObject::connect(delBtn, &QPushButton::clicked, &dlg, [&]() {
        int row = table->currentRow();
        if (row < 0 || row >= m_regions.size()) return;
        const Region& r = m_regions.at(row);
        auto reply = QMessageBox::question(&dlg, "Confirm",
            "Delete region \"" + r.name + "\"?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_regionRepo->remove(r.id);
            reload();
        }
    });

    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();

    // Refresh regions in parent after dialog closes
    m_regions = m_regionRepo->getAll();
}

void CustomersPage::retranslateUi() {
    retranslateWidget(this);
}

// ── Task C: Import Customers from Excel ──────────────────────────────────────
// Unique key: first phone number (semicolon-separated within a cell).
// Multi-value fields (phones, addresses): separate multiple values with ';'
// e.g. "01012345678;01098765432"
void CustomersPage::onImportCustomers() {
    // ── Import dialog with instructions and template download ─────────────────
    QDialog importDlg(this);
    importDlg.setWindowTitle("Import Customers from Excel");
    importDlg.setMinimumWidth(560);
    importDlg.setModal(true);

    auto* mainLayout = new QVBoxLayout(&importDlg);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 16, 20, 16);

    auto* instrLabel = new QLabel(
        "<b>Import format (one customer per row):</b><br>"
        "Columns: <code>Name | Phone Numbers | Region | Addresses | Notes | Status | "
        "Preferred Payment | Distance Km</code><br>"
        "<span style='color:#94A3B8;font-size:11px;'>"
        "<b>Multi-value fields</b> (Phone Numbers, Addresses): separate multiple values "
        "with a semicolon <b>;</b> within a single cell.<br>"
        "Example phones cell: <code>01012345678;01098765432</code><br>"
        "Duplicate detection uses the <b>first phone number</b> as the unique key.<br>"
        "Incomplete rows are imported with available fields; missing fields are left blank."
        "</span>");
    instrLabel->setWordWrap(true);
    mainLayout->addWidget(instrLabel);

    auto* btnRow = new QHBoxLayout;
    auto* templateBtn = new QPushButton("⬇ Download Template");
    templateBtn->setObjectName("secondaryBtn");
    btnRow->addWidget(templateBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Open)->setText("📂 Choose File…");
    btns->button(QDialogButtonBox::Open)->setObjectName("primaryBtn");
    mainLayout->addWidget(btns);

    // Template download
    QObject::connect(templateBtn, &QPushButton::clicked, &importDlg, [&]() {
        QString savePath = QFileDialog::getSaveFileName(
            &importDlg, "Save Template", "customers_template.xlsx",
            "Excel Files (*.xlsx)");
        if (savePath.isEmpty()) return;
        QXlsx::Document tmpl;
        QXlsx::Format hdr;
        hdr.setFontBold(true);
        hdr.setPatternBackgroundColor(QColor("#1E293B"));
        hdr.setFontColor(QColor("#FFFFFF"));
        tmpl.write(1, 1, "Name",              hdr);
        tmpl.write(1, 2, "Phone Numbers",     hdr);
        tmpl.write(1, 3, "Region",            hdr);
        tmpl.write(1, 4, "Addresses",         hdr);
        tmpl.write(1, 5, "Notes",             hdr);
        tmpl.write(1, 6, "Status",            hdr);
        tmpl.write(1, 7, "Preferred Payment", hdr);
        tmpl.write(1, 8, "Distance Km",       hdr);
        // Example row showing semicolon convention
        tmpl.write(2, 1, "Ahmed Hassan");
        tmpl.write(2, 2, "01012345678;01098765432");
        tmpl.write(2, 3, "Downtown");
        tmpl.write(2, 4, "123 Main St;45 Second Ave");
        tmpl.write(2, 5, "Regular customer");
        tmpl.write(2, 6, "Active");
        tmpl.write(2, 7, "Cash");
        tmpl.write(2, 8, 5.0);
        if (tmpl.saveAs(savePath))
            QMessageBox::information(&importDlg, "Template Saved",
                "Template saved to:\n" + savePath);
        else
            QMessageBox::warning(&importDlg, "Error", "Failed to save template.");
    });

    QObject::connect(btns, &QDialogButtonBox::rejected, &importDlg, &QDialog::reject);
    QObject::connect(btns, &QDialogButtonBox::accepted, &importDlg, &QDialog::accept);

    if (importDlg.exec() != QDialog::Accepted) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Excel File", QString(), "Excel Files (*.xlsx)");
    if (filePath.isEmpty()) return;

    QXlsx::Document doc(filePath);
    if (!doc.load()) {
        QMessageBox::critical(this, "Error", "Failed to open Excel file:\n" + filePath);
        return;
    }

    // Build phone → Customer lookup for duplicate detection
    QList<Customer> allCustomers = m_repo->getAll();
    QMap<QString, Customer> phoneIndex; // normalised phone → Customer
    for (const auto& c : allCustomers) {
        for (const auto& ph : c.phones()) {
            QString norm = ph.number.trimmed().remove(' ');
            if (!norm.isEmpty())
                phoneIndex[norm] = c;
        }
    }

    // Build region name → id lookup (case-insensitive)
    QMap<QString, int> regionNameIndex;
    for (const auto& r : m_regions)
        regionNameIndex[r.name.toLower()] = r.id;

    // Helper: get or create a region by name, returns its id (0 on failure)
    auto getOrCreateRegionId = [&](const QString& rname) -> int {
        if (rname.isEmpty()) return 0;
        QString key = rname.toLower();
        if (regionNameIndex.contains(key))
            return regionNameIndex[key];
        // Not found — create it
        Region newRegion;
        newRegion.name = rname;
        if (m_regionRepo->save(newRegion) && newRegion.id > 0) {
            regionNameIndex[key] = newRegion.id;
            m_regions.append(newRegion);
            Logger::instance().info("Auto-created region from import: " + rname);
            return newRegion.id;
        }
        return 0;
    };

    int totalRows  = doc.dimension().lastRow();
    int imported   = 0;
    int replaced   = 0;
    bool abortAll  = false;
    bool skipAll   = false;
    bool replaceAll = false;

    for (int row = 1; row <= totalRows && !abortAll; ++row) {
        QString name       = doc.read(row, 1).toString().trimmed();
        QString phonesRaw  = doc.read(row, 2).toString().trimmed();
        QString regionName = doc.read(row, 3).toString().trimmed();
        QString addrsRaw   = doc.read(row, 4).toString().trimmed();
        QString notes      = doc.read(row, 5).toString().trimmed();
        QString statusStr  = doc.read(row, 6).toString().trimmed();
        QString payment    = doc.read(row, 7).toString().trimmed();
        QString distStr    = doc.read(row, 8).toString().trimmed();

        // Skip header row
        if (row == 1 && name.compare("name", Qt::CaseInsensitive) == 0) continue;
        // Skip completely empty rows
        if (name.isEmpty() && phonesRaw.isEmpty()) continue;

        // Parse multi-value phone / address cells
        QList<Phone> phones;
        for (const QString& p : phonesRaw.split(';', Qt::SkipEmptyParts))
            phones.append(Phone{p.trimmed()});

        QList<Address> addresses;
        for (const QString& a : addrsRaw.split(';', Qt::SkipEmptyParts))
            addresses.append(Address{a.trimmed()});

        // Duplicate detection: match on first phone number
        QString firstPhone = phones.isEmpty() ? QString() : phones.first().number.trimmed().remove(' ');
        bool isDuplicate = !firstPhone.isEmpty() && phoneIndex.contains(firstPhone);

        if (isDuplicate && skipAll) { continue; }

        if (isDuplicate && replaceAll) {
            Customer existing = phoneIndex[firstPhone];
            if (!name.isEmpty())    existing.setName(name);
            if (!phones.isEmpty())  existing.setPhones(phones);
            if (!addresses.isEmpty()) existing.setAddresses(addresses);
            if (!notes.isEmpty())   existing.setNotes(notes);
            if (!statusStr.isEmpty()) existing.setStatus(Customer::stringToStatus(statusStr));
            if (!payment.isEmpty()) existing.setPreferredPaymentMethod(payment);
            if (!distStr.isEmpty()) existing.setDistanceKm(distStr.toDouble());
            if (!regionName.isEmpty()) {
                int rid = getOrCreateRegionId(regionName);
                if (rid > 0) existing.setRegionId(rid);
            }
            if (m_repo->save(existing)) {
                ++replaced;
                for (const auto& ph : existing.phones())
                    phoneIndex[ph.number.trimmed().remove(' ')] = existing;
            }
            continue;
        }

        if (isDuplicate) {
            Customer existing = phoneIndex[firstPhone];

            // Build comparison dialog
            QDialog conflictDlg(this);
            conflictDlg.setWindowTitle(QString("Duplicate Found — Row %1").arg(row));
            conflictDlg.setMinimumWidth(640);
            conflictDlg.setModal(true);

            auto* cl = new QVBoxLayout(&conflictDlg);
            cl->setSpacing(12);
            cl->setContentsMargins(20, 16, 20, 16);

            auto* hdrLbl = new QLabel(
                QString("<b>A customer with phone <code>%1</code> already exists.</b><br>"
                        "Choose how to handle this row:").arg(firstPhone));
            hdrLbl->setWordWrap(true);
            cl->addWidget(hdrLbl);

            // Side-by-side comparison table
            auto* tbl = new QTableWidget(8, 2);
            tbl->setHorizontalHeaderLabels({"Currently Registered", "Incoming (Excel)"});
            tbl->verticalHeader()->setVisible(true);
            tbl->setVerticalHeaderLabels({"Name","Phones","Region","Addresses","Notes","Status","Payment","Distance"});
            tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tbl->setSelectionMode(QAbstractItemView::NoSelection);
            tbl->setMaximumHeight(240);

            auto mkTbl = [](const QString& t) {
                auto* it = new QTableWidgetItem(t);
                it->setFlags(Qt::ItemIsEnabled);
                return it;
            };

            QString exPhones;
            for (const auto& p : existing.phones()) exPhones += p.number + "; ";
            QString exAddrs;
            for (const auto& a : existing.addresses()) exAddrs += a.text + "; ";
            QString inPhones = phonesRaw;
            QString inAddrs  = addrsRaw;

            // Find existing region name
            QString exRegionName;
            for (const auto& r : m_regions)
                if (r.id == existing.regionId()) { exRegionName = r.name; break; }

            tbl->setItem(0,0,mkTbl(existing.name()));              tbl->setItem(0,1,mkTbl(name));
            tbl->setItem(1,0,mkTbl(exPhones));                     tbl->setItem(1,1,mkTbl(inPhones));
            tbl->setItem(2,0,mkTbl(exRegionName));                 tbl->setItem(2,1,mkTbl(regionName));
            tbl->setItem(3,0,mkTbl(exAddrs));                      tbl->setItem(3,1,mkTbl(inAddrs));
            tbl->setItem(4,0,mkTbl(existing.notes()));             tbl->setItem(4,1,mkTbl(notes));
            tbl->setItem(5,0,mkTbl(Customer::statusToString(existing.status()))); tbl->setItem(5,1,mkTbl(statusStr));
            tbl->setItem(6,0,mkTbl(existing.preferredPaymentMethod()));           tbl->setItem(6,1,mkTbl(payment));
            tbl->setItem(7,0,mkTbl(QString::number(existing.distanceKm(),'f',2)));tbl->setItem(7,1,mkTbl(distStr));
            cl->addWidget(tbl);

            auto* choiceRow = new QHBoxLayout;
            auto* skipBtn       = new QPushButton("⏭ Skip");
            auto* skipAllBtn    = new QPushButton("⏭ Skip All");
            auto* replaceBtn    = new QPushButton("🔄 Replace");
            auto* replaceAllBtn = new QPushButton("🔄 Replace All");
            auto* cancelBtn     = new QPushButton("⛔ Cancel Import");
            skipBtn->setObjectName("secondaryBtn");
            skipAllBtn->setObjectName("secondaryBtn");
            replaceBtn->setObjectName("primaryBtn");
            replaceAllBtn->setObjectName("primaryBtn");
            cancelBtn->setObjectName("dangerBtn");
            choiceRow->addWidget(skipBtn);
            choiceRow->addWidget(skipAllBtn);
            choiceRow->addStretch();
            choiceRow->addWidget(replaceBtn);
            choiceRow->addWidget(replaceAllBtn);
            choiceRow->addWidget(cancelBtn);
            cl->addLayout(choiceRow);

            enum Choice { Skip, SkipAll, Replace, ReplaceAll, Cancel } choice = Skip;
            QObject::connect(skipBtn,       &QPushButton::clicked, &conflictDlg, [&](){ choice=Skip;       conflictDlg.accept(); });
            QObject::connect(skipAllBtn,    &QPushButton::clicked, &conflictDlg, [&](){ choice=SkipAll;    conflictDlg.accept(); });
            QObject::connect(replaceBtn,    &QPushButton::clicked, &conflictDlg, [&](){ choice=Replace;    conflictDlg.accept(); });
            QObject::connect(replaceAllBtn, &QPushButton::clicked, &conflictDlg, [&](){ choice=ReplaceAll; conflictDlg.accept(); });
            QObject::connect(cancelBtn,     &QPushButton::clicked, &conflictDlg, [&](){ choice=Cancel;     conflictDlg.accept(); });

            conflictDlg.exec();

            if (choice == Cancel)     { abortAll = true; break; }
            if (choice == SkipAll)    { skipAll = true; continue; }
            if (choice == Skip)       { continue; }
            if (choice == ReplaceAll) { replaceAll = true; }

            // Replace: update the existing customer with incoming data
            if (!name.isEmpty())    existing.setName(name);
            if (!phones.isEmpty())  existing.setPhones(phones);
            if (!addresses.isEmpty()) existing.setAddresses(addresses);
            if (!notes.isEmpty())   existing.setNotes(notes);
            if (!statusStr.isEmpty()) existing.setStatus(Customer::stringToStatus(statusStr));
            if (!payment.isEmpty()) existing.setPreferredPaymentMethod(payment);
            if (!distStr.isEmpty()) existing.setDistanceKm(distStr.toDouble());
            if (!regionName.isEmpty()) {
                int rid = getOrCreateRegionId(regionName);
                if (rid > 0) existing.setRegionId(rid);
            }

            if (m_repo->save(existing)) {
                ++replaced;
                // Update phone index
                for (const auto& ph : existing.phones())
                    phoneIndex[ph.number.trimmed().remove(' ')] = existing;
            }
            continue;
        }

        // No duplicate — insert new customer
        Customer c;
        c.setName(name.isEmpty() ? "(Unnamed)" : name);
        c.setPhones(phones);
        c.setAddresses(addresses);
        c.setNotes(notes);
        if (!statusStr.isEmpty()) c.setStatus(Customer::stringToStatus(statusStr));
        if (!payment.isEmpty())   c.setPreferredPaymentMethod(payment);
        if (!distStr.isEmpty())   c.setDistanceKm(distStr.toDouble());
        if (!regionName.isEmpty()) {
            int rid = getOrCreateRegionId(regionName);
            if (rid > 0) c.setRegionId(rid);
        }

        if (m_repo->save(c)) {
            ++imported;
            // Update phone index so later rows can detect duplicates within the file
            for (const auto& ph : c.phones())
                phoneIndex[ph.number.trimmed().remove(' ')] = c;
            Logger::instance().info("Customer imported: " + c.name());
        }
    }

    loadCustomers();
    Logger::instance().info(QString("Customer import: %1 new, %2 replaced").arg(imported).arg(replaced));

    QString summary = QString("Import complete.\n%1 new customer(s) added.").arg(imported);
    if (replaced > 0)
        summary += QString("\n%1 existing record(s) replaced.").arg(replaced);
    if (abortAll)
        summary += "\n(Import was cancelled before all rows were processed.)";
    QMessageBox::information(this, "Import Complete", summary);
}
