#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include "core/user.h"
#include "core/role.h"

// Holds the currently logged-in user and their resolved role.
// Check permissions anywhere in the app via SessionManager::instance().role().canXxx
class SessionManager {
public:
    static SessionManager& instance();

    void    login(const User& user, const Role& role);
    void    logout();
    bool    isLoggedIn() const;

    const User& currentUser() const;
    const Role& currentRole() const;
    bool        isAdmin() const;   // convenience: canManageUsers == true

private:
    SessionManager() = default;
    User m_user;
    Role m_role;
    bool m_loggedIn = false;
};

#endif // SESSION_MANAGER_H
