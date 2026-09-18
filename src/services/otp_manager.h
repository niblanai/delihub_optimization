#ifndef OTP_MANAGER_H
#define OTP_MANAGER_H

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QMutex>

struct OtpData {
    QString code;
    QDateTime createdAt;
    QDateTime expiresAt;
    int attemptCount = 0;
    bool isValid = true;
};

class OtpManager {
public:
    static OtpManager& instance();
    
    // Generate a new 6-digit OTP for an email
    // Returns the OTP code
    QString generateOtp(const QString& email);
    
    // Verify an OTP for an email
    // Returns true if valid, false otherwise
    // Increments attempt count and invalidates after 5 attempts
    bool verifyOtp(const QString& email, const QString& code);
    
    // Check if an OTP exists and is not expired
    bool hasValidOtp(const QString& email) const;
    
    // Invalidate an OTP (after successful password reset)
    void invalidateOtp(const QString& email);
    
    // Get remaining attempts for an email
    int getRemainingAttempts(const QString& email) const;
    
    // Get expiration time for an email's OTP
    QDateTime getExpirationTime(const QString& email) const;
    
    // Clean up expired OTPs (called periodically)
    void cleanupExpired();
    
private:
    OtpManager() = default;
    ~OtpManager() = default;
    OtpManager(const OtpManager&) = delete;
    OtpManager& operator=(const OtpManager&) = delete;
    
    QString generateSecureOtp() const;
    bool isExpired(const OtpData& data) const;
    
    mutable QMutex m_mutex;
    QMap<QString, OtpData> m_otps;  // email -> OTP data
    
    static constexpr int MAX_ATTEMPTS = 5;
    static constexpr int OTP_LIFETIME_MINUTES = 10;
};

#endif // OTP_MANAGER_H
