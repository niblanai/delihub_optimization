#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "ui/translatable_page.h"
#include "ui/settings/receipt_designer_widget.h"

class SettingsPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onSaveConfig();
    void onBackup();
    void onRestore();
    void onAddBranch();
    void onEditBranch();
    void onDeleteBranch();

private:
    void setupUi();
    void loadCurrentSettings();
    void loadBranches();

    // ── Company / Invoice ─────────────────────────────────────────────────────
    QLineEdit*   m_companyNameEdit    = nullptr;
    QLineEdit*   m_companyAddrEdit    = nullptr;
    QLineEdit*   m_companyPhoneEdit   = nullptr;
    QLineEdit*   m_companyTaxEdit     = nullptr;
    QLineEdit*   m_companyLogoEdit    = nullptr;
    QCheckBox*   m_requireAuthForPOSDeletionCheckbox = nullptr;

    // ── Database tab ──────────────────────────────────────────────────────────
    QComboBox*   m_dbTypeCombo    = nullptr;
    QLineEdit*   m_odbcEdit       = nullptr;
    QLineEdit*   m_sqlitePathEdit = nullptr;

    // ── Branch tab ────────────────────────────────────────────────────────────
    QLineEdit*      m_branchNameEdit = nullptr;
    QLineEdit*      m_currencyEdit   = nullptr;
    QDoubleSpinBox* m_taxRateSpin    = nullptr;

    // ── Branches management tab ───────────────────────────────────────────────
    QTableWidget* m_branchesTable   = nullptr;
    QPushButton*  m_addBranchBtn    = nullptr;
    QPushButton*  m_editBranchBtn   = nullptr;
    QPushButton*  m_deleteBranchBtn = nullptr;

    // ── Backup/Restore tab ────────────────────────────────────────────────────
    QPushButton* m_backupBtn     = nullptr;
    QPushButton* m_restoreBtn    = nullptr;
    QLabel*      m_backupStatus  = nullptr;

    // ── Reminders tab ─────────────────────────────────────────────────────────
    QCheckBox*   m_remindEnabledChk    = nullptr;
    QSpinBox*    m_remindMinutesBefore = nullptr;
    QSpinBox*    m_remindIntervalMin   = nullptr;

    // ── Notes board background ────────────────────────────────────────────────
    QComboBox*   m_notesBgModeCombo    = nullptr;
    QLineEdit*   m_notesBgImageEdit    = nullptr;
    QPushButton* m_notesBgBrowseBtn    = nullptr;

    QPushButton* m_saveBtn       = nullptr;

    // ── Receipt Designer tab ─────────────────────────────────────────────────
    ReceiptDesignerWidget* m_receiptDesigner = nullptr;
};

#endif // SETTINGS_PAGE_H
