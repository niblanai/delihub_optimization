#include "coupons_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "services/archive_manager.h"
#include "services/audit_service.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/tr_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
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

CouponsPage::CouponsPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_repo = useSqlite ? static_cast<ICouponRepository*>(new SQLiteCouponRepository)
                       : static_cast<ICouponRepository*>(new SQLiteCouponRepository); // Access stub same
    setupUi();
    // Data loaded by MainWindow::navigateTo — no singleShot needed
}

void CouponsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0); root->setContentsMargins(0,0,0,0);

    auto* toolbar = new QFrame; toolbar->setObjectName("pageToolbar");
    auto* tbL = new QHBoxLayout(toolbar); tbL->setContentsMargins(16,10,16,10);
    auto* title = new QLabel("Coupons & Discounts"); title->setObjectName("pageTitle");
    title->setVisible(false);
    m_addBtn    = new QPushButton(LangManager::instance().t("＋ Add Coupon"));
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
    tbL->addWidget(title); tbL->addStretch();
    tbL->addWidget(m_addBtn); tbL->addWidget(m_editBtn); tbL->addWidget(m_deleteBtn);
    root->addWidget(toolbar);

    m_table = new QTableWidget(0, 7);
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("Code"),
        LangManager::instance().t("Description"),
        LangManager::instance().t("Type"),
        LangManager::instance().t("Value"),
        LangManager::instance().t("Expiry"),
        LangManager::instance().t("Uses"),
        LangManager::instance().t("Active")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    auto* sb = new QFrame; sb->setObjectName("statusBar");
    auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(16,4,16,4);
    m_statusLbl = new QLabel("0 coupons"); m_statusLbl->setObjectName("statusLabel");
    sbl->addWidget(m_statusLbl); root->addWidget(sb);

    connect(m_addBtn,    &QPushButton::clicked, this, &CouponsPage::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &CouponsPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CouponsPage::onDelete);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &CouponsPage::onSelectionChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onEdit(); });
}

void CouponsPage::refresh() { loadCoupons(); }
void CouponsPage::retranslateUi() { retranslateWidget(this); }

void CouponsPage::loadCoupons() {
    m_coupons = m_repo->getAll();
    m_table->setSortingEnabled(false);   // prevent row scrambling during populate
    m_table->setRowCount(0);
    const QString sym = ConfigManager::instance().currencySymbol();
    for (const auto& c : m_coupons) {
        int row = m_table->rowCount(); m_table->insertRow(row);
        auto cell = [](const QString& t, Qt::Alignment a=Qt::AlignCenter){
            auto* it=new QTableWidgetItem(t); it->setTextAlignment(a); it->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable); return it;
        };
        m_table->setItem(row,0,cell(c.code()));
        m_table->setItem(row,1,cell(c.description(), Qt::AlignLeft|Qt::AlignVCenter));
        m_table->setItem(row,2,cell(c.type()==Coupon::Type::Percentage ? "%" : sym));
        m_table->setItem(row,3,cell(c.type()==Coupon::Type::Percentage
            ? QString::number(c.value(),'f',1)+"%"
            : QString::number(c.value(),'f',2)+" "+sym));
        m_table->setItem(row,4,cell(c.expiryDate().isValid()?c.expiryDate().toString("yyyy-MM-dd"):"∞"));
        m_table->setItem(row,5,cell(c.maxUses()>0 ? QString("%1/%2").arg(c.usedCount()).arg(c.maxUses()) : QString::number(c.usedCount())));
        auto* activeItem = cell(c.isValid()?"✅ Active":"❌ Inactive");
        activeItem->setForeground(c.isValid()?QColor("#4ADE80"):QColor("#F87171"));
        m_table->setItem(row,6,activeItem);
    }
    m_statusLbl->setText(QString("%1 coupon(s)").arg(m_coupons.size()));
    m_table->setSortingEnabled(true);
    m_editBtn->setEnabled(false); m_deleteBtn->setEnabled(false);
}

static Coupon showCouponDialog(const Coupon& existing, QWidget* parent) {
    QDialog dlg(parent); dlg.setWindowTitle(existing.id()>0?"Edit Coupon":"Add Coupon");
    dlg.setMinimumWidth(380); dlg.setModal(true);
    auto* form = new QFormLayout(&dlg); form->setSpacing(10); form->setContentsMargins(16,16,16,16);
    auto* codeEdit  = new QLineEdit(existing.code()); codeEdit->setPlaceholderText("e.g. SAVE10");
    auto* descEdit  = new QLineEdit(existing.description());
    auto* typeCombo = new QComboBox; typeCombo->addItem("Fixed Amount","FixedAmount"); typeCombo->addItem("Percentage (%)","Percentage");
    if (existing.type()==Coupon::Type::Percentage) typeCombo->setCurrentIndex(1);
    auto* valueSpin = new QDoubleSpinBox; valueSpin->setRange(0,100000); valueSpin->setDecimals(2); valueSpin->setValue(existing.value());
    auto* maxSpin   = new QSpinBox; maxSpin->setRange(0,999999); maxSpin->setValue(existing.maxUses()); maxSpin->setSpecialValueText("Unlimited");
    auto* expiryChk = new QCheckBox("Set expiry date"); expiryChk->setChecked(existing.expiryDate().isValid());
    auto* expiryEdit= new QDateEdit(existing.expiryDate().isValid()?existing.expiryDate():QDate::currentDate().addMonths(1));
    expiryEdit->setCalendarPopup(true); expiryEdit->setEnabled(expiryChk->isChecked());
    auto* activeChk = new QCheckBox("Active"); activeChk->setChecked(existing.isActive());
    form->addRow("Code *",       codeEdit);
    form->addRow("Description",  descEdit);
    form->addRow("Discount Type",typeCombo);
    form->addRow("Value *",      valueSpin);
    form->addRow("Max Uses",     maxSpin);
    form->addRow("",             expiryChk);
    form->addRow("Expiry Date",  expiryEdit);
    form->addRow("",             activeChk);
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Save)->setObjectName("saveBtn"); form->addRow(btns);
    QObject::connect(expiryChk,&QCheckBox::toggled,expiryEdit,&QDateEdit::setEnabled);
    QObject::connect(btns,&QDialogButtonBox::accepted,&dlg,[&](){
        if (codeEdit->text().trimmed().isEmpty()||valueSpin->value()<=0) {
            QMessageBox::warning(&dlg,"Validation","Code and value are required."); return; } dlg.accept(); });
    QObject::connect(btns,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    if (dlg.exec()!=QDialog::Accepted) return {};
    Coupon c; c.setId(existing.id()); c.setCode(codeEdit->text().trimmed());
    c.setDescription(descEdit->text().trimmed()); c.setType(Coupon::stringToType(typeCombo->currentData().toString()));
    c.setValue(valueSpin->value()); c.setMaxUses(maxSpin->value()); c.setUsedCount(existing.usedCount());
    c.setActive(activeChk->isChecked()); if (expiryChk->isChecked()) c.setExpiryDate(expiryEdit->date());
    return c;
}

void CouponsPage::onAdd() {
    Coupon c = showCouponDialog({}, this); if (c.code().isEmpty()) return;
    if (!m_repo->save(c)) { QMessageBox::critical(this,"Error","Failed to save coupon."); return; }
    Logger::instance().info("Coupon created: "+c.code()); loadCoupons();
}
void CouponsPage::onEdit() {
    int row = m_table->currentRow(); if (row<0||row>=m_coupons.size()) return;
    Coupon updated = showCouponDialog(m_coupons.at(row), this); if (updated.code().isEmpty()) return;
    if (!m_repo->save(updated)) { QMessageBox::critical(this,"Error","Failed to update coupon."); return; }
    loadCoupons();
}
void CouponsPage::onDelete() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_coupons.size()) return;
    const Coupon& c = m_coupons.at(row);
    auto reply = QMessageBox::question(this, "Confirm",
        "Delete coupon \"" + c.code() + "\"?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    // Task 11: soft-delete
    QString archErr;
    int archiveId = ArchiveManager::instance().softDelete("Coupons", "Id", c.id(), &archErr);
    if (archiveId < 0) {
        QMessageBox::critical(this, "Error",
            "Failed to archive coupon before deletion:\n" + archErr);
        return;
    }
    AuditService::instance().logDelete("Coupon", c.id(),
        QString("{\"archiveId\":%1,\"code\":\"%2\"}").arg(archiveId).arg(c.code()));
    loadCoupons();
}
void CouponsPage::onSelectionChanged() {
    bool has = m_table->currentRow()>=0; m_editBtn->setEnabled(has); m_deleteBtn->setEnabled(has);
}

