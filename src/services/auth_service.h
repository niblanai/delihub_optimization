#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include "core/user.h"
#include "data/irepositories.h"
#include <QString>

class AuthService {
public:
    explicit AuthService(IUserRepository* userRepo);

    // Hash a plain-text password using PBKDF2-HMAC-SHA256; returns hex string
    static QString hashPassword(const QString& password, const QString& salt);

    // Generate a random hex salt
    static QString generateSalt();

    // Verify a password against stored hash + salt
    static bool verifyPassword(const QString& password,
                                const QString& storedHash,
                                const QString& salt);

    // Set a new password on a user (generates salt, hashes, stores both)
    void setPassword(User& user, const QString& plainPassword);

    // Authenticate by username + password; returns matched User (id==0 on failure)
    User authenticate(const QString& username, const QString& password);

private:
    IUserRepository* m_userRepo;
};

#endif // AUTH_SERVICE_H
