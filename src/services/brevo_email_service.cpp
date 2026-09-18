#include "brevo_email_service.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

BrevoEmailService::BrevoEmailService(QObject* parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &BrevoEmailService::onReplyFinished);
    
    // Load credentials from config
    auto& config = ConfigManager::instance();
    m_apiKey = config.brevoApiKey();
    m_senderEmail = config.brevoSenderEmail();
    m_senderName = config.brevoSenderName();
}

void BrevoEmailService::setApiKey(const QString& apiKey) {
    m_apiKey = apiKey;
    ConfigManager::instance().setBrevoApiKey(apiKey);
}

void BrevoEmailService::setSenderEmail(const QString& email) {
    m_senderEmail = email;
    ConfigManager::instance().setBrevoSenderEmail(email);
}

void BrevoEmailService::setSenderName(const QString& name) {
    m_senderName = name;
    ConfigManager::instance().setBrevoSenderName(name);
}

void BrevoEmailService::sendPasswordRecoveryOtp(const QString& toEmail, const QString& otpCode) {
    if (m_apiKey.isEmpty()) {
        emit emailSent(false, "Brevo API Key not configured");
        Logger::instance().error("Brevo API Key is missing");
        return;
    }
    
    if (m_senderEmail.isEmpty()) {
        emit emailSent(false, "Sender email not configured");
        Logger::instance().error("Brevo sender email is missing");
        return;
    }

    // Build JSON payload for Brevo API v3
    QJsonObject sender;
    sender["email"] = m_senderEmail;
    sender["name"] = m_senderName.isEmpty() ? "DeliHub" : m_senderName;

    QJsonObject recipient;
    recipient["email"] = toEmail;

    QJsonArray to;
    to.append(recipient);

    // Email content in Arabic (simplified, professional)
    QString htmlContent = QString(
        "<div style='direction: rtl; text-align: right; font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;'>"
        "<div style='background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); padding: 30px; text-align: center;'>"
        "<h1 style='color: white; margin: 0;'>رمز استعادة كلمة المرور</h1>"
        "</div>"
        "<div style='padding: 40px 30px; background: #ffffff;'>"
        "<p style='font-size: 16px; color: #333; line-height: 1.6;'>رمز التحقق الخاص بك هو:</p>"
        "<div style='background: #f8fafc; border: 2px dashed #cbd5e1; padding: 25px; text-align: center; "
        "margin: 25px 0; border-radius: 8px;'>"
        "<div style='font-size: 42px; font-weight: bold; color: #1e293b; letter-spacing: 12px; "
        "font-family: \"Courier New\", monospace;'>%1</div>"
        "</div>"
        "<p style='font-size: 14px; color: #ef4444; background: #fef2f2; padding: 15px; border-radius: 6px; "
        "border-right: 4px solid #ef4444;'>"
        "<strong>⚠ مهم:</strong> هذا الرمز صالح لمدة <strong>10 دقائق فقط</strong>."
        "</p>"
        "<p style='font-size: 14px; color: #64748b; line-height: 1.6;'>"
        "إذا لم تطلب استعادة كلمة المرور، يمكنك تجاهل هذه الرسالة بأمان."
        "</p>"
        "</div>"
        "<div style='background: #f8fafc; padding: 20px; text-align: center; color: #94a3b8; font-size: 12px;'>"
        "DeliHub - نظام إدارة التوصيل"
        "</div>"
        "</div>"
    ).arg(otpCode);

    QString textContent = QString(
        "رمز استعادة كلمة المرور\n\n"
        "رمز التحقق الخاص بك هو: %1\n\n"
        "هذا الرمز صالح لمدة 10 دقائق فقط.\n\n"
        "إذا لم تطلب استعادة كلمة المرور، يمكنك تجاهل هذه الرسالة."
    ).arg(otpCode);

    QJsonObject root;
    root["sender"] = sender;
    root["to"] = to;
    root["subject"] = "رمز استعادة كلمة المرور - DeliHub";
    root["htmlContent"] = htmlContent;
    root["textContent"] = textContent;

    QJsonDocument doc(root);
    QByteArray payload = doc.toJson();

    // Prepare HTTP request
    QNetworkRequest request(QUrl("https://api.brevo.com/v3/smtp/email"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("api-key", m_apiKey.toUtf8());
    request.setRawHeader("accept", "application/json");

    Logger::instance().info("Sending password recovery OTP via Brevo");
    // Note: We do NOT log the email address for user privacy
    m_networkManager->post(request, payload);
}

void BrevoEmailService::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        
        Logger::instance().info("Email sent successfully via Brevo");
        emit emailSent(true, "Email sent successfully");
    } else {
        QString errorMsg = reply->errorString();
        QByteArray errorData = reply->readAll();
        
        Logger::instance().error("Brevo API error: " + errorMsg + " | " + QString::fromUtf8(errorData));
        emit emailSent(false, "Failed to send email: " + errorMsg);
    }
}
