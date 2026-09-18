#include "forgot_password_dialog.h"
#include "services/auth_service.h"
#include "services/lang_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

ForgotPasswordDialog::ForgotPasswordDialog(IUserRepository* userRepo, QWidget* parent)
    : QDialog(parent)
    , m_userRepo(userRepo)
    , m_userId(0)
    , m_networkManager(new QNetworkAccessManager(this))
{
    setupUi();
    showStep(0);
}

void ForgotPasswordDialog::setupUi() {
    setWindowTitle(LangManager::tr("forgot_password_title"));
    setMinimumWidth(450);
    setModal(true);
    
    auto* mainLayout = new QVBoxLayout(this);
    m_stackedWidget = new QStackedWidget(this);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Step 1: Email Input
    // ═══════════════════════════════════════════════════════════════════════════
    auto* step1Widget = new QWidget();
    auto* step1Layout = new QVBoxLayout(step1Widget);
    
    auto* step1Title = new QLabel(LangManager::tr("enter_your_email"));
    step1Title->setStyleSheet("font-size: 16px; font-weight: bold; margin-bottom: 10px;");
    step1Layout->addWidget(step1Title);
    
    auto* step1Desc = new QLabel(LangManager::tr("email_recovery_desc"));
    step1Desc->setWordWrap(true);
    step1Desc->setStyleSheet("color: #64748b; margin-bottom: 20px;");
    step1Layout->addWidget(step1Desc);
    
    auto* emailForm = new QFormLayout();
    m_emailEdit = new QLineEdit();
    m_emailEdit->setPlaceholderText("example@email.com");
    emailForm->addRow(LangManager::tr("email_address"), m_emailEdit);
    step1Layout->addLayout(emailForm);
    
    m_emailStatusLabel = new QLabel();
    m_emailStatusLabel->setWordWrap(true);
    m_emailStatusLabel->setStyleSheet("color: #dc2626; margin: 10px 0;");
    m_emailStatusLabel->setVisible(false);
    step1Layout->addWidget(m_emailStatusLabel);
    
    m_requestBtn = new QPushButton(LangManager::tr("send_verification_code"));
    m_requestBtn->setMinimumHeight(40);
    connect(m_requestBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onRequestOtp);
    step1Layout->addWidget(m_requestBtn);
    
    step1Layout->addStretch();
    m_stackedWidget->addWidget(step1Widget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Step 2: OTP Verification
    // ═══════════════════════════════════════════════════════════════════════════
    auto* step2Widget = new QWidget();
    auto* step2Layout = new QVBoxLayout(step2Widget);
    
    auto* step2Title = new QLabel(LangManager::tr("enter_verification_code"));
    step2Title->setStyleSheet("font-size: 16px; font-weight: bold; margin-bottom: 10px;");
    step2Layout->addWidget(step2Title);
    
    auto* step2Desc = new QLabel(LangManager::tr("otp_sent_desc"));
    step2Desc->setWordWrap(true);
    step2Desc->setStyleSheet("color: #64748b; margin-bottom: 20px;");
    step2Layout->addWidget(step2Desc);
    
    auto* otpForm = new QFormLayout();
    m_otpEdit = new QLineEdit();
    m_otpEdit->setPlaceholderText("000000");
    m_otpEdit->setMaxLength(6);
    m_otpEdit->setStyleSheet("font-size: 24px; letter-spacing: 8px; text-align: center; font-family: monospace;");
    otpForm->addRow(LangManager::tr("verification_code"), m_otpEdit);
    step2Layout->addLayout(otpForm);
    
    m_timerLabel = new QLabel();
    m_timerLabel->setStyleSheet("color: #f59e0b; font-weight: bold; margin: 10px 0;");
    m_timerLabel->setAlignment(Qt::AlignCenter);
    step2Layout->addWidget(m_timerLabel);
    
    m_attemptsLabel = new QLabel();
    m_attemptsLabel->setStyleSheet("color: #64748b; margin: 5px 0;");
    m_attemptsLabel->setAlignment(Qt::AlignCenter);
    step2Layout->addWidget(m_attemptsLabel);
    
    m_otpStatusLabel = new QLabel();
    m_otpStatusLabel->setWordWrap(true);
    m_otpStatusLabel->setStyleSheet("color: #dc2626; margin: 10px 0;");
    m_otpStatusLabel->setVisible(false);
    step2Layout->addWidget(m_otpStatusLabel);
    
    m_verifyBtn = new QPushButton(LangManager::tr("verify_code"));
    m_verifyBtn->setMinimumHeight(40);
    connect(m_verifyBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onVerifyOtp);
    step2Layout->addWidget(m_verifyBtn);
    
    m_resendBtn = new QPushButton(LangManager::tr("resend_code"));
    m_resendBtn->setStyleSheet("background: transparent; border: none; color: #3b82f6; text-decoration: underline;");
    connect(m_resendBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onResendOtp);
    step2Layout->addWidget(m_resendBtn);
    
    step2Layout->addStretch();
    m_stackedWidget->addWidget(step2Widget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Step 3: New Password
    // ═══════════════════════════════════════════════════════════════════════════
    auto* step3Widget = new QWidget();
    auto* step3Layout = new QVBoxLayout(step3Widget);
    
    auto* step3Title = new QLabel(LangManager::tr("set_new_password"));
    step3Title->setStyleSheet("font-size: 16px; font-weight: bold; margin-bottom: 10px;");
    step3Layout->addWidget(step3Title);
    
    auto* step3Desc = new QLabel(LangManager::tr("new_password_desc"));
    step3Desc->setWordWrap(true);
    step3Desc->setStyleSheet("color: #64748b; margin-bottom: 20px;");
    step3Layout->addWidget(step3Desc);
    
    auto* passwordForm = new QFormLayout();
    m_newPasswordEdit = new QLineEdit();
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setPlaceholderText(LangManager::tr("min_8_chars"));
    passwordForm->addRow(LangManager::tr("new_password"), m_newPasswordEdit);
    
    m_confirmPasswordEdit = new QLineEdit();
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(LangManager::tr("retype_password"));
    passwordForm->addRow(LangManager::tr("confirm_password"), m_confirmPasswordEdit);
    step3Layout->addLayout(passwordForm);
    
    m_passwordStatusLabel = new QLabel();
    m_passwordStatusLabel->setWordWrap(true);
    m_passwordStatusLabel->setStyleSheet("color: #dc2626; margin: 10px 0;");
    m_passwordStatusLabel->setVisible(false);
    step3Layout->addWidget(m_passwordStatusLabel);
    
    m_resetBtn = new QPushButton(LangManager::tr("reset_password"));
    m_resetBtn->setMinimumHeight(40);
    connect(m_resetBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onResetPassword);
    step3Layout->addWidget(m_resetBtn);
    
    step3Layout->addStretch();
    m_stackedWidget->addWidget(step3Widget);
    
    mainLayout->addWidget(m_stackedWidget);
    
    // Timer for OTP expiration
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ForgotPasswordDialog::updateTimer);
}

void ForgotPasswordDialog::showStep(int step) {
    m_stackedWidget->setCurrentIndex(step);
    
    // Reset status labels
    m_emailStatusLabel->setVisible(false);
    m_otpStatusLabel->setVisible(false);
    m_passwordStatusLabel->setVisible(false);
}

bool ForgotPasswordDialog::validateEmail(const QString& email) const {
    QRegularExpression emailRegex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return emailRegex.match(email).hasMatch();
}

bool ForgotPasswordDialog::validatePassword(const QString& password) const {
    return password.length() >= 8;
}

void ForgotPasswordDialog::onRequestOtp() {
    m_email = m_emailEdit->text().trimmed();
    
    // Validate email format
    if (m_email.isEmpty() || !validateEmail(m_email)) {
        m_emailStatusLabel->setText(LangManager::tr("invalid_email_format"));
        m_emailStatusLabel->setVisible(true);
        return;
    }
    
    // Rate limiting check
    if (m_lastOtpRequestTime.isValid()) {
        qint64 elapsed = m_lastOtpRequestTime.secsTo(QDateTime::currentDateTime());
        if (elapsed < COOLDOWN_SECONDS) {
            int remaining = COOLDOWN_SECONDS - elapsed;
            m_emailStatusLabel->setText(
                LangManager::tr("please_wait_seconds").arg(remaining));
            m_emailStatusLabel->setVisible(true);
            return;
        }
    }
    
    // Check if user exists
    User user = m_userRepo->getByEmail(m_email);
    if (user.id() > 0) {
        m_userId = user.id();
    } else {
        // For security: show generic error without revealing if email exists
        m_emailStatusLabel->setText(LangManager::tr("invalid_email_format"));
        m_emailStatusLabel->setVisible(true);
        return;
    }
    
    // Disable button during processing
    m_requestBtn->setEnabled(false);
    m_requestBtn->setText(LangManager::tr("sending"));
    
    // Send OTP request to Cloudflare Worker
    QUrl url("https://erp-password-recovery.hosamwork2003.workers.dev/request-reset");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject json;
    json["email"] = m_email;
    
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_requestBtn->setEnabled(true);
        m_requestBtn->setText(LangManager::tr("send_verification_code"));
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject obj = doc.object();
            
            if (obj["success"].toBool()) {
                // Success - move to OTP verification step
                QMessageBox::information(this, 
                    LangManager::tr("success"),
                    LangManager::tr("otp_sent_generic_message"));
                
                // Calculate expiration time (10 minutes from now)
                m_expirationTime = QDateTime::currentDateTime().addSecs(600);
                m_remainingAttempts = 5;
                
                showStep(1);
                startTimer();
                
                m_attemptsLabel->setText(
                    LangManager::tr("remaining_attempts").arg(m_remainingAttempts));
                
                m_lastOtpRequestTime = QDateTime::currentDateTime();
            } else {
                // Error from worker
                QString errorMsg = obj["error"].toString();
                m_emailStatusLabel->setText(errorMsg);
                m_emailStatusLabel->setVisible(true);
                Logger::instance().error("Worker error: " + errorMsg);
            }
        } else {
            // Network error
            QString errorMsg = reply->errorString();
            m_emailStatusLabel->setText(LangManager::tr("network_error"));
            m_emailStatusLabel->setVisible(true);
            Logger::instance().error("Network error: " + errorMsg);
        }
        
        reply->deleteLater();
    });
}


void ForgotPasswordDialog::onVerifyOtp() {
    QString enteredOtp = m_otpEdit->text().trimmed();
    
    if (enteredOtp.length() != 6) {
        m_otpStatusLabel->setText(LangManager::tr("otp_must_be_6_digits"));
        m_otpStatusLabel->setVisible(true);
        return;
    }
    
    // Disable button during processing
    m_verifyBtn->setEnabled(false);
    m_verifyBtn->setText(LangManager::tr("verifying"));
    
    // Send OTP verification request to Cloudflare Worker
    QUrl url("https://erp-password-recovery.hosamwork2003.workers.dev/verify-otp");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject json;
    json["email"] = m_email;
    json["otp"] = enteredOtp;
    
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_verifyBtn->setEnabled(true);
        m_verifyBtn->setText(LangManager::tr("verify_code"));
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject obj = doc.object();
            
            if (obj["success"].toBool()) {
                // OTP is correct
                stopTimer();
                showStep(2);  // Move to password reset step
            } else {
                // OTP is incorrect
                QString errorMsg = obj["error"].toString();
                int remaining = obj["remainingAttempts"].toInt();
                
                m_remainingAttempts = remaining;
                
                if (remaining > 0) {
                    m_otpStatusLabel->setText(
                        LangManager::tr("incorrect_otp_remaining").arg(remaining));
                    m_attemptsLabel->setText(
                        LangManager::tr("remaining_attempts").arg(remaining));
                } else {
                    m_otpStatusLabel->setText(LangManager::tr("otp_attempts_exhausted"));
                    m_verifyBtn->setEnabled(false);
                    stopTimer();
                }
                
                m_otpStatusLabel->setVisible(true);
                m_otpEdit->clear();
                m_otpEdit->setFocus();
            }
        } else {
            // Network error
            QString errorMsg = reply->errorString();
            m_otpStatusLabel->setText(LangManager::tr("network_error"));
            m_otpStatusLabel->setVisible(true);
            Logger::instance().error("Network error: " + errorMsg);
        }
        
        reply->deleteLater();
    });
}

void ForgotPasswordDialog::onResendOtp() {
    // Check cooldown
    if (m_lastOtpRequestTime.isValid()) {
        qint64 elapsed = m_lastOtpRequestTime.secsTo(QDateTime::currentDateTime());
        if (elapsed < COOLDOWN_SECONDS) {
            int remaining = COOLDOWN_SECONDS - elapsed;
            QMessageBox::warning(this, 
                LangManager::tr("please_wait"),
                LangManager::tr("please_wait_seconds").arg(remaining));
            return;
        }
    }
    
    // Go back to step 1
    showStep(0);
    stopTimer();
}

void ForgotPasswordDialog::onResetPassword() {
    QString newPassword = m_newPasswordEdit->text();
    QString confirmPassword = m_confirmPasswordEdit->text();
    
    // Validate password
    if (newPassword.isEmpty()) {
        m_passwordStatusLabel->setText(LangManager::tr("password_required"));
        m_passwordStatusLabel->setVisible(true);
        return;
    }
    
    if (!validatePassword(newPassword)) {
        m_passwordStatusLabel->setText(LangManager::tr("password_min_8_chars"));
        m_passwordStatusLabel->setVisible(true);
        return;
    }
    
    if (newPassword != confirmPassword) {
        m_passwordStatusLabel->setText(LangManager::tr("passwords_dont_match"));
        m_passwordStatusLabel->setVisible(true);
        return;
    }
    
    // Check if we have a valid user ID
    if (m_userId <= 0) {
        m_passwordStatusLabel->setText(LangManager::tr("password_reset_failed"));
        m_passwordStatusLabel->setVisible(true);
        Logger::instance().error("Password reset failed: invalid user ID");
        return;
    }
    
    // Get user and update password
    User user = m_userRepo->getById(m_userId);
    if (user.id() <= 0) {
        m_passwordStatusLabel->setText(LangManager::tr("password_reset_failed"));
        m_passwordStatusLabel->setVisible(true);
        Logger::instance().error("Password reset failed: user not found");
        return;
    }
    
    // Hash new password
    QString salt = AuthService::generateSalt();
    QString hash = AuthService::hashPassword(newPassword, salt);
    
    user.setPasswordHash(hash);
    user.setPasswordSalt(salt);
    
    // Save updated user
    if (m_userRepo->save(user)) {
        QMessageBox::information(this, 
            LangManager::tr("success"),
            LangManager::tr("password_reset_success"));
        
        accept();
    } else {
        m_passwordStatusLabel->setText(LangManager::tr("password_reset_failed"));
        m_passwordStatusLabel->setVisible(true);
        Logger::instance().error("Failed to save new password");
    }
}

void ForgotPasswordDialog::startTimer() {
    m_timer->start(1000);  // Update every second
    updateTimer();
}

void ForgotPasswordDialog::stopTimer() {
    m_timer->stop();
}

void ForgotPasswordDialog::updateTimer() {
    if (!m_expirationTime.isValid()) {
        return;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 remaining = now.secsTo(m_expirationTime);
    
    if (remaining <= 0) {
        m_timerLabel->setText(LangManager::tr("otp_expired"));
        m_timerLabel->setStyleSheet("color: #dc2626; font-weight: bold;");
        m_verifyBtn->setEnabled(false);
        stopTimer();
    } else {
        int minutes = remaining / 60;
        int seconds = remaining % 60;
        m_timerLabel->setText(
            LangManager::tr("time_remaining").arg(minutes).arg(seconds, 2, 10, QChar('0')));
    }
}
