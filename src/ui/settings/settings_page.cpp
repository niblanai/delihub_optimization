#include "settings_page.h"
#include "custom_theme_dialog.h"
#include "infra/config_manager.h"
#include "infra/backup_manager.h"
#include "infra/schema_deployer.h"
#include "infra/logger.h"
#include "services/branch_manager.h"
#include "services/theme_manager.h"
#include "ui/tr_helper.h"
#include "ui/svg_icon_helper.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QFrame>
#include <QHeaderView>
#include <QCheckBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QApplication>
#include <QFile>

static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    QString devPath = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(devPath)) return devPath;
    return file;
}

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    loadCurrentSettings();
}

void SettingsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    auto* pageTitle = new QLabel("Settings");
    pageTitle->setObjectName("pageTitle");
    pageTitle->setVisible(false);
    m_saveBtn = new QPushButton("💾  Save Settings");
    m_saveBtn->setObjectName("primaryBtn");
    tbLayout->addWidget(pageTitle);
    tbLayout->addStretch();
    tbLayout->addWidget(m_saveBtn);
    root->addWidget(toolbar);

    auto* tabs = new QTabWidget;
    tabs->setContentsMargins(0, 0, 0, 0);

    // ══════════════════════════════════════════════════════════════════
    // TAB 1 — Company / Invoice
    // ══════════════════════════════════════════════════════════════════
    auto* companyWidget = new QWidget;
    auto* companyLayout = new QVBoxLayout(companyWidget);
    companyLayout->setSpacing(16);
    companyLayout->setContentsMargins(24, 20, 24, 20);

    // ── Theme picker card ─────────────────────────────────────────────────────
    auto* themeCard = new QFrame;
    themeCard->setObjectName("kpiCard");
    auto* themeCardLayout = new QVBoxLayout(themeCard);
    themeCardLayout->setSpacing(10);
    themeCardLayout->setContentsMargins(16, 12, 16, 12);

    auto* themeCardTitle = new QLabel("🎨  App Theme");
    themeCardTitle->setObjectName("cardSectionTitle");
    themeCardLayout->addWidget(themeCardTitle);
    auto* themeCardHint = new QLabel("Choose Dark, Light, or create your own Custom theme.");
    themeCardHint->setObjectName("hintLabel");
    themeCardLayout->addWidget(themeCardHint);

    // ── Scrollable theme button area ──────────────────────────────────────────
    auto* scrollWidget = new QWidget;
    auto* themeBtnRow  = new QHBoxLayout(scrollWidget);
    themeBtnRow->setSpacing(10);
    themeBtnRow->setContentsMargins(0, 0, 0, 0);

    auto makeThemeBtn = [](const QString& icon, const QString& label,
                           const QString& value, const QString& accent) -> QPushButton* {
        auto* btn = new QPushButton(icon + "\n" + label);
        btn->setObjectName("secondaryBtn");
        btn->setCheckable(true);
        btn->setProperty("themeValue", value);
        btn->setMinimumHeight(64); btn->setMinimumWidth(90);
        btn->setStyleSheet(QString(
            "QPushButton{border-radius:10px;font-size:11px;font-weight:600;}"
            "QPushButton:checked{background-color:%1;color:#FFFFFF;border:2px solid %1;}")
            .arg(accent));
        return btn;
    };
    auto* darkBtn        = makeThemeBtn("🌙", "Dark",        "dark",        "#4C4FE0");
    auto* lightBtn       = makeThemeBtn("☀️", "Light",       "light",       "#64748B");
    
    // Custom Theme button - opens editor dialog
    auto* customBtn = new QPushButton("🎨\nCustomize");
    customBtn->setObjectName("secondaryBtn");
    customBtn->setCheckable(false);  // Not a theme selector, opens dialog
    customBtn->setMinimumSize(85, 64);
    customBtn->setMaximumSize(85, 64);
    customBtn->setStyleSheet(QString(
        "QPushButton { border: 2px solid %1; color: %1; font-weight: 600; }"
        "QPushButton:hover { background-color: %1; color: white; }"
    ).arg("#FF6B35"));

    QString cur = ConfigManager::instance().theme();
    if (cur == "dark")   darkBtn->setChecked(true);
    if (cur == "light")  lightBtn->setChecked(true);
    if (cur == "custom") {
        // Show that custom theme is active
        customBtn->setText("🎨\nCustom ✓");
        customBtn->setStyleSheet(QString(
            "QPushButton { border: 2px solid %1; background-color: %1; color: white; font-weight: 600; }"
        ).arg("#FF6B35"));
    }

    auto onThemePick = [darkBtn, lightBtn](QPushButton* clicked, const QString& val) {
        for (auto* b : {darkBtn, lightBtn})
            b->setChecked(false);
        clicked->setChecked(true);
        ThemeManager::instance().applyTheme(val);
        ConfigManager::instance().setTheme(val);
    };
    
    QObject::connect(darkBtn,  &QPushButton::clicked, darkBtn,  [=](){ onThemePick(darkBtn,  "dark");  });
    QObject::connect(lightBtn, &QPushButton::clicked, lightBtn, [=](){ onThemePick(lightBtn, "light"); });
    
    // Custom theme button opens the editor dialog
    QObject::connect(customBtn, &QPushButton::clicked, [=]() {
        auto* dialog = new CustomThemeDialog(this);
        if (dialog->exec() == QDialog::Accepted) {
            // Theme was saved and applied in the dialog
            // Reload settings page to reflect new state
            loadCurrentSettings();
        }
        delete dialog;
    });

    themeBtnRow->addWidget(darkBtn);
    themeBtnRow->addWidget(lightBtn);
    themeBtnRow->addWidget(customBtn);
    themeBtnRow->addStretch();

    // Wrap in horizontal scroll area so all buttons are accessible
    auto* themeScroll = new QScrollArea;
    themeScroll->setWidget(scrollWidget);
    themeScroll->setWidgetResizable(true);
    themeScroll->setFrameShape(QFrame::NoFrame);
    themeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    themeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    themeScroll->setFixedHeight(88);   // enough for 64px buttons + scroll bar
    themeCardLayout->addWidget(themeScroll);
    companyLayout->addWidget(themeCard);

    // ── Notes Board Background card ───────────────────────────────────────────
    auto* notesBgCard = new QFrame;
    notesBgCard->setObjectName("kpiCard");
    auto* notesBgLayout = new QVBoxLayout(notesBgCard);
    notesBgLayout->setSpacing(10);
    notesBgLayout->setContentsMargins(16, 12, 16, 12);

    auto* notesBgTitle = new QLabel("📝  Notes Board Background");
    notesBgTitle->setObjectName("cardSectionTitle");
    notesBgLayout->addWidget(notesBgTitle);

    auto* notesBgHint = new QLabel("Choose whether the Notes board uses the current theme color or a custom image.");
    notesBgHint->setObjectName("hintLabel");
    notesBgHint->setWordWrap(true);
    notesBgLayout->addWidget(notesBgHint);

    auto* notesBgRow = new QHBoxLayout;
    notesBgRow->setSpacing(10);

    auto* notesBgModeLabel = new QLabel("Background:");
    notesBgModeLabel->setObjectName("hintLabel");
    m_notesBgModeCombo = new QComboBox;
    m_notesBgModeCombo->addItem("Solid color (follows theme)", "solid");
    m_notesBgModeCombo->addItem("Custom image",                "image");
    notesBgRow->addWidget(notesBgModeLabel);
    notesBgRow->addWidget(m_notesBgModeCombo);
    notesBgRow->addStretch();
    notesBgLayout->addLayout(notesBgRow);

    auto* notesBgImageRow = new QHBoxLayout;
    notesBgImageRow->setSpacing(10);
    m_notesBgImageEdit = new QLineEdit;
    m_notesBgImageEdit->setPlaceholderText("Path to background image (PNG / JPG)");
    m_notesBgBrowseBtn = new QPushButton("Choose image…");
    m_notesBgBrowseBtn->setObjectName("secondaryBtn");
    notesBgImageRow->addWidget(m_notesBgImageEdit, 1);
    notesBgImageRow->addWidget(m_notesBgBrowseBtn);
    notesBgLayout->addLayout(notesBgImageRow);

    // Enable image widgets only when "image" mode is selected
    auto updateNotesBgWidgets = [this]() {
        bool isImage = (m_notesBgModeCombo->currentData().toString() == "image");
        m_notesBgImageEdit->setEnabled(isImage);
        m_notesBgBrowseBtn->setEnabled(isImage);
    };
    updateNotesBgWidgets();
    QObject::connect(m_notesBgModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     notesBgCard, [updateNotesBgWidgets](int){ updateNotesBgWidgets(); });
    QObject::connect(m_notesBgBrowseBtn, &QPushButton::clicked, notesBgCard, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Select Background Image", "",
            "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) m_notesBgImageEdit->setText(path);
    });
    companyLayout->addWidget(notesBgCard);

    auto* companyGroup = new QGroupBox("Company Details (appear on invoices)");
    auto* companyForm  = new QFormLayout(companyGroup);
    companyForm->setSpacing(12);

    m_companyNameEdit  = new QLineEdit;  m_companyNameEdit->setPlaceholderText("DeliHub");
    m_companyAddrEdit  = new QLineEdit;  m_companyAddrEdit->setPlaceholderText("123 Main St, Cairo");
    m_companyPhoneEdit = new QLineEdit;  m_companyPhoneEdit->setPlaceholderText("+20 1xx xxx xxxx");
    m_companyTaxEdit   = new QLineEdit;  m_companyTaxEdit->setPlaceholderText("Tax registration number");

    auto* logoRow = new QHBoxLayout;
    m_companyLogoEdit = new QLineEdit;
    m_companyLogoEdit->setPlaceholderText("Path to company logo (PNG)");
    auto* browseLogoBtn = new QPushButton("Browse…");
    browseLogoBtn->setObjectName("secondaryBtn");
    logoRow->addWidget(m_companyLogoEdit, 1);
    logoRow->addWidget(browseLogoBtn);

    companyForm->addRow("Company Name:", m_companyNameEdit);
    companyForm->addRow("Address:",       m_companyAddrEdit);
    companyForm->addRow("Phone:",         m_companyPhoneEdit);
    companyForm->addRow("Tax Number:",    m_companyTaxEdit);
    companyForm->addRow("Logo:",          logoRow);

    companyLayout->addWidget(companyGroup);
    
    // ── POS Security Settings ─────────────────────────────────────────────────
    auto* posSecurityGroup = new QGroupBox("🔒  POS Security");
    auto* posSecurityLayout = new QVBoxLayout(posSecurityGroup);
    posSecurityLayout->setSpacing(10);
    
    m_requireAuthForPOSDeletionCheckbox = new QCheckBox("Require manager authorization to delete items from POS cart");
    m_requireAuthForPOSDeletionCheckbox->setToolTip("When enabled, deleting items from POS requires scanning a manager's fingerprint barcode");
    posSecurityLayout->addWidget(m_requireAuthForPOSDeletionCheckbox);
    
    auto* posHint = new QLabel("Managers must have 'Can Delete From POS' permission in their role.");
    posHint->setObjectName("hintLabel");
    posHint->setWordWrap(true);
    posSecurityLayout->addWidget(posHint);
    
    companyLayout->addWidget(posSecurityGroup);
    companyLayout->addStretch();
    tabs->addTab(companyWidget, "🏭  Company");

    connect(browseLogoBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Select Logo", "", "Images (*.png *.jpg *.jpeg *.svg)");
        if (!path.isEmpty()) m_companyLogoEdit->setText(path);
    });

    // ══════════════════════════════════════════════════════════════════
    // TAB 2 — Branch & Currency
    // ══════════════════════════════════════════════════════════════════
    auto* branchWidget = new QWidget;
    auto* branchLayout = new QVBoxLayout(branchWidget);
    branchLayout->setSpacing(16);
    branchLayout->setContentsMargins(24, 20, 24, 20);

    auto* branchGroup = new QGroupBox("Current Branch Info");
    auto* branchForm  = new QFormLayout(branchGroup);
    branchForm->setSpacing(12);

    m_branchNameEdit = new QLineEdit;
    m_branchNameEdit->setPlaceholderText("e.g. Main Branch");
    branchForm->addRow("Branch Name:", m_branchNameEdit);

    m_currencyEdit = new QLineEdit;
    m_currencyEdit->setPlaceholderText("£  $  EGP");
    m_currencyEdit->setMaxLength(6);
    branchForm->addRow("Currency Symbol:", m_currencyEdit);

    m_taxRateSpin = new QDoubleSpinBox;
    m_taxRateSpin->setRange(0.0, 100.0);
    m_taxRateSpin->setDecimals(2);
    m_taxRateSpin->setSuffix(" %");
    m_taxRateSpin->setToolTip("Tax rate applied to products with tax enabled (e.g., 14.00 for 14% VAT)");
    branchForm->addRow("Tax Rate:", m_taxRateSpin);

    branchLayout->addWidget(branchGroup);
    branchLayout->addStretch();
    tabs->addTab(branchWidget, "🏢  Branch");

    // ══════════════════════════════════════════════════════════════════
    // TAB 3 — Branches Management (Multi-Branch)
    // ══════════════════════════════════════════════════════════════════
    auto* branchesWidget = new QWidget;
    auto* branchesLayout = new QVBoxLayout(branchesWidget);
    branchesLayout->setSpacing(0);
    branchesLayout->setContentsMargins(0, 0, 0, 0);

    auto* btb = new QFrame;
    btb->setObjectName("pageToolbar");
    auto* btbLayout = new QHBoxLayout(btb);
    btbLayout->setContentsMargins(16, 8, 16, 8);
    auto* branchesTitle = new QLabel("Multi-Branch Configuration");
    branchesTitle->setStyleSheet("font-size:14px; font-weight:bold;");
    m_addBranchBtn    = new QPushButton("＋ Add Branch");    m_addBranchBtn->setObjectName("primaryBtn");
    m_editBranchBtn   = new QPushButton("✏️ Edit");           m_editBranchBtn->setObjectName("secondaryBtn"); m_editBranchBtn->setEnabled(false);
    m_deleteBranchBtn = new QPushButton("Delete");
    m_deleteBranchBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                    QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteBranchBtn->setObjectName("dangerBtn");  m_deleteBranchBtn->setEnabled(false);
    btbLayout->addWidget(branchesTitle); btbLayout->addStretch();
    btbLayout->addWidget(m_addBranchBtn); btbLayout->addWidget(m_editBranchBtn); btbLayout->addWidget(m_deleteBranchBtn);
    branchesLayout->addWidget(btb);

    auto* branchNote = new QLabel(
        "ℹ️  Each branch has its own database. SQLite paths support network shares (\\\\server\\...).\n"
        "    Supabase: paste the full PostgreSQL URL (postgresql://...).");
    branchNote->setObjectName("hintLabel");
    branchNote->setWordWrap(true);
    branchNote->setContentsMargins(16, 8, 16, 0);
    branchesLayout->addWidget(branchNote);

    m_branchesTable = new QTableWidget(0, 3);
    m_branchesTable->setObjectName("dataTable");
    m_branchesTable->setHorizontalHeaderLabels({"#", "Branch Name", "Database Path / URL"});
    m_branchesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_branchesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_branchesTable->setColumnWidth(0, 40);
    m_branchesTable->verticalHeader()->setVisible(false);
    m_branchesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branchesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_branchesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_branchesTable->setAlternatingRowColors(true);
    branchesLayout->addWidget(m_branchesTable, 1);

    tabs->addTab(branchesWidget, "🌐  Branches");

    connect(m_addBranchBtn,    &QPushButton::clicked, this, &SettingsPage::onAddBranch);
    connect(m_editBranchBtn,   &QPushButton::clicked, this, &SettingsPage::onEditBranch);
    connect(m_deleteBranchBtn, &QPushButton::clicked, this, &SettingsPage::onDeleteBranch);
    connect(m_branchesTable, &QTableWidget::itemSelectionChanged, this, [this](){
        bool has = m_branchesTable->currentRow() >= 0;
        m_editBranchBtn->setEnabled(has);
        m_deleteBranchBtn->setEnabled(has);

        // ── Auto-fill Branch tab name field from the selected row ────────────
        if (has) {
            QString selectedName = m_branchesTable->item(m_branchesTable->currentRow(), 1)->text();
            if (!selectedName.isEmpty())
                m_branchNameEdit->setText(selectedName);
        }
    });
    connect(m_branchesTable, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onEditBranch(); });

    // Deploy schema button
    auto* deployRow = new QHBoxLayout;
    auto* deployBtn = new QPushButton("☁️  Deploy Schema to Selected Cloud Branch");
    deployBtn->setObjectName("primaryBtn");
    auto* deployNote = new QLabel("Pushes the database structure to Supabase (no data).");
    deployNote->setObjectName("hintLabel");
    deployRow->addWidget(deployBtn); deployRow->addWidget(deployNote, 1);
    branchesLayout->addLayout(deployRow);

    connect(deployBtn, &QPushButton::clicked, this, [this]() {
        int row = m_branchesTable->currentRow();
        if (row < 0) { QMessageBox::information(this,"Select Branch","Please select a cloud branch first."); return; }
        int id = m_branchesTable->item(row,0)->text().toInt();
        BranchInfo b = BranchManager::instance().branchById(id);
        if (!b.isCloud) { QMessageBox::warning(this,"Not Cloud","This branch is not a cloud (Supabase) branch."); return; }

        // Derive schema name from the branch name itself (not the local id),
        // so re-typing the same branch name on another device resolves to
        // this exact same schema instead of creating a new one.
        QString schema = SchemaDeployer::sanitizeSchemaName(b.name);

        QString err;
        if (SchemaDeployer::deployToPostgres(b.dbPath, schema, err)) {
            // Update branch DBPath to include schema hint
            QMessageBox::information(this,"Success",
                QString("Schema '%1' created on Supabase.\n\n"
                        "All tables are ready. Users can now log in to this branch.")
                    .arg(schema));
        } else {
            QMessageBox::critical(this,"Deploy Failed", err);
        }
    });

    // ── Upload local SQLite → cloud schema ────────────────────────────────────
    auto* migrateRow  = new QHBoxLayout;
    auto* migrateBtn  = new QPushButton("⬆️  Upload Local DB to Selected Cloud Branch");
    migrateBtn->setObjectName("secondaryBtn");
    auto* migrateNote = new QLabel("Copies all local data into the Supabase schema. Safe to re-run.");
    migrateNote->setObjectName("hintLabel");
    migrateRow->addWidget(migrateBtn); migrateRow->addWidget(migrateNote, 1);
    branchesLayout->addLayout(migrateRow);

    // Progress label (hidden until migration starts)
    auto* migrateStatus = new QLabel;
    migrateStatus->setObjectName("hintLabel");
    migrateStatus->setWordWrap(true);
    migrateStatus->setVisible(false);
    branchesLayout->addWidget(migrateStatus);

    connect(migrateBtn, &QPushButton::clicked, this, [this, migrateBtn, migrateStatus]() {
        // ── 1. Validate selection ─────────────────────────────────────────────
        int row = m_branchesTable->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Select Branch",
                "Please select a cloud branch to upload into.");
            return;
        }
        int id = m_branchesTable->item(row, 0)->text().toInt();
        BranchInfo b = BranchManager::instance().branchById(id);
        if (!b.isCloud) {
            QMessageBox::warning(this, "Not a Cloud Branch",
                "The selected branch is not a cloud (Supabase) branch.\n"
                "Please select a branch whose DB Path starts with postgresql://");
            return;
        }

        // ── 2. Pick source SQLite file ────────────────────────────────────────
        QString srcPath = QFileDialog::getOpenFileName(
            this, "Select Local Database to Upload",
            ConfigManager::instance().sqlitePath(),
            "SQLite Databases (*.db);;All Files (*)");
        if (srcPath.isEmpty()) return;

        // ── 3. Warn user ──────────────────────────────────────────────────────
        auto reply = QMessageBox::warning(this, "Upload Local DB to Cloud",
            QString("This will copy ALL data from:\n  %1\n\ninto the Supabase schema:\n  %2\n\n"
                    "Existing cloud rows with the same ID are kept unchanged\n"
                    "(INSERT … ON CONFLICT DO NOTHING).\n\n"
                    "Continue?").arg(srcPath, SchemaDeployer::sanitizeSchemaName(b.name)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        // ── 4. Run migration (blocking — runs on UI thread with status updates) ─
        migrateBtn->setEnabled(false);
        migrateStatus->setStyleSheet("");
        migrateStatus->setText("⏳  Starting migration…");
        migrateStatus->setVisible(true);
        QApplication::processEvents();

        QString schema = SchemaDeployer::sanitizeSchemaName(b.name);

        auto progress = [this, migrateStatus](const QString& tbl, int done, int total) {
            QString msg = total > 0
                ? QString("⏳  %1 — %2 / %3 rows").arg(tbl).arg(done).arg(total)
                : QString("⏳  %1 — skipped (empty)").arg(tbl);
            migrateStatus->setText(msg);
            QApplication::processEvents();
        };

        QString err;
        bool ok = SchemaDeployer::migrateFromSqlite(srcPath, b.dbPath, schema, progress, err);

        migrateBtn->setEnabled(true);

        if (ok) {
            migrateStatus->setStyleSheet("color:#4ADE80;");
            migrateStatus->setText("✅  Migration completed successfully.");
            QMessageBox::information(this, "Upload Complete",
                QString("All data from the local database has been uploaded to\n"
                        "the Supabase schema '%1'.\n\n"
                        "You can now log in to this branch from any device.")
                    .arg(schema));
        } else {
            migrateStatus->setStyleSheet("color:#F87171;");
            migrateStatus->setText("❌  " + err);
            QMessageBox::critical(this, "Migration Failed", err);
        }
    });

    // ══════════════════════════════════════════════════════════════════
    // TAB 4 — Database (raw connection)
    // ══════════════════════════════════════════════════════════════════
    auto* dbWidget = new QWidget;
    auto* dbLayout = new QVBoxLayout(dbWidget);
    dbLayout->setSpacing(16);
    dbLayout->setContentsMargins(24, 20, 24, 20);

    auto* dbGroup = new QGroupBox("Database Connection (default branch)");
    auto* dbForm  = new QFormLayout(dbGroup);
    dbForm->setSpacing(12);

    m_dbTypeCombo = new QComboBox;
    m_dbTypeCombo->addItem("SQLite (local)",     "QSQLITE");
    m_dbTypeCombo->addItem("Access via ODBC",    "QODBC");
    m_dbTypeCombo->addItem("PostgreSQL/Supabase","QPSQL");
    dbForm->addRow("Database Type:", m_dbTypeCombo);

    m_sqlitePathEdit = new QLineEdit;
    m_sqlitePathEdit->setPlaceholderText("C:/data/delivery.db  or  \\\\server\\share\\branch.db");
    dbForm->addRow("SQLite / Network Path:", m_sqlitePathEdit);

    m_odbcEdit = new QLineEdit;
    m_odbcEdit->setPlaceholderText("Driver={...};DBQ=...  OR  postgresql://user:pass@host:5432/db");
    dbForm->addRow("ODBC / Supabase URL:", m_odbcEdit);

    auto* dbNote = new QLabel("ℹ️  For Supabase, paste the full PostgreSQL URL in the Supabase URL field and set type to PostgreSQL/Supabase.");
    dbNote->setObjectName("hintLabel"); dbNote->setWordWrap(true);

    dbLayout->addWidget(dbGroup); dbLayout->addWidget(dbNote); dbLayout->addStretch();
    tabs->addTab(dbWidget, "🗄️  Database");

    connect(m_dbTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        bool isOdbc   = m_dbTypeCombo->currentData().toString() == "QODBC";
        bool isPsql   = m_dbTypeCombo->currentData().toString() == "QPSQL";
        m_sqlitePathEdit->setEnabled(!isOdbc && !isPsql);
        m_odbcEdit->setEnabled(isOdbc || isPsql);
    });

    // ══════════════════════════════════════════════════════════════════
    // TAB 5 — Backup / Restore
    // ══════════════════════════════════════════════════════════════════
    auto* backupWidget = new QWidget;
    auto* backupLayout = new QVBoxLayout(backupWidget);
    backupLayout->setSpacing(16);
    backupLayout->setContentsMargins(24, 20, 24, 20);

    auto* backupGroup   = new QGroupBox("Database Backup & Restore");
    auto* backupVLayout = new QVBoxLayout(backupGroup);
    backupVLayout->setSpacing(12);

    auto* backupDesc = new QLabel(
        "Create a copy of the current database, or restore from a previous backup.\n"
        "The app will briefly close the database connection during the operation.");
    backupDesc->setObjectName("hintLabel"); backupDesc->setWordWrap(true);
    backupVLayout->addWidget(backupDesc);

    auto* btnRow = new QHBoxLayout;
    m_backupBtn  = new QPushButton("📦  Backup Database");    m_backupBtn->setObjectName("primaryBtn");
    m_restoreBtn = new QPushButton("♻️  Restore from Backup"); m_restoreBtn->setObjectName("secondaryBtn");
    btnRow->addWidget(m_backupBtn); btnRow->addWidget(m_restoreBtn); btnRow->addStretch();
    backupVLayout->addLayout(btnRow);

    m_backupStatus = new QLabel; m_backupStatus->setObjectName("hintLabel"); m_backupStatus->setWordWrap(true);
    backupVLayout->addWidget(m_backupStatus);

    backupLayout->addWidget(backupGroup); backupLayout->addStretch();
    tabs->addTab(backupWidget, "💾  Backup");

    // ══════════════════════════════════════════════════════════════════
    // TAB — Receipt Designer
    // ══════════════════════════════════════════════════════════════════
    m_receiptDesigner = new ReceiptDesignerWidget;
    tabs->addTab(m_receiptDesigner, "🧾  Receipt Designer");

    // ── NOTE: Reminders tab removed — reminder settings are managed directly
    // in the Scheduled Orders page (per-schedule settings).

    root->addWidget(tabs, 1);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_saveBtn,    &QPushButton::clicked, this, &SettingsPage::onSaveConfig);
    connect(m_backupBtn,  &QPushButton::clicked, this, &SettingsPage::onBackup);
    connect(m_restoreBtn, &QPushButton::clicked, this, &SettingsPage::onRestore);
}

void SettingsPage::refresh() {
    if (m_receiptDesigner) m_receiptDesigner->reload();
    loadCurrentSettings();
    loadBranches();
}

void SettingsPage::retranslateUi() { retranslateWidget(this); }

void SettingsPage::loadCurrentSettings() {
    const auto& cfg = ConfigManager::instance();

    int dbIdx = m_dbTypeCombo->findData(cfg.databaseType());
    m_dbTypeCombo->setCurrentIndex(dbIdx >= 0 ? dbIdx : 0);

    m_sqlitePathEdit->setText(cfg.sqlitePath());
    m_odbcEdit->setText(cfg.odbcConnectionString());

    // Branch name: prefer the active branch from BranchManager (reflects login choice)
    QString activeName = BranchManager::instance().activeBranch().name;
    m_branchNameEdit->setText(activeName.isEmpty() ? cfg.branchName() : activeName);
    m_currencyEdit->setText(cfg.currencySymbol());
    m_taxRateSpin->setValue(cfg.taxRate());

    m_companyNameEdit->setText(cfg.companyName());
    m_companyAddrEdit->setText(cfg.companyAddress());
    m_companyPhoneEdit->setText(cfg.companyPhone());
    m_companyTaxEdit->setText(cfg.companyTaxNumber());
    m_companyLogoEdit->setText(cfg.invoiceLogoPath());
    
    // POS Security
    m_requireAuthForPOSDeletionCheckbox->setChecked(cfg.requireAuthForPOSDeletion());

    bool isOdbc = cfg.databaseType() == "QODBC";
    bool isPsql = cfg.databaseType() == "QPSQL";
    m_sqlitePathEdit->setEnabled(!isOdbc && !isPsql);
    m_odbcEdit->setEnabled(isOdbc || isPsql);

    // Reminder settings
    if (m_remindEnabledChk)
        m_remindEnabledChk->setChecked(
        cfg.value("Reminders/Enabled", "true") != "false");
    if (m_remindMinutesBefore)
        m_remindMinutesBefore->setValue(
        cfg.value("Reminders/MinutesBefore", "60").toInt());
    if (m_remindIntervalMin)
        m_remindIntervalMin->setValue(
        cfg.value("Reminders/CheckIntervalMinutes", "5").toInt());

    // Notes board background
    if (m_notesBgModeCombo) {
        int modeIdx = m_notesBgModeCombo->findData(cfg.notesBgMode());
        m_notesBgModeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    }
    if (m_notesBgImageEdit)
        m_notesBgImageEdit->setText(cfg.notesBgImagePath());

    loadBranches();
}

void SettingsPage::loadBranches() {
    const auto& branches = BranchManager::instance().branches();
    m_branchesTable->setRowCount(0);
    for (const auto& b : branches) {
        int row = m_branchesTable->rowCount();
        m_branchesTable->insertRow(row);
        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a); it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        m_branchesTable->setItem(row, 0, cell(QString::number(b.id)));
        m_branchesTable->setItem(row, 1, cell(b.name, Qt::AlignLeft | Qt::AlignVCenter));
        QString pathDisplay = b.isCloud
            ? "☁️ " + b.dbPath.left(50) + (b.dbPath.length() > 50 ? "..." : "")
            : "🏢 " + b.dbPath;
        m_branchesTable->setItem(row, 2, cell(pathDisplay, Qt::AlignLeft | Qt::AlignVCenter));
    }
}

static BranchInfo showBranchDialog(const BranchInfo& existing, QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(existing.id > 0 ? "Edit Branch" : "Add Branch");
    dlg.setMinimumWidth(500);
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(existing.name);
    nameEdit->setPlaceholderText("e.g. Cairo Main Branch");
    form->addRow("Branch Name *", nameEdit);

    auto* pathEdit = new QLineEdit(existing.dbPath);
    pathEdit->setPlaceholderText("C:/data/branch.db  or  \\\\server\\share\\branch.db  or  postgresql://...");
    form->addRow("Database Path / Supabase URL *", pathEdit);

    auto* cloudChk = new QCheckBox("This is a cloud database (Supabase / PostgreSQL)");
    cloudChk->setChecked(existing.isCloud || existing.dbPath.startsWith("postgresql://") || existing.dbPath.startsWith("postgres://"));
    form->addRow("", cloudChk);

    // Auto-detect cloud from URL
    QObject::connect(pathEdit, &QLineEdit::textChanged, cloudChk, [cloudChk](const QString& t){
        if (t.startsWith("postgresql://") || t.startsWith("postgres://"))
            cloudChk->setChecked(true);
    });

    auto* note = new QLabel(
        "<b>Supabase:</b> paste the full URL from:<br>"
        "Dashboard → Settings → Database → Connection string");
    note->setObjectName("hintLabel");
    note->setWordWrap(true);

    layout->addLayout(form);
    layout->addWidget(note);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    layout->addWidget(btns);

    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&](){
        if (nameEdit->text().trimmed().isEmpty() || pathEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Validation", "Name and database path are required.");
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return {};

    BranchInfo b;
    b.id       = existing.id;
    b.name     = nameEdit->text().trimmed();
    b.dbPath   = pathEdit->text().trimmed();
    b.isCloud  = cloudChk->isChecked();
    return b;
}

void SettingsPage::onAddBranch() {
    BranchInfo b = showBranchDialog({}, this);
    if (b.name.isEmpty()) return;

    // ── Step 0.5: resolve bare-folder path → branch subfolder ────────────────
    if (!b.isCloud) {
        QFileInfo fi(b.dbPath);

        // A path is "bare" when it has no suffix that looks like a db filename.
        // We treat it as bare when the path has no extension OR the whole path
        // string has no dot after the last path separator.
        bool isBareFolder = fi.suffix().isEmpty();

        if (isBareFolder) {
            // Sanitize the branch name for Windows filesystem:
            // strip \ / : * ? " < > | (invalid on Windows)
            QString safeName = b.name;
            static const QRegularExpression kWinBadChars(R"([\\/:*?"<>|])");
            safeName.remove(kWinBadChars);
            safeName = safeName.trimmed();
            if (safeName.isEmpty()) safeName = "branch_" + QString::number(b.id > 0 ? b.id : 1);

            // Build: <userPath>/<safeName>/
            QString branchDir = QDir(b.dbPath).absoluteFilePath(safeName);
            if (!QDir().mkpath(branchDir)) {
                QMessageBox::critical(this, "Error",
                    QString("Could not create folder:\n%1\n\nCheck that the path exists and is writable.")
                        .arg(branchDir));
                return;
            }

            // Create app.log (empty — Logger will append to it)
            QString logPath = branchDir + "/app.log";
            if (!QFile::exists(logPath)) {
                QFile lf(logPath);
                if (lf.open(QIODevice::WriteOnly))
                    lf.close();
            }

            // Create delivery_system.db (empty file — schema initialized on first login)
            QString dbFilePath = branchDir + "/delivery_system.db";
            if (!QFile::exists(dbFilePath)) {
                QFile df(dbFilePath);
                if (!df.open(QIODevice::WriteOnly)) {
                    QMessageBox::critical(this, "Error",
                        QString("Could not create database file:\n%1").arg(dbFilePath));
                    return;
                }
                df.close();
            }

            // Create config.ini for this branch
            QString cfgPath = branchDir + "/config.ini";
            if (!QFile::exists(cfgPath)) {
                QSettings branchCfg(cfgPath, QSettings::IniFormat);
                branchCfg.setValue("Database/Type",      "SQLITE");
                branchCfg.setValue("Database/SQLitePath", dbFilePath);
                branchCfg.setValue("App/BranchName",      b.name);
                branchCfg.sync();
            }

            // Override dbPath to point at the actual .db file (absolute path)
            b.dbPath = QDir::toNativeSeparators(dbFilePath);

            Logger::instance().info(
                QString("Branch '%1' folder created at: %2").arg(b.name, branchDir));
        }
    }

    // Assign new ID
    int maxId = 0;
    for (const auto& existing : BranchManager::instance().branches())
        maxId = qMax(maxId, existing.id);
    b.id = maxId + 1;

    BranchManager::instance().saveBranch(b);
    loadBranches();

    QString msg = b.isCloud
        ? QString("Cloud branch \"%1\" added.\n\nLog out and select this branch to use it.").arg(b.name)
        : QString("Branch \"%1\" added.\n\nDatabase: %2\n\nLog out and select this branch to use it.")
              .arg(b.name, b.dbPath);
    QMessageBox::information(this, "Branch Added", msg);
}

void SettingsPage::onEditBranch() {
    int row = m_branchesTable->currentRow();
    if (row < 0) return;
    int id = m_branchesTable->item(row, 0)->text().toInt();
    BranchInfo current = BranchManager::instance().branchById(id);
    BranchInfo updated = showBranchDialog(current, this);
    if (updated.name.isEmpty()) return;
    BranchManager::instance().saveBranch(updated);
    loadBranches();
}

void SettingsPage::onDeleteBranch() {
    int row = m_branchesTable->currentRow();
    if (row < 0) return;
    int id = m_branchesTable->item(row, 0)->text().toInt();
    QString name = m_branchesTable->item(row, 1)->text();
    if (BranchManager::instance().branches().size() <= 1) {
        QMessageBox::warning(this, "Cannot Delete", "You must keep at least one branch.");
        return;
    }
    auto reply = QMessageBox::question(this, "Confirm", "Delete branch \"" + name + "\"?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        BranchManager::instance().removeBranch(id);
        loadBranches();
    }
}

void SettingsPage::onSaveConfig() {
    auto& cfg = ConfigManager::instance();

    cfg.setDatabaseType(m_dbTypeCombo->currentData().toString());
    cfg.setSqlitePath(m_sqlitePathEdit->text().trimmed());
    cfg.setOdbcConnectionString(m_odbcEdit->text().trimmed());
    cfg.setBranchName(m_branchNameEdit->text().trimmed());
    cfg.setCurrencySymbol(m_currencyEdit->text().trimmed());
    cfg.setTaxRate(m_taxRateSpin->value());

    cfg.setCompanyName(m_companyNameEdit->text().trimmed());
    cfg.setCompanyAddress(m_companyAddrEdit->text().trimmed());
    cfg.setCompanyPhone(m_companyPhoneEdit->text().trimmed());
    cfg.setCompanyTaxNumber(m_companyTaxEdit->text().trimmed());
    cfg.setInvoiceLogoPath(m_companyLogoEdit->text().trimmed());
    
    // POS Security
    cfg.setRequireAuthForPOSDeletion(m_requireAuthForPOSDeletionCheckbox->isChecked());

    // Reminder settings — persist to config.ini
    if (m_remindEnabledChk) cfg.setOverride("Reminders/Enabled", m_remindEnabledChk->isChecked() ? "true" : "false");
    if (m_remindMinutesBefore) cfg.setOverride("Reminders/MinutesBefore", QString::number(m_remindMinutesBefore->value()));
    if (m_remindIntervalMin) cfg.setOverride("Reminders/CheckIntervalMinutes", QString::number(m_remindIntervalMin->value()));
    // Also write to the actual QSettings file
    {
        QSettings s(cfg.configFilePath(), QSettings::IniFormat);
        if (m_remindEnabledChk)   s.setValue("Reminders/Enabled",              m_remindEnabledChk->isChecked());
        if (m_remindMinutesBefore) s.setValue("Reminders/MinutesBefore",        m_remindMinutesBefore->value());
        if (m_remindIntervalMin)   s.setValue("Reminders/CheckIntervalMinutes", m_remindIntervalMin->value());
    }

    // Notes board background
    if (m_notesBgModeCombo)
        cfg.setNotesBgMode(m_notesBgModeCombo->currentData().toString());
    if (m_notesBgImageEdit)
        cfg.setNotesBgImagePath(m_notesBgImageEdit->text().trimmed());

    Logger::instance().info("Settings saved.");
    QMessageBox::information(this, "Settings", "Settings saved.\nDatabase changes take effect after restart.");
}

void SettingsPage::onBackup() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save Backup", "backup_delivery.db", "Database Files (*.db *.accdb);;All Files (*)");
    if (path.isEmpty()) return;

    QString err;
    if (BackupManager::instance().backupDatabase(path, err)) {
        m_backupStatus->setStyleSheet("color:#4ADE80;");
        m_backupStatus->setText("✅  Backup saved: " + path);
        Logger::instance().info("Backup: " + path);
        QMessageBox::information(this, "Backup", "Backup completed.");
    } else {
        m_backupStatus->setStyleSheet("color:#F87171;");
        m_backupStatus->setText("❌  " + err);
        QMessageBox::critical(this, "Backup Failed", err);
    }
}

void SettingsPage::onRestore() {
    auto reply = QMessageBox::warning(this, "Restore",
        "This will REPLACE the current database. Continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString path = QFileDialog::getOpenFileName(
        this, "Select Backup", "", "Database Files (*.db *.accdb);;All Files (*)");
    if (path.isEmpty()) return;

    QString err;
    if (BackupManager::instance().restoreDatabase(path, err)) {
        m_backupStatus->setStyleSheet("color:#4ADE80;");
        m_backupStatus->setText("✅  Restored from: " + path);
        QMessageBox::information(this, "Restore", "Restored. Please restart the app.");
    } else {
        m_backupStatus->setStyleSheet("color:#F87171;");
        m_backupStatus->setText("❌  " + err);
        QMessageBox::critical(this, "Restore Failed", err);
    }
}

