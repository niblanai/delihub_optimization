#include "auth_service.h"
#include "infra/logger.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QByteArray>

AuthService::AuthService(IUserRepository* userRepo)
    : m_userRepo(userRepo) {}

QString AuthService::generateSalt() {
    QByteArray salt(32, Qt::Uninitialized);
    for (int i = 0; i < 32; ++i)
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(salt.toHex());
}

// PBKDF2-HMAC-SHA256 implementation using Qt primitives
// Runs 100,000 iterations as recommended by NIST SP 800-132
QString AuthService::hashPassword(const QString& password, const QString& salt) {
    const int iterations = 100000;
    const int dkLen      = 32; // 256-bit output

    QByteArray pwd  = password.toUtf8();
    QByteArray saltBytes = salt.toLatin1(); // salt is hex, use as-is bytes

    // Initial U1 = HMAC-SHA256(password, salt || INT(1))
    QByteArray block = saltBytes;
    block.append('\x00'); block.append('\x00'); block.append('\x00'); block.append('\x01');

    QByteArray u = QMessageAuthenticationCode::hash(block, pwd, QCryptographicHash::Sha256);
    QByteArray result = u;

    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, pwd, QCryptographicHash::Sha256);
        for (int j = 0; j < result.size(); ++j)
            result[j] ^= u[j];
    }

    return QString::fromLatin1(result.left(dkLen).toHex());
}

bool AuthService::verifyPassword(const QString& password,
                                  const QString& storedHash,
                                  const QString& salt) {
    return hashPassword(password, salt) == storedHash;
}

void AuthService::setPassword(User& user, const QString& plainPassword) {
    QString salt = generateSalt();
    QString hash = hashPassword(plainPassword, salt);
    user.setPasswordSalt(salt);
    user.setPasswordHash(hash);
}

User AuthService::authenticate(const QString& username, const QString& password) {
    User u = m_userRepo->getByUsername(username);
    if (u.id() == 0) {
        Logger::instance().warn("Auth failed: unknown username " + username);
        return User{};
    }
    if (!verifyPassword(password, u.passwordHash(), u.passwordSalt())) {
        Logger::instance().warn("Auth failed: wrong password for " + username);
        return User{};
    }
    Logger::instance().info("Auth success: " + username);
    return u;
}
