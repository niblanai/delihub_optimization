#include "suppliers_page.h"
#include "services/lang_manager.h"
#include "services/archive_manager.h"
#include "services/audit_service.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/tr_helper.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QTextEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QTimer>
#include <QCoreApplication>
#include <QFile>

static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    QString devPath = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(devPath)) return devPath;
    return file;
}

// Forward declaration
static Supplier showSupplierDialog(const Supplier& existing, QWidget* parent);

SuppliersPage::SuppliersPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_repo = useSqlite ? static_cast<ISupplierRepository*>(new SQLiteSupplierRepository)
                       : static_cast<ISupplierRepository*>(new AccessSupplierRepository);
    setupUi();
}

void SuppliersPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0); root->setContentsMargins(0,0,0,0);

    // Toolbar
    auto* toolbar = new QFrame; toolbar->setObjectName("pageToolbar");
    auto* tbL = new QHBoxLayout(toolbar); tbL->setContentsMargins(16,10,16,10);
    
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search suppliers..."));
    m_searchEdit->setMaximumWidth(300);
    
    m_addBtn    = new QPushButton(LangManager::instance().t("＋ Add Supplier"));
    m_addBtn->setObjectName("primaryBtn");
    
    tbL->addWidget(m_searchEdit);
    tbL->addStretch();
    tbL->addWidget(m_addBtn);
    root->addWidget(toolbar);

    // Table
    m_table = new QTableWidget(0, 7);  // 7 columns now with Actions
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("Name"),
        LangManager::instance().t("Contact Person"),
        LangManager::instance().t("Phone"),
        LangManager::instance().t("Email"),
        LangManager::instance().t("Address"),
        LangManager::instance().t("Status")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    // Status bar
    auto* sb = new QFrame; sb->setObjectName("statusBar");
    auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(16,4,16,4);
    m_statusLbl = new QLabel("0 suppliers");
    m_statusLbl->setObjectName("statusLabel");
    sbl->addWidget(m_statusLbl);
    root->addWidget(sb);

    // Connections
    connect(m_addBtn,    &QPushButton::clicked, this, &SuppliersPage::onAdd);
    connect(m_searchEdit,&QLineEdit::textChanged, this, &SuppliersPage::onSearch);
    
    // Load data after UI is shown
    QTimer::singleShot(0, this, &SuppliersPage::loadSuppliers);
}

void SuppliersPage::refresh() { loadSuppliers(); }
void SuppliersPage::retranslateUi() { retranslateWidget(this); }

void SuppliersPage::loadSuppliers() {
    m_suppliers = m_repo->getAll();
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    
    // Update table columns to include Actions
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("Name"),
        LangManager::instance().t("Contact Person"),
        LangManager::instance().t("Phone"),
        LangManager::instance().t("Email"),
        LangManager::instance().t("Address"),
        LangManager::instance().t("Status"),
        LangManager::instance().t("Actions")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    
    QString filter = m_searchEdit->text().trimmed().toLower();
    int displayCount = 0;
    
    for (const auto& s : m_suppliers) {
        // Simple search filter
        if (!filter.isEmpty()) {
            if (!s.name().toLower().contains(filter) &&
                !s.contactName().toLower().contains(filter) &&
                !s.phone().toLower().contains(filter) &&
                !s.email().toLower().contains(filter)) {
                continue;
            }
        }
        
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        
        auto* nameItem = cell(s.name());
        nameItem->setData(Qt::UserRole, s.id());  // Store supplier ID
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, cell(s.contactName()));
        m_table->setItem(row, 2, cell(s.phone()));
        m_table->setItem(row, 3, cell(s.email()));
        m_table->setItem(row, 4, cell(s.address()));
        
        auto* statusItem = cell(s.active() ? "✅ Active" : "❌ Inactive", Qt::AlignCenter);
        statusItem->setForeground(s.active() ? QColor("#4ADE80") : QColor("#F87171"));
        m_table->setItem(row, 5, statusItem);
        
        // Actions column
        auto* actionsWidget = new QWidget;
        auto* actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 2, 4, 2);
        actionsLayout->setSpacing(4);
        
        auto* editBtn = new QPushButton("✏️");
        editBtn->setObjectName("secondaryBtn");
        editBtn->setMaximumWidth(32);
        editBtn->setToolTip("Edit");
        connect(editBtn, &QPushButton::clicked, [this, row]() {
            int supplierId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
            Supplier supplier;
            for (const auto& s : m_suppliers) {
                if (s.id() == supplierId) { supplier = s; break; }
            }
            if (supplier.id() > 0) {
                Supplier updated = showSupplierDialog(supplier, this);
                if (updated.id() > 0 && m_repo->save(updated)) {
                    Logger::instance().info("Supplier updated: " + updated.name());
                    loadSuppliers();
                }
            }
        });
        
        auto* deleteBtn = new QPushButton;
        deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                               QColor(ThemeManager::instance().tokens().textPrimary), 16));
        deleteBtn->setObjectName("dangerBtn");
        deleteBtn->setMaximumWidth(32);
        deleteBtn->setToolTip("Delete");
        connect(deleteBtn, &QPushButton::clicked, [this, row]() {
            int supplierId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
            Supplier supplier;
            for (const auto& s : m_suppliers) {
                if (s.id() == supplierId) { supplier = s; break; }
            }
            if (supplier.id() > 0) {
                auto reply = QMessageBox::question(this, "Confirm Delete",
                    QString("Delete supplier '%1'?").arg(supplier.name()),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes && m_repo->remove(supplier.id())) {
                    AuditService::instance().logDelete("Supplier", supplier.id(), supplier.name());
                    loadSuppliers();
                }
            }
        });
        
        actionsLayout->addWidget(editBtn);
        actionsLayout->addWidget(deleteBtn);
        actionsLayout->addStretch();
        m_table->setCellWidget(row, 6, actionsWidget);
        
        displayCount++;
    }
    
    m_statusLbl->setText(QString("%1 supplier(s)").arg(displayCount));
    m_table->setSortingEnabled(true);
}

static Supplier showSupplierDialog(const Supplier& existing, QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(existing.id() > 0 ? LangManager::instance().t("Edit Supplier") : LangManager::instance().t("Add Supplier"));
    dlg.setMinimumWidth(500);
    dlg.setModal(true);
    
    auto* form = new QFormLayout(&dlg);
    form->setSpacing(10);
    form->setContentsMargins(16, 16, 16, 16);
    
    auto* nameEdit = new QLineEdit(existing.name());
    nameEdit->setPlaceholderText("Required");
    
    auto* contactEdit = new QLineEdit(existing.contactName());
    contactEdit->setPlaceholderText("Optional");
    
    auto* phoneEdit = new QLineEdit(existing.phone());
    phoneEdit->setPlaceholderText("Optional");
    
    auto* emailEdit = new QLineEdit(existing.email());
    emailEdit->setPlaceholderText("Optional");
    
    auto* addressEdit = new QTextEdit;
    addressEdit->setPlainText(existing.address());
    addressEdit->setMaximumHeight(80);
    addressEdit->setPlaceholderText("Optional");
    
    auto* notesEdit = new QTextEdit;
    notesEdit->setPlainText(existing.notes());
    notesEdit->setMaximumHeight(80);
    notesEdit->setPlaceholderText("Optional");
    
    auto* activeChk = new QCheckBox("Active");
    activeChk->setChecked(existing.active());
    
    form->addRow("Supplier Name *", nameEdit);
    form->addRow("Contact Person", contactEdit);
    form->addRow("Phone", phoneEdit);
    form->addRow("Email", emailEdit);
    form->addRow("Address", addressEdit);
    form->addRow("Notes", notesEdit);
    form->addRow("", activeChk);
    
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    form->addRow(btns);
    
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Supplier name is required.");
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    
    if (dlg.exec() != QDialog::Accepted) return {};
    
    Supplier s;
    s.setId(existing.id());
    s.setName(nameEdit->text().trimmed());
    s.setContactName(contactEdit->text().trimmed());
    s.setPhone(phoneEdit->text().trimmed());
    s.setEmail(emailEdit->text().trimmed());
    s.setAddress(addressEdit->toPlainText().trimmed());
    s.setNotes(notesEdit->toPlainText().trimmed());
    s.setActive(activeChk->isChecked());
    
    return s;
}

void SuppliersPage::onAdd() {
    Supplier s = showSupplierDialog({}, this);
    if (s.name().isEmpty()) return;
    
    if (!m_repo->save(s)) {
        QMessageBox::critical(this, "Error", "Failed to save supplier.");
        return;
    }
    
    AuditService::instance().logCreate("Supplier", s.id(),
        QString("{\"name\":\"%1\"}").arg(s.name()));
    Logger::instance().info("Supplier created: " + s.name());
    loadSuppliers();
}

void SuppliersPage::onSearch() {
    loadSuppliers();
}
