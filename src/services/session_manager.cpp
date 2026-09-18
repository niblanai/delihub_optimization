#include "session_manager.h"

SessionManager& SessionManager::instance() {
    static SessionManager inst;
    return inst;
}

void SessionManager::login(const User& user, const Role& role) {
    m_user      = user;
    m_role      = role;
    m_loggedIn  = true;
}

void SessionManager::logout() {
    m_user      = User{};
    m_role      = Role{};
    m_loggedIn  = false;
}

bool SessionManager::isLoggedIn() const { return m_loggedIn; }

const User& SessionManager::currentUser() const { return m_user; }
const Role& SessionManager::currentRole() const { return m_role; }

bool SessionManager::isAdmin() const {
    return m_loggedIn && m_role.canManageUsers;
}
