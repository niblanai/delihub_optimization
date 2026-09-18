#ifndef USERS_PAGE_H
#define USERS_PAGE_H

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QPrinter>
#include <QPrintDialog>
#include "data/irepositories.h"
#include "ui/translatable_page.h"
#include "ui/id_card_renderer.h"

class UsersPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit UsersPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    // Users tab
    void onAddUser();
    void onEditUser();
    void onDeleteUser();
    void onUserSelectionChanged();

    // Roles tab
    void onAddRole();
    void onEditRole();
    void onDeleteRole();
    void onRoleSelectionChanged();

    // Drivers tab
    void onAddDriver();
    void onEditDriver();
    void onDeleteDriver();
    void onDriverSelectionChanged();

    // Attendance tab
    void onFilterAttendance();
    void onCalculateSalary();
    
    // Fingerprint cards tab
    void onUserCardSelectionChanged();
    void onPrintCard();
    void onTestCardScan();

private:
    void setupUi();
    void loadUsers();
    void loadRoles();
    void loadDrivers();
    void loadAttendanceRecords();
    void populateUserCombos();
    bool buildCardData(int userId, IdCardRenderer::CardData& out,
                       QString* error = nullptr) const;

    IUserRepository*             m_userRepo    = nullptr;
    IRoleRepository*             m_roleRepo    = nullptr;
    IDeliveryDriverRepository*   m_driverRepo  = nullptr;

    QList<Role>          m_roles;
    QList<User>          m_users;
    QMap<int,QString>    m_roleMap;

    // ── Users Tab ─────────────────────────────────────────────────────────────
    QTableWidget* m_usersTable     = nullptr;
    QPushButton*  m_addUserBtn     = nullptr;
    QPushButton*  m_editUserBtn    = nullptr;
    QPushButton*  m_deleteUserBtn  = nullptr;

    // ── Roles Tab ─────────────────────────────────────────────────────────────
    QTableWidget* m_rolesTable     = nullptr;
    QPushButton*  m_addRoleBtn     = nullptr;
    QPushButton*  m_editRoleBtn    = nullptr;
    QPushButton*  m_deleteRoleBtn  = nullptr;

    // ── Drivers Tab ───────────────────────────────────────────────────────────
    QTableWidget* m_driversTable   = nullptr;
    QPushButton*  m_addDriverBtn   = nullptr;
    QPushButton*  m_editDriverBtn  = nullptr;
    QPushButton*  m_deleteDriverBtn= nullptr;

    QList<DeliveryDriver> m_drivers;
    
    // ── Attendance Tab ────────────────────────────────────────────────────────
    QTableWidget* m_attendanceTable = nullptr;
    QComboBox*    m_attendanceUserCombo = nullptr;
    QDateEdit*    m_attendanceDateFromEdit = nullptr;
    QDateEdit*    m_attendanceDateToEdit = nullptr;
    
    // ── Fingerprint Cards Tab ─────────────────────────────────────────────────
    QComboBox*    m_selectUserForCardCombo = nullptr;
    QLabel*       m_cardPreviewLabel = nullptr;
    QLineEdit*    m_cardTestEdit = nullptr;
    QLabel*       m_cardTestResultLabel = nullptr;
};

#endif // USERS_PAGE_H
