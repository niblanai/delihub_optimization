#include "users_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "services/archive_manager.h"
#include "services/audit_service.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "ui/tr_helper.h"
#include "services/auth_service.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "core/attendance.h"
#include "services/barcode_generator.h"
#include "ui/id_card_renderer.h"
#include "services/barcode_auth.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <algorithm>
#include <QInputDialog>
#include <QFrame>
#include <QTimer>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QDateEdit>
#include <QPainter>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Small inline dialogs
// ─────────────────────────────────────────────────────────────────────────────

// User add/edit dialog
class UserDialog : public QDialog {
public:
    explicit UserDialog(const QList<Role>& roles, QWidget* parent = nullptr)
        : QDialog(parent), m_roles(roles)
    {
        setWindowTitle("User");
        setMinimumWidth(360);
        setModal(true);

        auto* form = new QFormLayout(this);
        form->setSpacing(10);
        form->setContentsMargins(16, 16, 16, 16);

        m_nameEdit     = new QLineEdit;
        m_usernameEdit = new QLineEdit;
        m_passwordEdit = new QLineEdit;
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("Leave blank to keep current");
        m_phoneEdit    = new QLineEdit;
        m_emailEdit    = new QLineEdit;  // NEW: Email field
        m_emailEdit->setPlaceholderText("user@example.com");
        m_roleCombo    = new QComboBox;
        for (const auto& r : roles) m_roleCombo->addItem(r.name, r.id);
        
        // Attendance fields
        m_photoPathEdit = new QLineEdit;
        m_photoPathEdit->setPlaceholderText("Path to user photo (optional)");
        m_photoPathEdit->setReadOnly(true);  // Make read-only, use button to browse
        auto* browsePhotoBtn = new QPushButton("Browse...");
        connect(browsePhotoBtn, &QPushButton::clicked, this, [this]() {
            QString fileName = QFileDialog::getOpenFileName(this, 
                "Select User Photo", 
                "", 
                "Images (*.png *.jpg *.jpeg *.bmp)");
            if (!fileName.isEmpty()) {
                m_photoPathEdit->setText(fileName);
            }
        });
        
        m_fingerprintEdit = new QLineEdit;
        m_fingerprintEdit->setReadOnly(true);
        m_fingerprintEdit->setPlaceholderText("Auto-generate...");
        auto* generateBarcodeBtn = new QPushButton("Generate Fingerprint");
        connect(generateBarcodeBtn, &QPushButton::clicked, this, [this]() {
            const QString existing = m_fingerprintEdit->text().trimmed();

            // Regenerating invalidates every card already printed with the old
            // code, so make the operator confirm before overwriting it.
            if (!existing.isEmpty()) {
                QMessageBox box(this);
                box.setIcon(QMessageBox::Warning);
                box.setWindowTitle("Regenerate Barcode");
                box.setText("This user already has a fingerprint barcode.");
                box.setInformativeText(
                    "Generating a new one will permanently replace:\n\n    " + existing +
                    "\n\nAny card already printed with the old barcode will stop working, "
                    "and you will have to print and hand out a new card for this user.\n\n"
                    "Generate a new barcode anyway?");
                box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                box.setDefaultButton(QMessageBox::No);
                box.button(QMessageBox::Yes)->setText("Yes, regenerate");
                box.button(QMessageBox::No)->setText("Keep current");
                if (box.exec() != QMessageBox::Yes) return;

                m_barcodeRegenerated = true;
            }

            QString generated;
            do {
                generated = QString("FP-%1-%2")
                    .arg(QDateTime::currentSecsSinceEpoch())
                    .arg(QRandomGenerator::global()->bounded(1000, 9999));
            } while (generated == existing);   // guarantee a different value

            m_fingerprintEdit->setText(generated);
        });
        m_hourlyRateSpin = new QDoubleSpinBox;
        m_hourlyRateSpin->setRange(0, 999999);
        m_hourlyRateSpin->setDecimals(2);
        m_hourlyRateSpin->setSuffix(" " + ConfigManager::instance().currencySymbol());
        m_weeklyOffDaysSpin = new QSpinBox;
        m_weeklyOffDaysSpin->setRange(0, 7);
        m_weeklyOffDaysSpin->setValue(1);

        form->addRow("Name *",     m_nameEdit);
        form->addRow("Username *", m_usernameEdit);
        form->addRow("Password",   m_passwordEdit);
        form->addRow("Phone",      m_phoneEdit);
        form->addRow("Email",      m_emailEdit);  // NEW: Email field in form
        form->addRow("Role",       m_roleCombo);
        
        auto* photoLayout = new QHBoxLayout;
        photoLayout->addWidget(m_photoPathEdit, 1);
        photoLayout->addWidget(browsePhotoBtn);
        form->addRow("Photo Path", photoLayout);
        
        auto* barcodeLayout = new QHBoxLayout;
        barcodeLayout->addWidget(m_fingerprintEdit, 1);
        barcodeLayout->addWidget(generateBarcodeBtn);
        form->addRow("Fingerprint Barcode", barcodeLayout);
        
        form->addRow("Hourly Rate", m_hourlyRateSpin);
        form->addRow("Weekly Off Days", m_weeklyOffDaysSpin);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
        form->addRow(btns);

        connect(btns, &QDialogButtonBox::accepted, this, [this]{
            if (m_nameEdit->text().trimmed().isEmpty() || m_usernameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Validation", "Name and Username are required.");
                return;
            }
            accept();
        });
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    // True when the operator replaced an already-existing barcode in this
    // dialog session — the caller uses it to remind them to reprint the card.
    bool barcodeRegenerated() const { return m_barcodeRegenerated; }

    void setUser(const User& u) {
        m_barcodeRegenerated = false;
        m_userId = u.id();
        m_nameEdit->setText(u.name());
        m_usernameEdit->setText(u.username());
        m_phoneEdit->setText(u.phone());
        m_emailEdit->setText(u.email());  // NEW: Set email
        m_photoPathEdit->setText(u.photoPath());
        m_fingerprintEdit->setText(u.fingerprintBarcode());
        m_hourlyRateSpin->setValue(u.hourlyRate());
        m_weeklyOffDaysSpin->setValue(u.weeklyOffDays());
        int idx = m_roleCombo->findData(u.roleId());
        if (idx >= 0) m_roleCombo->setCurrentIndex(idx);
    }

    User getUser() const {
        User u;
        u.setId(m_userId);
        u.setName(m_nameEdit->text().trimmed());
        u.setUsername(m_usernameEdit->text().trimmed());
        u.setPhone(m_phoneEdit->text().trimmed());
        u.setEmail(m_emailEdit->text().trimmed());  // NEW: Get email
        u.setRoleId(m_roleCombo->currentData().toInt());
        u.setPhotoPath(m_photoPathEdit->text().trimmed());
        u.setFingerprintBarcode(m_fingerprintEdit->text().trimmed());
        u.setHourlyRate(m_hourlyRateSpin->value());
        u.setWeeklyOffDays(m_weeklyOffDaysSpin->value());
        return u;
    }

    QString plainPassword() const { return m_passwordEdit->text(); }

private:
    int         m_userId = 0;
    QList<Role> m_roles;
    QLineEdit*  m_nameEdit     = nullptr;
    QLineEdit*  m_usernameEdit = nullptr;
    QLineEdit*  m_passwordEdit = nullptr;
    QLineEdit*  m_phoneEdit    = nullptr;
    QLineEdit*  m_emailEdit    = nullptr;  // NEW: Email field
    QComboBox*  m_roleCombo    = nullptr;
    QLineEdit*  m_photoPathEdit = nullptr;
    QLineEdit*  m_fingerprintEdit = nullptr;
    bool        m_barcodeRegenerated = false;
    QDoubleSpinBox* m_hourlyRateSpin = nullptr;
    QSpinBox*   m_weeklyOffDaysSpin = nullptr;
};

// Role add/edit dialog — granular permissions
class RoleDialog : public QDialog {
public:
    explicit RoleDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Role");
        setMinimumWidth(400);
        setModal(true);

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(8);
        layout->setContentsMargins(16, 16, 16, 16);

        auto* form = new QFormLayout;
        m_nameEdit = new QLineEdit;
        form->addRow("Role Name *", m_nameEdit);
        layout->addLayout(form);

        // ── Permission groups ──────────────────────────────────────────────
        auto addGroup = [&](const QString& title,
                            std::initializer_list<std::pair<QString,QCheckBox**>> items) {
            auto* grp = new QGroupBox(title);
            auto* gl  = new QHBoxLayout(grp);
            gl->setSpacing(8);
            for (auto& [label, ptr] : items) {
                *ptr = new QCheckBox(label);
                gl->addWidget(*ptr);
            }
            gl->addStretch();
            layout->addWidget(grp);
        };

        addGroup("Customers",
            {{"Add",   &m_chkAddCust},  {"Edit",   &m_chkEditCust},  {"Delete", &m_chkDelCust}});
        addGroup("Products",
            {{"Add",   &m_chkAddProd},  {"Edit",   &m_chkEditProd},  {"Delete", &m_chkDelProd}});
        addGroup("Orders",
            {{"Add",        &m_chkAddOrd},   {"Edit",       &m_chkEditOrd},
             {"Edit Date",  &m_chkEditOrdDate},
             {"Cancel",     &m_chkCancelOrd},{"Delete",     &m_chkDelOrd}});
        addGroup("Access",
            {{"Dashboard",    &m_chkDashboard},{"View Reports", &m_chkReports},
             {"Manage Regions",&m_chkRegions},
             {"Settings",     &m_chkSettings},{"Manage Users",  &m_chkUsers}});
        // Phase 1 permissions
        addGroup("Phase 1",
            {{"Purchases",  &m_chkPurchases}, {"POS",        &m_chkPOS},
             {"Del POS Item", &m_chkDelFromPOS},
             {"Register",   &m_chkRegister},  {"Expenses",   &m_chkExpenses},
             {"Inventory",  &m_chkInventory}});

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
        layout->addWidget(btns);

        connect(btns, &QDialogButtonBox::accepted, this, [this]{
            if (m_nameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Validation", "Role name is required."); return;
            }
            accept();
        });
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void setRole(const Role& r) {
        m_roleId = r.id;
        m_nameEdit->setText(r.name);
        m_chkAddCust->setChecked(r.canAddCustomer);
        m_chkEditCust->setChecked(r.canEditCustomer);
        m_chkDelCust->setChecked(r.canDeleteCustomer);
        m_chkAddProd->setChecked(r.canAddProduct);
        m_chkEditProd->setChecked(r.canEditProduct);
        m_chkDelProd->setChecked(r.canDeleteProduct);
        m_chkAddOrd->setChecked(r.canAddOrder);
        m_chkEditOrd->setChecked(r.canEditOrder);
        m_chkEditOrdDate->setChecked(r.canEditOrderDate);
        m_chkCancelOrd->setChecked(r.canCancelOrder);
        m_chkDelOrd->setChecked(r.canDeleteOrders);
        m_chkReports->setChecked(r.canViewReports);
        m_chkRegions->setChecked(r.canManageRegions);
        m_chkDashboard->setChecked(r.canAccessDashboard);
        m_chkSettings->setChecked(r.canAccessSettings);
        m_chkUsers->setChecked(r.canManageUsers);
        // Phase 1
        m_chkPurchases->setChecked(r.canManagePurchases);
        m_chkPOS->setChecked(r.canAccessPOS);
        m_chkDelFromPOS->setChecked(r.canDeleteFromPOS);
        m_chkRegister->setChecked(r.canManageRegister);
        m_chkExpenses->setChecked(r.canManageExpenses);
        m_chkInventory->setChecked(r.canManageInventory);
    }

    Role getRole() const {
        Role r;
        r.id                = m_roleId;
        r.name              = m_nameEdit->text().trimmed();
        r.canAddCustomer    = m_chkAddCust->isChecked();
        r.canEditCustomer   = m_chkEditCust->isChecked();
        r.canDeleteCustomer = m_chkDelCust->isChecked();
        r.canAddProduct     = m_chkAddProd->isChecked();
        r.canEditProduct    = m_chkEditProd->isChecked();
        r.canDeleteProduct  = m_chkDelProd->isChecked();
        r.canAddOrder       = m_chkAddOrd->isChecked();
        r.canEditOrder      = m_chkEditOrd->isChecked();
        r.canEditOrderDate  = m_chkEditOrdDate->isChecked();
        r.canCancelOrder    = m_chkCancelOrd->isChecked();
        r.canDeleteOrders   = m_chkDelOrd->isChecked();
        r.canViewReports    = m_chkReports->isChecked();
        r.canManageRegions  = m_chkRegions->isChecked();
        r.canAccessDashboard = m_chkDashboard->isChecked();
        r.canAccessSettings = m_chkSettings->isChecked();
        r.canManageUsers    = m_chkUsers->isChecked();
        // Phase 1
        r.canManagePurchases = m_chkPurchases->isChecked();
        r.canAccessPOS       = m_chkPOS->isChecked();
        r.canDeleteFromPOS   = m_chkDelFromPOS->isChecked();
        r.canManageRegister  = m_chkRegister->isChecked();
        r.canManageExpenses  = m_chkExpenses->isChecked();
        r.canManageInventory = m_chkInventory->isChecked();
        return r;
    }

private:
    int        m_roleId = 0;
    QLineEdit* m_nameEdit      = nullptr;
    QCheckBox* m_chkAddCust    = nullptr;
    QCheckBox* m_chkEditCust   = nullptr;
    QCheckBox* m_chkDelCust    = nullptr;
    QCheckBox* m_chkAddProd    = nullptr;
    QCheckBox* m_chkEditProd   = nullptr;
    QCheckBox* m_chkDelProd    = nullptr;
    QCheckBox* m_chkAddOrd      = nullptr;
    QCheckBox* m_chkEditOrd     = nullptr;
    QCheckBox* m_chkEditOrdDate = nullptr;
    QCheckBox* m_chkCancelOrd   = nullptr;
    QCheckBox* m_chkDelOrd     = nullptr;
    QCheckBox* m_chkReports    = nullptr;
    QCheckBox* m_chkRegions    = nullptr;
    QCheckBox* m_chkDashboard  = nullptr;
    QCheckBox* m_chkSettings   = nullptr;
    QCheckBox* m_chkUsers      = nullptr;
    // Phase 1 checkboxes
    QCheckBox* m_chkPurchases  = nullptr;
    QCheckBox* m_chkPOS        = nullptr;
    QCheckBox* m_chkDelFromPOS = nullptr;
    QCheckBox* m_chkRegister   = nullptr;
    QCheckBox* m_chkExpenses   = nullptr;
    QCheckBox* m_chkInventory  = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// UsersPage
// ─────────────────────────────────────────────────────────────────────────────
UsersPage::UsersPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_userRepo   = new SQLiteUserRepository;
        m_roleRepo   = new SQLiteRoleRepository;
        m_driverRepo = new SQLiteDeliveryDriverRepository;
    } else {
        m_userRepo   = new AccessUserRepository;
        m_roleRepo   = new AccessRoleRepository;
        m_driverRepo = new AccessDeliveryDriverRepository;
    }

    setupUi();
    // Data loaded by MainWindow::navigateTo — no singleShot needed
}

void UsersPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto* tabs = new QTabWidget;

    // ══════════════════════════════════════════════════════════════════
    // TAB 1 — USERS
    // ══════════════════════════════════════════════════════════════════
    auto* usersWidget = new QWidget;
    auto* usersLayout = new QVBoxLayout(usersWidget);
    usersLayout->setSpacing(0);
    usersLayout->setContentsMargins(0, 0, 0, 0);

    auto* utb = new QFrame;
    utb->setObjectName("pageToolbar");
    auto* utbL = new QHBoxLayout(utb);
    utbL->setContentsMargins(16, 10, 16, 10);
    auto* uTitle = new QLabel("Users");
    uTitle->setObjectName("pageTitle");
    uTitle->setVisible(false);
    m_addUserBtn    = new QPushButton(LangManager::instance().t("＋ Add User"));
    m_addUserBtn->setObjectName("primaryBtn");
    
    m_editUserBtn   = new QPushButton(LangManager::instance().t("Edit"));
    m_editUserBtn->setObjectName("secondaryBtn");
    m_editUserBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editUserBtn->setIconSize(QSize(18, 18));
    m_editUserBtn->setEnabled(false);
    
    m_deleteUserBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteUserBtn->setObjectName("dangerBtn");
    m_deleteUserBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                   QColor("#FFFFFF"), 18));
    m_deleteUserBtn->setIconSize(QSize(18, 18));
    m_deleteUserBtn->setEnabled(false);
    utbL->addWidget(uTitle); utbL->addStretch();
    utbL->addWidget(m_addUserBtn); utbL->addWidget(m_editUserBtn); utbL->addWidget(m_deleteUserBtn);
    usersLayout->addWidget(utb);

    m_usersTable = new QTableWidget(0, 5);
    m_usersTable->setObjectName("dataTable");
    m_usersTable->setHorizontalHeaderLabels({
        "#",
        LangManager::instance().t("Name"),
        LangManager::instance().t("Username"),
        LangManager::instance().t("Role"),
        LangManager::instance().t("Phone")
    });
    m_usersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_usersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_usersTable->verticalHeader()->setVisible(false);
    m_usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_usersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_usersTable->setAlternatingRowColors(true);
    m_usersTable->setSortingEnabled(true);
    usersLayout->addWidget(m_usersTable, 1);
    tabs->addTab(usersWidget, "");
    tabs->setTabIcon(0, SvgIconHelper::icon(sidebarSvgPath("user-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(0, "Users");

    // ══════════════════════════════════════════════════════════════════
    // TAB 2 — ROLES
    // ══════════════════════════════════════════════════════════════════
    auto* rolesWidget = new QWidget;
    auto* rolesLayout = new QVBoxLayout(rolesWidget);
    rolesLayout->setSpacing(0);
    rolesLayout->setContentsMargins(0, 0, 0, 0);

    auto* rtb = new QFrame;
    rtb->setObjectName("pageToolbar");
    auto* rtbL = new QHBoxLayout(rtb);
    rtbL->setContentsMargins(16, 10, 16, 10);
    auto* rTitle = new QLabel("Roles & Permissions");
    rTitle->setObjectName("pageTitle");
    m_addRoleBtn    = new QPushButton(LangManager::instance().t("＋ Add Role"));
    m_addRoleBtn->setObjectName("primaryBtn");
    
    m_editRoleBtn   = new QPushButton(LangManager::instance().t("Edit"));
    m_editRoleBtn->setObjectName("secondaryBtn");
    m_editRoleBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                 QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editRoleBtn->setIconSize(QSize(18, 18));
    m_editRoleBtn->setEnabled(false);
    
    m_deleteRoleBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteRoleBtn->setObjectName("dangerBtn");
    m_deleteRoleBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                   QColor("#FFFFFF"), 18));
    m_deleteRoleBtn->setIconSize(QSize(18, 18));
    m_deleteRoleBtn->setEnabled(false);
    rtbL->addWidget(rTitle); rtbL->addStretch();
    rtbL->addWidget(m_addRoleBtn); rtbL->addWidget(m_editRoleBtn); rtbL->addWidget(m_deleteRoleBtn);
    rolesLayout->addWidget(rtb);

    m_rolesTable = new QTableWidget(0, 5);
    m_rolesTable->setObjectName("dataTable");
    m_rolesTable->setHorizontalHeaderLabels({
        "#",
        LangManager::instance().t("Role Name"),
        LangManager::instance().t("Customers"),
        LangManager::instance().t("Products"),
        LangManager::instance().t("Orders")
    });
    m_rolesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_rolesTable->verticalHeader()->setVisible(false);
    m_rolesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rolesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rolesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rolesTable->setAlternatingRowColors(true);
    m_rolesTable->setSortingEnabled(true);
    rolesLayout->addWidget(m_rolesTable, 1);
    tabs->addTab(rolesWidget, "");
    tabs->setTabIcon(1, SvgIconHelper::icon(sidebarSvgPath("portal-roles-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(1, "Roles");

    // ══════════════════════════════════════════════════════════════════
    // TAB 3 — DELIVERY DRIVERS
    // ══════════════════════════════════════════════════════════════════
    auto* driverWidget = new QWidget;
    auto* driverLayout = new QVBoxLayout(driverWidget);
    driverLayout->setSpacing(0);
    driverLayout->setContentsMargins(0,0,0,0);

    auto* dtb = new QFrame;
    dtb->setObjectName("pageToolbar");
    auto* dtbL = new QHBoxLayout(dtb);
    dtbL->setContentsMargins(16,10,16,10);
    auto* dTitle = new QLabel("Delivery Drivers");
    dTitle->setObjectName("pageTitle");
    m_addDriverBtn    = new QPushButton(LangManager::instance().t("＋ Add Driver"));
    m_addDriverBtn->setObjectName("primaryBtn");
    
    m_editDriverBtn   = new QPushButton(LangManager::instance().t("Edit"));
    m_editDriverBtn->setObjectName("secondaryBtn");
    m_editDriverBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editDriverBtn->setIconSize(QSize(18, 18));
    m_editDriverBtn->setEnabled(false);
    
    m_deleteDriverBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteDriverBtn->setObjectName("dangerBtn");
    m_deleteDriverBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                     QColor("#FFFFFF"), 18));
    m_deleteDriverBtn->setIconSize(QSize(18, 18));
    m_deleteDriverBtn->setEnabled(false);
    dtbL->addWidget(dTitle); dtbL->addStretch();
    dtbL->addWidget(m_addDriverBtn); dtbL->addWidget(m_editDriverBtn); dtbL->addWidget(m_deleteDriverBtn);
    driverLayout->addWidget(dtb);

    m_driversTable = new QTableWidget(0, 4);
    m_driversTable->setObjectName("dataTable");
    m_driversTable->setHorizontalHeaderLabels({
        "#",
        LangManager::instance().t("Name"),
        LangManager::instance().t("Phone"),
        LangManager::instance().t("National ID")
    });
    m_driversTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_driversTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_driversTable->verticalHeader()->setVisible(false);
    m_driversTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_driversTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_driversTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_driversTable->setAlternatingRowColors(true);
    m_driversTable->setSortingEnabled(true);
    driverLayout->addWidget(m_driversTable, 1);

    tabs->addTab(driverWidget, "");
    tabs->setTabIcon(2, SvgIconHelper::icon(sidebarSvgPath("delivery-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(2, "Delivery Drivers");

    // ══════════════════════════════════════════════════════════════════
    // TAB 3 — ATTENDANCE RECORDS
    // ══════════════════════════════════════════════════════════════════
    auto* attendanceWidget = new QWidget;
    auto* attendanceLayout = new QVBoxLayout(attendanceWidget);
    attendanceLayout->setSpacing(0);
    attendanceLayout->setContentsMargins(0, 0, 0, 0);

    auto* atb = new QFrame;
    atb->setObjectName("pageToolbar");
    auto* atbL = new QHBoxLayout(atb);
    atbL->setContentsMargins(16, 10, 16, 10);
    auto* aTitle = new QLabel("Attendance Records");
    aTitle->setObjectName("pageTitle");
    aTitle->setVisible(false);
    
    // Filter controls
    m_attendanceUserCombo = new QComboBox;
    m_attendanceUserCombo->addItem("All Users", 0);
    m_attendanceUserCombo->setMinimumWidth(180);
    
    m_attendanceDateFromEdit = new QDateEdit(QDate::currentDate().addDays(-30));
    m_attendanceDateFromEdit->setCalendarPopup(true);
    m_attendanceDateFromEdit->setDisplayFormat("yyyy-MM-dd");
    
    m_attendanceDateToEdit = new QDateEdit(QDate::currentDate());
    m_attendanceDateToEdit->setCalendarPopup(true);
    m_attendanceDateToEdit->setDisplayFormat("yyyy-MM-dd");
    
    auto* filterBtn = new QPushButton("🔍 Filter");
    filterBtn->setObjectName("secondaryBtn");
    
    auto* calculateSalaryBtn = new QPushButton;
    calculateSalaryBtn->setText("Calculate Salary");
    calculateSalaryBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("calculate-calculator-math-device-technology-svgrepo-com.svg"), 
                                                     QColor(ThemeManager::instance().tokens().textPrimary), 18));
    calculateSalaryBtn->setObjectName("primaryBtn");
    
    atbL->addWidget(aTitle);
    atbL->addWidget(new QLabel("User:"));
    atbL->addWidget(m_attendanceUserCombo);
    atbL->addWidget(new QLabel("From:"));
    atbL->addWidget(m_attendanceDateFromEdit);
    atbL->addWidget(new QLabel("To:"));
    atbL->addWidget(m_attendanceDateToEdit);
    atbL->addWidget(filterBtn);
    atbL->addStretch();
    atbL->addWidget(calculateSalaryBtn);
    attendanceLayout->addWidget(atb);

    m_attendanceTable = new QTableWidget(0, 5);
    m_attendanceTable->setObjectName("dataTable");
    m_attendanceTable->setHorizontalHeaderLabels({
        "#",
        "User",
        "Date",
        "Time",
        "Type"
    });
    m_attendanceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_attendanceTable->verticalHeader()->setVisible(false);
    m_attendanceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_attendanceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_attendanceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_attendanceTable->setAlternatingRowColors(true);
    m_attendanceTable->setSortingEnabled(true);
    attendanceLayout->addWidget(m_attendanceTable, 1);
    
    tabs->addTab(attendanceWidget, "");
    tabs->setTabIcon(3, SvgIconHelper::icon(sidebarSvgPath("attendance-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(3, "Attendance");

    // ══════════════════════════════════════════════════════════════════
    // TAB 4 — FINGERPRINT CARDS
    // ══════════════════════════════════════════════════════════════════
    auto* cardsWidget = new QWidget;
    auto* cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setSpacing(0);
    cardsLayout->setContentsMargins(0, 0, 0, 0);

    auto* ctb = new QFrame;
    ctb->setObjectName("pageToolbar");
    auto* ctbL = new QHBoxLayout(ctb);
    ctbL->setContentsMargins(16, 10, 16, 10);
    auto* cTitle = new QLabel("Fingerprint Cards");
    cTitle->setObjectName("pageTitle");
    cTitle->setVisible(false);
    
    m_selectUserForCardCombo = new QComboBox;
    m_selectUserForCardCombo->setMinimumWidth(200);
    
    auto* printCardBtn = new QPushButton;
    printCardBtn->setText("Print Card");
    printCardBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("print-svgrepo-com.svg"), 
                                              QColor(ThemeManager::instance().tokens().textPrimary), 18));
    printCardBtn->setObjectName("primaryBtn");
    
    ctbL->addWidget(cTitle);
    ctbL->addWidget(new QLabel("Select User:"));
    ctbL->addWidget(m_selectUserForCardCombo);
    ctbL->addWidget(printCardBtn);
    ctbL->addStretch();
    cardsLayout->addWidget(ctb);
    
    // Card preview area
    m_cardPreviewLabel = new QLabel();
    m_cardPreviewLabel->setAlignment(Qt::AlignCenter);
    m_cardPreviewLabel->setStyleSheet("background: #F9FAFB; border: 2px dashed #D1D5DB; padding: 20px;");
    m_cardPreviewLabel->setText("<div style='color:#9CA3AF; font-size:14px;'>Select a user to preview their fingerprint card</div>");
    cardsLayout->addWidget(m_cardPreviewLabel, 1);

    // ── Scan test bar ───────────────────────────────────────────────────────
    // Lets the operator verify a freshly printed card actually reads back as
    // the right user, before handing it out.
    auto* testBar = new QFrame;
    testBar->setObjectName("pageToolbar");
    auto* testL = new QHBoxLayout(testBar);
    testL->setContentsMargins(16, 10, 16, 10);
    testL->addWidget(new QLabel("Test scan:"));

    m_cardTestEdit = new QLineEdit;
    m_cardTestEdit->setPlaceholderText("Scan the printed card here, or type the code under the barcode...");
    m_cardTestEdit->setClearButtonEnabled(true);
    m_cardTestEdit->setMinimumWidth(320);
    testL->addWidget(m_cardTestEdit, 1);

    m_cardTestResultLabel = new QLabel;
    m_cardTestResultLabel->setMinimumWidth(260);
    testL->addWidget(m_cardTestResultLabel);

    cardsLayout->addWidget(testBar);

    connect(m_cardTestEdit, &QLineEdit::returnPressed, this, &UsersPage::onTestCardScan);
    connect(m_cardTestEdit, &QLineEdit::textEdited, this, [this](const QString&) {
        m_cardTestResultLabel->clear();
    });
    
    tabs->addTab(cardsWidget, "");
    tabs->setTabIcon(4, SvgIconHelper::icon(sidebarSvgPath("publish-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    tabs->setTabText(4, "Print Cards");

    root->addWidget(tabs);

    connect(m_addDriverBtn,    &QPushButton::clicked, this, &UsersPage::onAddDriver);
    connect(m_editDriverBtn,   &QPushButton::clicked, this, &UsersPage::onEditDriver);
    connect(m_deleteDriverBtn, &QPushButton::clicked, this, &UsersPage::onDeleteDriver);
    connect(m_driversTable, &QTableWidget::itemSelectionChanged, this, &UsersPage::onDriverSelectionChanged);
    connect(m_driversTable, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onEditDriver(); });

    // Attendance tab connections
    connect(filterBtn, &QPushButton::clicked, this, &UsersPage::onFilterAttendance);
    connect(calculateSalaryBtn, &QPushButton::clicked, this, &UsersPage::onCalculateSalary);
    
    // Fingerprint card connections
    connect(printCardBtn, &QPushButton::clicked, this, &UsersPage::onPrintCard);
    connect(m_selectUserForCardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &UsersPage::onUserCardSelectionChanged);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_addUserBtn,    &QPushButton::clicked, this, &UsersPage::onAddUser);
    connect(m_editUserBtn,   &QPushButton::clicked, this, &UsersPage::onEditUser);
    connect(m_deleteUserBtn, &QPushButton::clicked, this, &UsersPage::onDeleteUser);
    connect(m_usersTable, &QTableWidget::itemSelectionChanged, this, &UsersPage::onUserSelectionChanged);
    connect(m_usersTable, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onEditUser(); });

    connect(m_addRoleBtn,    &QPushButton::clicked, this, &UsersPage::onAddRole);
    connect(m_editRoleBtn,   &QPushButton::clicked, this, &UsersPage::onEditRole);
    connect(m_deleteRoleBtn, &QPushButton::clicked, this, &UsersPage::onDeleteRole);
    connect(m_rolesTable, &QTableWidget::itemSelectionChanged, this, &UsersPage::onRoleSelectionChanged);
    connect(m_rolesTable, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onEditRole(); });
}

// ─────────────────────────────────────────────────────────────────────────────
void UsersPage::refresh() {
    loadRoles();
    loadUsers();
    loadDrivers();
    loadAttendanceRecords();
    populateUserCombos();
}

void UsersPage::loadRoles() {
    m_roles = m_roleRepo->getAll();
    // Sort by id for consistent display
    std::sort(m_roles.begin(), m_roles.end(), [](const Role& a, const Role& b){ return a.id < b.id; });
    m_roleMap.clear();
    for (const auto& r : m_roles) m_roleMap[r.id] = r.name;

    m_rolesTable->setSortingEnabled(false);   // prevent index mismatch during populate
    m_rolesTable->setRowCount(0);
    for (const auto& r : m_roles) {
        int row = m_rolesTable->rowCount();
        m_rolesTable->insertRow(row);
        auto yn = [](bool v){ return v ? "✅" : "—"; };
        auto c = [](const QString& t, Qt::Alignment a = Qt::AlignCenter){
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        auto* idItem = c(QString::number(r.id));
        idItem->setData(Qt::UserRole + 1, r.id);
        m_rolesTable->setItem(row, 0, idItem);
        m_rolesTable->setItem(row, 1, c(r.name, Qt::AlignLeft | Qt::AlignVCenter));
        m_rolesTable->setItem(row, 2, c(yn(r.canAddCustomer || r.canEditCustomer || r.canDeleteCustomer)));
        m_rolesTable->setItem(row, 3, c(yn(r.canAddProduct  || r.canEditProduct  || r.canDeleteProduct)));
        m_rolesTable->setItem(row, 4, c(yn(r.canAddOrder    || r.canEditOrder    || r.canDeleteOrders)));
    }
    m_rolesTable->setSortingEnabled(true);
}

void UsersPage::loadUsers() {
    m_users = m_userRepo->getAll();
    // Sort by id ascending for consistent display order
    std::sort(m_users.begin(), m_users.end(), [](const User& a, const User& b){
        return a.id() < b.id();
    });
    m_usersTable->setSortingEnabled(false);   // disable while populating to prevent row scrambling
    m_usersTable->setRowCount(0);
    for (const auto& u : m_users) {
        int row = m_usersTable->rowCount();
        m_usersTable->insertRow(row);
        auto c = [](const QString& t, Qt::Alignment a = Qt::AlignCenter){
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        auto* idItem = c(QString::number(u.id()));
        idItem->setData(Qt::UserRole + 1, u.id());   // numeric sort key for id column
        m_usersTable->setItem(row, 0, idItem);
        m_usersTable->setItem(row, 1, c(u.name(), Qt::AlignLeft | Qt::AlignVCenter));
        m_usersTable->setItem(row, 2, c(u.username()));
        m_usersTable->setItem(row, 3, c(m_roleMap.value(u.roleId(), "—")));
        m_usersTable->setItem(row, 4, c(u.phone()));
    }
    m_usersTable->setSortingEnabled(true);
    m_editUserBtn->setEnabled(false);
    m_deleteUserBtn->setEnabled(false);
}

// ─────────────────────────────────────────────────────────────────────────────
void UsersPage::onAddUser() {
    UserDialog dlg(m_roles, this);
    if (dlg.exec() != QDialog::Accepted) return;

    User u = dlg.getUser();
    QString pwd = dlg.plainPassword();
    if (!pwd.isEmpty()) {
        AuthService auth(m_userRepo);
        auth.setPassword(u, pwd);
    }
    if (!m_userRepo->save(u)) {
        QMessageBox::critical(this, "Error", "Failed to save user.");
        return;
    }
    Logger::instance().info("User created: " + u.username());
    loadUsers();
}

void UsersPage::onEditUser() {
    int row = m_usersTable->currentRow();
    if (row < 0) return;

    int uid = m_usersTable->item(row, 0)->text().toInt();
    User u = m_userRepo->getById(uid);

    UserDialog dlg(m_roles, this);
    dlg.setUser(u);
    if (dlg.exec() != QDialog::Accepted) return;

    User updated = dlg.getUser();
    QString pwd = dlg.plainPassword();
    if (!pwd.isEmpty()) {
        AuthService auth(m_userRepo);
        auth.setPassword(updated, pwd);
    } else {
        // Keep existing hash/salt
        updated.setPasswordHash(u.passwordHash());
        updated.setPasswordSalt(u.passwordSalt());
    }
    if (!m_userRepo->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update user.");
        return;
    }
    Logger::instance().info("User updated: " + updated.username());
    loadUsers();

    if (dlg.barcodeRegenerated()) {
        QMessageBox::information(this, "Reprint Required",
            "The fingerprint barcode for \"" + updated.name() + "\" has been changed.\n\n"
            "The old card is no longer valid. Go to the \"Print Cards\" tab and print "
            "a new card for this user.");
    }
}

void UsersPage::onDeleteUser() {
    int row = m_usersTable->currentRow();
    if (row < 0) return;
    auto* nameItem = m_usersTable->item(row, 1);
    auto* idItem   = m_usersTable->item(row, 0);
    if (!nameItem || !idItem) return;
    QString name = nameItem->text();
    int uid = idItem->text().toInt();
    if (uid <= 0) return;

    auto reply = QMessageBox::question(this, "Confirm",
        "Delete user \"" + name + "\"?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    m_usersTable->clearSelection();

    // Task 11: soft-delete
    QString archErr;
    int archiveId = ArchiveManager::instance().softDelete("Users", "Id", uid, &archErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive user before deletion:\n" + archErr);
        return;
    }
    AuditService::instance().logDelete("User", uid,
        QString("{\"archiveId\":%1,\"username\":\"%2\"}").arg(archiveId).arg(name));
    Logger::instance().info("User deleted ID=" + QString::number(uid));
    loadUsers();
}

void UsersPage::onUserSelectionChanged() {
    bool has = m_usersTable->currentRow() >= 0;
    m_editUserBtn->setEnabled(has);
    m_deleteUserBtn->setEnabled(has);
}

void UsersPage::onAddRole() {
    RoleDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    Role r = dlg.getRole();
    if (!m_roleRepo->save(r)) { QMessageBox::critical(this, "Error", "Failed to save role."); return; }
    refresh();
}

void UsersPage::onEditRole() {
    int row = m_rolesTable->currentRow();
    if (row < 0) return;
    // Read role id from the table cell (handles sorted order correctly)
    auto* idItem = m_rolesTable->item(row, 0);
    if (!idItem) return;
    int rid = idItem->text().toInt();
    // Find matching role in m_roles by id
    int idx = -1;
    for (int i = 0; i < m_roles.size(); ++i) {
        if (m_roles.at(i).id == rid) { idx = i; break; }
    }
    if (idx < 0) return;
    RoleDialog dlg(this);
    dlg.setRole(m_roles.at(idx));
    if (dlg.exec() != QDialog::Accepted) return;
    Role r = dlg.getRole();
    if (!m_roleRepo->save(r)) { QMessageBox::critical(this, "Error", "Failed to update role."); return; }
    refresh();
}

void UsersPage::onDeleteRole() {
    int row = m_rolesTable->currentRow();
    if (row < 0) return;
    auto* idItem   = m_rolesTable->item(row, 0);
    auto* nameItem = m_rolesTable->item(row, 1);
    if (!idItem || !nameItem) return;
    int rid = idItem->text().toInt();
    QString rname = nameItem->text();
    auto reply = QMessageBox::question(this, "Confirm",
        "Delete role \"" + rname + "\"?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    if (!m_roleRepo->remove(rid)) {
        QMessageBox::critical(this, "Error", "Failed to delete role."); return;
    }
    refresh();
}

void UsersPage::onRoleSelectionChanged() {
    bool has = m_rolesTable->currentRow() >= 0;
    m_editRoleBtn->setEnabled(has);
    m_deleteRoleBtn->setEnabled(has);
}

// ── Drivers ──────────────────────────────────────────────────────────────────
void UsersPage::loadDrivers() {
    m_drivers = m_driverRepo->getAll();
    m_driversTable->setSortingEnabled(false);   // prevent row scrambling during populate
    m_driversTable->setRowCount(0);
    for (const auto& d : m_drivers) {
        int row = m_driversTable->rowCount();
        m_driversTable->insertRow(row);
        auto c = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        auto* idItem = c(QString::number(d.id()));
        idItem->setData(Qt::UserRole + 1, d.id());   // numeric sort key
        m_driversTable->setItem(row, 0, idItem);
        m_driversTable->setItem(row, 1, c(d.name(),       Qt::AlignLeft | Qt::AlignVCenter));
        m_driversTable->setItem(row, 2, c(d.phone(),      Qt::AlignLeft | Qt::AlignVCenter));
        m_driversTable->setItem(row, 3, c(d.nationalId(), Qt::AlignLeft | Qt::AlignVCenter));
    }
    m_driversTable->setSortingEnabled(true);
    m_editDriverBtn->setEnabled(false);
    m_deleteDriverBtn->setEnabled(false);
}

static DeliveryDriver showDriverDialog(const DeliveryDriver& existing, QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(existing.id() > 0 ? "Edit Driver" : "Add Driver");
    dlg.setMinimumWidth(360);
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);
    form->setSpacing(10);
    form->setContentsMargins(16,16,16,16);

    auto* nameEdit = new QLineEdit(existing.name());
    auto* phoneEdit = new QLineEdit(existing.phone());
    auto* idEdit    = new QLineEdit(existing.nationalId());
    auto* activeChk = new QCheckBox("Active");
    activeChk->setChecked(existing.active());

    form->addRow("Name *",       nameEdit);
    form->addRow("Phone",        phoneEdit);
    form->addRow("National ID",  idEdit);
    form->addRow("",             activeChk);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    form->addRow(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&](){
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Name is required.");
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return {};

    DeliveryDriver d;
    d.setId(existing.id());
    d.setName(nameEdit->text().trimmed());
    d.setPhone(phoneEdit->text().trimmed());
    d.setNationalId(idEdit->text().trimmed());
    d.setActive(activeChk->isChecked());
    return d;
}

void UsersPage::onAddDriver() {
    DeliveryDriver d = showDriverDialog({}, this);
    if (d.name().isEmpty()) return;
    if (!m_driverRepo->save(d)) {
        QMessageBox::critical(this, "Error", "Failed to save driver.");
        return;
    }
    loadDrivers();
}

void UsersPage::onEditDriver() {
    int row = m_driversTable->currentRow();
    if (row < 0 || row >= m_drivers.size()) return;
    DeliveryDriver d = showDriverDialog(m_drivers.at(row), this);
    if (d.name().isEmpty()) return;
    if (!m_driverRepo->save(d)) {
        QMessageBox::critical(this, "Error", "Failed to update driver.");
        return;
    }
    loadDrivers();
}

void UsersPage::onDeleteDriver() {
    int row = m_driversTable->currentRow();
    if (row < 0 || row >= m_drivers.size()) return;
    auto reply = QMessageBox::question(this, "Confirm",
        "Delete driver \"" + m_drivers.at(row).name() + "\"?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    m_driverRepo->remove(m_drivers.at(row).id());
    loadDrivers();
}

void UsersPage::onDriverSelectionChanged() {
    bool has = m_driversTable->currentRow() >= 0;
    m_editDriverBtn->setEnabled(has);
    m_deleteDriverBtn->setEnabled(has);
}


void UsersPage::retranslateUi() { retranslateWidget(this); }


// ── Attendance ───────────────────────────────────────────────────────────────
void UsersPage::loadAttendanceRecords() {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    IAttendanceRepository* attendanceRepo = useSqlite
        ? static_cast<IAttendanceRepository*>(new SQLiteAttendanceRepository)
        : nullptr;
    
    if (!attendanceRepo) {
        m_attendanceTable->setRowCount(0);
        return;
    }
    
    // Get filtered records
    int userId = m_attendanceUserCombo->currentData().toInt();
    QDate fromDate = m_attendanceDateFromEdit->date();
    QDate toDate = m_attendanceDateToEdit->date();
    
    QList<Attendance> records;
    if (userId == 0) {
        records = attendanceRepo->getAll();
    } else {
        records = attendanceRepo->getByUserId(userId);
    }
    
    // Filter by date range
    QList<Attendance> filteredRecords;
    for (const auto& att : records) {
        QDate attDate = att.timestamp().date();
        if (attDate >= fromDate && attDate <= toDate) {
            filteredRecords.append(att);
        }
    }
    
    // Sort by timestamp descending
    std::sort(filteredRecords.begin(), filteredRecords.end(), 
              [](const Attendance& a, const Attendance& b) {
                  return a.timestamp() > b.timestamp();
              });
    
    m_attendanceTable->setSortingEnabled(false);
    m_attendanceTable->setRowCount(0);
    
    for (int i = 0; i < filteredRecords.size(); ++i) {
        const auto& att = filteredRecords[i];
        m_attendanceTable->insertRow(i);
        
        auto* idItem = new QTableWidgetItem(QString::number(att.id()));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_attendanceTable->setItem(i, 0, idItem);
        
        m_attendanceTable->setItem(i, 1, new QTableWidgetItem(att.userName()));
        m_attendanceTable->setItem(i, 2, new QTableWidgetItem(att.timestamp().toString("yyyy-MM-dd")));
        m_attendanceTable->setItem(i, 3, new QTableWidgetItem(att.timestamp().toString("hh:mm:ss")));
        
        QString typeText = (att.type() == Attendance::CheckIn) ? "✅ Check-In" : "🔴 Check-Out";
        auto* typeItem = new QTableWidgetItem(typeText);
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_attendanceTable->setItem(i, 4, typeItem);
    }
    
    m_attendanceTable->setSortingEnabled(true);
    delete attendanceRepo;
}

void UsersPage::populateUserCombos() {
    // Populate attendance filter combo
    m_attendanceUserCombo->clear();
    m_attendanceUserCombo->addItem("All Users", 0);
    
    // Populate fingerprint card combo
    m_selectUserForCardCombo->clear();
    m_selectUserForCardCombo->addItem("-- Select User --", 0);
    
    for (const auto& user : m_users) {
        m_attendanceUserCombo->addItem(user.name(), user.id());
        m_selectUserForCardCombo->addItem(user.name(), user.id());
    }
}

void UsersPage::onFilterAttendance() {
    loadAttendanceRecords();
}

void UsersPage::onCalculateSalary() {
    int userId = m_attendanceUserCombo->currentData().toInt();
    if (userId == 0) {
        QMessageBox::information(this, "Calculate Salary", 
            "Please select a specific user to calculate their salary.");
        return;
    }
    
    // Find selected user
    User selectedUser;
    for (const auto& u : m_users) {
        if (u.id() == userId) {
            selectedUser = u;
            break;
        }
    }
    
    if (selectedUser.id() == 0) return;
    
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    IAttendanceRepository* attendanceRepo = useSqlite
        ? static_cast<IAttendanceRepository*>(new SQLiteAttendanceRepository)
        : nullptr;
    
    if (!attendanceRepo) return;
    
    QDate fromDate = m_attendanceDateFromEdit->date();
    QDate toDate = m_attendanceDateToEdit->date();
    
    QList<Attendance> records = attendanceRepo->getByUserId(userId);
    
    // Filter by date range
    QList<Attendance> filteredRecords;
    for (const auto& att : records) {
        QDate attDate = att.timestamp().date();
        if (attDate >= fromDate && attDate <= toDate) {
            filteredRecords.append(att);
        }
    }
    
    // Sort by timestamp
    std::sort(filteredRecords.begin(), filteredRecords.end(), 
              [](const Attendance& a, const Attendance& b) {
                  return a.timestamp() < b.timestamp();
              });
    
    // Calculate working hours
    double totalHours = 0.0;
    QDateTime lastCheckIn;
    
    for (const auto& att : filteredRecords) {
        if (att.type() == Attendance::CheckIn) {
            lastCheckIn = att.timestamp();
        } else if (att.type() == Attendance::CheckOut && lastCheckIn.isValid()) {
            qint64 seconds = lastCheckIn.secsTo(att.timestamp());
            totalHours += seconds / 3600.0;
            lastCheckIn = QDateTime();  // Reset
        }
    }
    
    // Calculate salary
    double hourlyRate = selectedUser.hourlyRate();
    double totalSalary = totalHours * hourlyRate;
    
    // Count weekly off days in period
    int totalDays = fromDate.daysTo(toDate) + 1;
    int totalWeeks = totalDays / 7;
    int weeklyOffDays = selectedUser.weeklyOffDays();
    int totalOffDays = totalWeeks * weeklyOffDays;
    
    QString currency = ConfigManager::instance().currencySymbol();
    
    QString message = QString(
        "<h3>Salary Report</h3>"
        "<p><b>User:</b> %1</p>"
        "<p><b>Period:</b> %2 to %3 (%4 days)</p>"
        "<p><b>Total Hours Worked:</b> %5 hours</p>"
        "<p><b>Hourly Rate:</b> %6 %7</p>"
        "<p><b>Weekly Off Days:</b> %8 days/week</p>"
        "<p><b>Total Off Days in Period:</b> %9 days</p>"
        "<hr>"
        "<h2 style='color:#10B981;'><b>Total Salary:</b> %10 %11</h2>"
    ).arg(selectedUser.name())
     .arg(fromDate.toString("yyyy-MM-dd"))
     .arg(toDate.toString("yyyy-MM-dd"))
     .arg(totalDays)
     .arg(QString::number(totalHours, 'f', 2))
     .arg(QString::number(hourlyRate, 'f', 2))
     .arg(currency)
     .arg(weeklyOffDays)
     .arg(totalOffDays)
     .arg(QString::number(totalSalary, 'f', 2))
     .arg(currency);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Salary Calculation");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
    
    delete attendanceRepo;
}

// Builds the card payload for the currently selected user.
// Returns false (and leaves 'out' untouched) when there is nothing to render.
bool UsersPage::buildCardData(int userId, IdCardRenderer::CardData& out, QString* error) const {
    if (userId == 0) {
        if (error) *error = "Select a user to preview their fingerprint card";
        return false;
    }

    User selectedUser;
    for (const auto& u : m_users) {
        if (u.id() == userId) { selectedUser = u; break; }
    }

    if (selectedUser.id() == 0) {
        if (error) *error = "User not found.";
        return false;
    }
    if (selectedUser.fingerprintBarcode().trimmed().isEmpty()) {
        if (error) *error = "User has no fingerprint barcode generated yet!";
        return false;
    }

    out.name       = selectedUser.name();
    out.role       = m_roleMap.value(selectedUser.roleId(), "N/A");
    out.employeeId = QString("ID %1").arg(selectedUser.id(), 4, 10, QLatin1Char('0'));
    out.barcode    = selectedUser.fingerprintBarcode().trimmed();
    out.photoPath  = selectedUser.photoPath();
    // Header strip text. Replace with ConfigManager::instance().companyName()
    // if your config exposes one; leave empty to drop the coloured header.
    out.orgName    = "EMPLOYEE ID";
    return true;
}

void UsersPage::onUserCardSelectionChanged() {
    const int userId = m_selectUserForCardCombo->currentData().toInt();

    IdCardRenderer::CardData data;
    QString error;
    if (!buildCardData(userId, data, &error)) {
        m_cardPreviewLabel->setPixmap(QPixmap());
        m_cardPreviewLabel->setText(
            QString("<div style='color:%1; font-size:14px;'>%2</div>")
                .arg(userId == 0 ? "#9CA3AF" : "#EF4444", error));
        return;
    }

    // Same renderer the printer uses — preview matches the printed card 1:1.
    const int previewDpi = 170;
    QImage cardImage = IdCardRenderer::render(data, previewDpi);
    if (cardImage.isNull()) {
        m_cardPreviewLabel->setText(
            "<div style='color:#EF4444; font-size:14px;'>Could not render the card.</div>");
        return;
    }

    QPixmap pm = QPixmap::fromImage(cardImage);
    pm.setDevicePixelRatio(m_cardPreviewLabel->devicePixelRatioF());
    m_cardPreviewLabel->setText(QString());
    m_cardPreviewLabel->setPixmap(pm);
}

void UsersPage::onPrintCard() {
    const int userId = m_selectUserForCardCombo->currentData().toInt();

    IdCardRenderer::CardData data;
    QString error;
    if (!buildCardData(userId, data, &error)) {
        QMessageBox::warning(this, "Print Card",
                             userId == 0 ? "Please select a user first." : error);
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Portrait);

    QPrintDialog printDialog(&printer, this);
    printDialog.setWindowTitle("Print Fingerprint Card");
    if (printDialog.exec() != QDialog::Accepted) return;

    // Render at the printer's real resolution so the bars land on exact
    // device pixels — that is what keeps the barcode scannable.
    const int dpi = qMax(150, printer.resolution());
    const QImage cardImage = IdCardRenderer::render(data, dpi);
    if (cardImage.isNull()) {
        QMessageBox::critical(this, "Print Card", "Could not render the card.");
        return;
    }

    QPainter painter;
    if (!painter.begin(&printer)) {
        QMessageBox::critical(this, "Print Card", "Could not start the print job.");
        return;
    }

    // Place the card at its true physical size, centred near the top of the page.
    const QRect pageRect = printer.pageRect(QPrinter::DevicePixel).toRect();
    const int cardW = qRound(IdCardRenderer::kCardWidthMm  * dpi / 25.4);
    const int cardH = qRound(IdCardRenderer::kCardHeightMm * dpi / 25.4);
    const int x = pageRect.x() + qMax(0, (pageRect.width()  - cardW) / 2);
    const int y = pageRect.y() + qMax(0, qMin((pageRect.height() - cardH) / 2,
                                              qRound(15.0 * dpi / 25.4)));

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRect(x, y, cardW, cardH), cardImage);

    // Light cut guide so the card can be trimmed to size.
    painter.setPen(QPen(QColor("#9CA3AF"), qMax(1, dpi / 300), Qt::DashLine));
    painter.drawRect(x - 2, y - 2, cardW + 4, cardH + 4);

    painter.end();

    QMessageBox::information(this, "Print Card",
        "Card sent to the printer for \"" + data.name + "\".\n\n"
        "Barcode: " + data.barcode);
}

void UsersPage::onTestCardScan() {
    const QString entered = m_cardTestEdit->text().trimmed();
    if (entered.isEmpty()) return;

    // Which user does this code actually resolve to?
    User matched;
    for (const auto& u : m_users) {
        if (BarcodeAuth::matches(u.fingerprintBarcode(), entered)) { matched = u; break; }
    }

    const int selectedId = m_selectUserForCardCombo->currentData().toInt();

    if (matched.id() == 0) {
        m_cardTestResultLabel->setText(
            "<span style='color:#EF4444; font-weight:bold;'>❌ No user matches this code</span>");
    } else if (selectedId != 0 && matched.id() != selectedId) {
        m_cardTestResultLabel->setText(
            QString("<span style='color:#F59E0B; font-weight:bold;'>⚠ Reads as: %1 (not the selected user)</span>")
                .arg(matched.name()));
    } else {
        m_cardTestResultLabel->setText(
            QString("<span style='color:#10B981; font-weight:bold;'>✅ %1 — card is valid</span>")
                .arg(matched.name()));
    }

    m_cardTestEdit->selectAll();
}
