#include "otp_manager.h"
#include "infra/logger.h"
#include <QRandomGenerator>
#include <QMutexLocker>

OtpManager& OtpManager::instance() {
    static OtpManager inst;
    return inst;
}

QString OtpManager::generateSecureOtp() const {
    // Generate cryptographically secure 6-digit OTP
    quint32 otp = QRandomGenerator::securelySeeded().bounded(0, 1000000);
    return QString("%1").arg(otp, 6, 10, QChar('0'));
}

QString OtpManager::generateOtp(const QString& email) {
    QMutexLocker locker(&m_mutex);
    
    // Clean up any existing OTP for this email
    if (m_otps.contains(email)) {
        m_otps.remove(email);
    }
    
    // Generate new OTP
    OtpData data;
    data.code = generateSecureOtp();
    data.createdAt = QDateTime::currentDateTime();
    data.expiresAt = data.createdAt.addSecs(OTP_LIFETIME_MINUTES * 60);
    data.attemptCount = 0;
    data.isValid = true;
    
    m_otps[email] = data;
    
    Logger::instance().info(QString("OTP generated for email (expires in %1 min)")
                            .arg(OTP_LIFETIME_MINUTES));
    
    // Note: We do NOT log the actual OTP code for security
    return data.code;
}

bool OtpManager::verifyOtp(const QString& email, const QString& code) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_otps.contains(email)) {
        Logger::instance().warn("OTP verification failed: no OTP found for email");
        return false;
    }
    
    OtpData& data = m_otps[email];
    
    // Check if already invalid
    if (!data.isValid) {
        Logger::instance().warn("OTP verification failed: OTP already invalidated");
        return false;
    }
    
    // Check if expired
    if (isExpired(data)) {
        Logger::instance().warn("OTP verification failed: OTP expired");
        data.isValid = false;
        return false;
    }
    
    // Increment attempt count
    data.attemptCount++;
    
    // Check if too many attempts
    if (data.attemptCount > MAX_ATTEMPTS) {
        Logger::instance().warn(QString("OTP verification failed: too many attempts (%1)")
                                .arg(data.attemptCount));
        data.isValid = false;
        return false;
    }
    
    // Verify code
    if (data.code != code) {
        Logger::instance().warn(QString("OTP verification failed: incorrect code (attempt %1/%2)")
                                .arg(data.attemptCount).arg(MAX_ATTEMPTS));
        return false;
    }
    
    // Success
    Logger::instance().info("OTP verified successfully");
    return true;
}

bool OtpManager::hasValidOtp(const QString& email) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_otps.contains(email)) {
        return false;
    }
    
    const OtpData& data = m_otps.value(email);
    return data.isValid && !isExpired(data);
}

void OtpManager::invalidateOtp(const QString& email) {
    QMutexLocker locker(&m_mutex);
    
    if (m_otps.contains(email)) {
        m_otps[email].isValid = false;
        Logger::instance().info("OTP invalidated for email");
    }
}

int OtpManager::getRemainingAttempts(const QString& email) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_otps.contains(email)) {
        return MAX_ATTEMPTS;
    }
    
    const OtpData& data = m_otps.value(email);
    return MAX_ATTEMPTS - data.attemptCount;
}

QDateTime OtpManager::getExpirationTime(const QString& email) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_otps.contains(email)) {
        return QDateTime();
    }
    
    return m_otps.value(email).expiresAt;
}

void OtpManager::cleanupExpired() {
    QMutexLocker locker(&m_mutex);
    
    QList<QString> toRemove;
    for (auto it = m_otps.begin(); it != m_otps.end(); ++it) {
        if (isExpired(it.value())) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& email : toRemove) {
        m_otps.remove(email);
    }
    
    if (!toRemove.isEmpty()) {
        Logger::instance().info(QString("Cleaned up %1 expired OTPs").arg(toRemove.size()));
    }
}

bool OtpManager::isExpired(const OtpData& data) const {
    return QDateTime::currentDateTime() > data.expiresAt;
}
