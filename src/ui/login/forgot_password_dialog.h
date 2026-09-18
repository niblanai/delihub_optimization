#ifndef FORGOT_PASSWORD_DIALOG_H
#define FORGOT_PASSWORD_DIALOG_H

#include <QDialog>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "data/irepositories.h"

class QNetworkAccessManager;

class ForgotPasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit ForgotPasswordDialog(IUserRepository* userRepo, QWidget* parent = nullptr);

private slots:
    // Step 1: Email input
    void onRequestOtp();
    
    // Step 2: OTP verification
    void onVerifyOtp();
    void onResendOtp();
    void updateTimer();
    
    // Step 3: Password reset
    void onResetPassword();

private:
    void setupUi();
    void showStep(int step);
    bool validateEmail(const QString& email) const;
    bool validatePassword(const QString& password) const;
    void startTimer();
    void stopTimer();

    IUserRepository* m_userRepo;
    QNetworkAccessManager* m_networkManager;
    
    // UI Components
    QStackedWidget* m_stackedWidget;
    
    // Step 1: Email
    QLineEdit* m_emailEdit;
    QPushButton* m_requestBtn;
    QLabel* m_emailStatusLabel;
    
    // Step 2: OTP
    QLineEdit* m_otpEdit;
    QPushButton* m_verifyBtn;
    QPushButton* m_resendBtn;
    QLabel* m_otpStatusLabel;
    QLabel* m_timerLabel;
    QLabel* m_attemptsLabel;
    QTimer* m_timer;
    QDateTime m_expirationTime;
    
    // Step 3: New Password
    QLineEdit* m_newPasswordEdit;
    QLineEdit* m_confirmPasswordEdit;
    QPushButton* m_resetBtn;
    QLabel* m_passwordStatusLabel;
    
    // Data
    QString m_email;
    int m_userId;
    int m_remainingAttempts;
    
    // Rate limiting
    QDateTime m_lastOtpRequestTime;
    static constexpr int COOLDOWN_SECONDS = 60;
};

#endif // FORGOT_PASSWORD_DIALOG_H
