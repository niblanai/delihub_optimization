#ifndef BREVO_EMAIL_SERVICE_H
#define BREVO_EMAIL_SERVICE_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class BrevoEmailService : public QObject {
    Q_OBJECT
public:
    explicit BrevoEmailService(QObject* parent = nullptr);
    
    // Send OTP code for password recovery
    void sendPasswordRecoveryOtp(const QString& toEmail, const QString& otpCode);
    
    // Configure Brevo credentials
    void setApiKey(const QString& apiKey);
    void setSenderEmail(const QString& email);
    void setSenderName(const QString& name);

signals:
    void emailSent(bool success, const QString& message);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QString m_apiKey;
    QString m_senderEmail;
    QString m_senderName;
};

#endif // BREVO_EMAIL_SERVICE_H
