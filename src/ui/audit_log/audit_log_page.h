#ifndef AUDIT_LOG_PAGE_H
#define AUDIT_LOG_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class AuditLogPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit AuditLogPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

signals:
    // Emitted after a successful undo so MainWindow can refresh affected pages
    void dataRestored(const QString& entityType);

private slots:
    void onFilter();
    void onUndoEntry();
    void onClearFilters();

private:
    void setupUi();
    void loadEntries();
    bool confirmUndoAuth(const AuditLogEntry& entry);  // dual-auth dialog
    bool performUndo(const AuditLogEntry& entry);      // execute the undo

    IAuditLogRepository* m_auditRepo   = nullptr;
    IOrderRepository*    m_orderRepo   = nullptr;
    ICustomerRepository* m_customerRepo = nullptr;
    IProductRepository*  m_productRepo = nullptr;
    IUserRepository*     m_userRepo    = nullptr;
    IRoleRepository*     m_roleRepo    = nullptr;

    // Filters
    QComboBox*  m_userFilterCombo    = nullptr;
    QComboBox*  m_actionFilterCombo  = nullptr;
    QComboBox*  m_entityFilterCombo  = nullptr;
    QDateEdit*  m_fromDateEdit       = nullptr;
    QDateEdit*  m_toDateEdit         = nullptr;
    QLineEdit*  m_searchEdit         = nullptr;

    // Table
    QTableWidget* m_table            = nullptr;
    QLabel*       m_statusLabel      = nullptr;

    // Buttons
    QPushButton*  m_undoBtn          = nullptr;
    QPushButton*  m_refreshBtn       = nullptr;
    QPushButton*  m_filterBtn        = nullptr;
    QPushButton*  m_clearFilterBtn   = nullptr;

    QList<AuditLogEntry> m_entries;  // currently displayed entries
};

#endif // AUDIT_LOG_PAGE_H
