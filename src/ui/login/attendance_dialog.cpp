#include "attendance_dialog.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QCoreApplication>
#include <QFile>

AttendanceDialog::AttendanceDialog(IUserRepository* userRepo, IAttendanceRepository* attendanceRepo, QWidget* parent)
    : QDialog(parent), m_userRepo(userRepo), m_attendanceRepo(attendanceRepo)
{
    setWindowTitle(LangManager::instance().t("تسجيل الحضور والانصراف"));
    setModal(true);
    resize(500, 400);
    setupUi();
}

void AttendanceDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(20);
    layout->setContentsMargins(40, 40, 40, 40);
    
    // Title
    m_titleLbl = new QLabel(LangManager::instance().t("امسح البصمة الخاصة بك"), this);
    m_titleLbl->setAlignment(Qt::AlignCenter);
    m_titleLbl->setStyleSheet("font-size: 18px; font-weight: bold; color: #1F2937;");
    layout->addWidget(m_titleLbl);
    
    // Animation area - simple text icon since SVG widget complex
    m_animationLbl = new QLabel("📱", this);
    m_animationLbl->setAlignment(Qt::AlignCenter);
    m_animationLbl->setMinimumHeight(150);
    m_animationLbl->setStyleSheet("font-size: 80px;");
    layout->addWidget(m_animationLbl);
    
    auto* instructionLbl = new QLabel(LangManager::instance().t("امسح باركود البصمة"), this);
    instructionLbl->setAlignment(Qt::AlignCenter);
    instructionLbl->setStyleSheet("font-size: 14px; color: #6B7280;");
    layout->addWidget(instructionLbl);
    
    // Hidden barcode input (for barcode scanner input)
    m_barcodeEdit = new QLineEdit(this);
    m_barcodeEdit->setPlaceholderText(LangManager::instance().t("امسح البصمة أو اضغط 'إدخال كود البصمة'"));
    m_barcodeEdit->setStyleSheet("font-size: 14px; padding: 12px; border: 2px solid #D1D5DB; border-radius: 8px;");
    connect(m_barcodeEdit, &QLineEdit::returnPressed, this, &AttendanceDialog::onBarcodeEntered);
    layout->addWidget(m_barcodeEdit);
    
    // Manual entry button (small, text-only)
    m_manualBtn = new QPushButton(LangManager::instance().t("إدخال كود البصمة"), this);
    m_manualBtn->setStyleSheet(
        "QPushButton { "
        "  background: transparent; "
        "  border: none; "
        "  color: #3B82F6; "
        "  font-size: 12px; "
        "  text-decoration: underline; "
        "  padding: 5px; "
        "}"
        "QPushButton:hover { color: #2563EB; }"
    );
    m_manualBtn->setCursor(Qt::PointingHandCursor);
    connect(m_manualBtn, &QPushButton::clicked, this, &AttendanceDialog::onManualEntry);
    
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_manualBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    layout->addStretch();
    
    // Focus on barcode input
    m_barcodeEdit->setFocus();
}

void AttendanceDialog::onBarcodeEntered() {
    QString barcode = m_barcodeEdit->text().trimmed();
    if (barcode.isEmpty()) return;
    
    processBarcode(barcode);
    m_barcodeEdit->clear();
}

void AttendanceDialog::onManualEntry() {
    bool ok;
    QString barcode = QInputDialog::getText(this,
        LangManager::instance().t("إدخال كود البصمة"),
        LangManager::instance().t("أدخل كود البصمة:"),
        QLineEdit::Normal, "", &ok);
    
    if (ok && !barcode.trimmed().isEmpty()) {
        processBarcode(barcode.trimmed());
    }
}

void AttendanceDialog::processBarcode(const QString& barcode) {
    // Find user by fingerprint barcode
    User foundUser;
    QList<User> allUsers = m_userRepo->getAll();
    
    for (const auto& user : allUsers) {
        if (user.fingerprintBarcode() == barcode) {
            foundUser = user;
            break;
        }
    }
    
    if (foundUser.id() == 0) {
        QMessageBox::warning(this,
            LangManager::instance().t("غير موجود"),
            LangManager::instance().t("لم يتم العثور على مستخدم بهذه البصمة"));
        return;
    }
    
    // Get last attendance record
    Attendance lastAttendance = m_attendanceRepo->getLastByUserId(foundUser.id());
    
    // Determine if this is check-in or check-out
    Attendance::Type newType = Attendance::CheckIn;
    
    if (lastAttendance.id() > 0) {
        // If last was check-in, this is check-out
        if (lastAttendance.type() == Attendance::CheckIn) {
            newType = Attendance::CheckOut;
        }
    }
    
    // Create new attendance record
    Attendance newAttendance;
    newAttendance.setUserId(foundUser.id());
    newAttendance.setUserName(foundUser.name());
    newAttendance.setTimestamp(QDateTime::currentDateTime());
    newAttendance.setType(newType);
    
    if (m_attendanceRepo->save(newAttendance)) {
        showSuccessMessage(foundUser, newType, newAttendance.timestamp());
    } else {
        QMessageBox::critical(this,
            LangManager::instance().t("خطأ"),
            LangManager::instance().t("فشل تسجيل الحضور"));
    }
}

void AttendanceDialog::showSuccessMessage(const User& user, Attendance::Type type, const QDateTime& timestamp) {
    QString typeText = (type == Attendance::CheckIn) 
        ? LangManager::instance().t("تسجيل دخول")
        : LangManager::instance().t("تسجيل خروج");
    
    QString message = QString("%1: %2\n%3: %4\n%5: %6")
        .arg(LangManager::instance().t("المستخدم"))
        .arg(user.name())
        .arg(LangManager::instance().t("العملية"))
        .arg(typeText)
        .arg(LangManager::instance().t("الوقت"))
        .arg(timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    
    // Create temporary dialog that auto-closes after 4 seconds
    QDialog* successDlg = new QDialog(this);
    successDlg->setWindowTitle(LangManager::instance().t("نجح"));
    successDlg->setModal(true);
    successDlg->resize(400, 200);
    
    auto* dlgLayout = new QVBoxLayout(successDlg);
    dlgLayout->setSpacing(20);
    dlgLayout->setContentsMargins(40, 40, 40, 40);
    
    // Success icon
    auto* iconLbl = new QLabel("✅", successDlg);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet("font-size: 48px;");
    dlgLayout->addWidget(iconLbl);
    
    // Message
    auto* msgLbl = new QLabel(message, successDlg);
    msgLbl->setAlignment(Qt::AlignCenter);
    msgLbl->setStyleSheet("font-size: 14px; color: #1F2937;");
    msgLbl->setWordWrap(true);
    dlgLayout->addWidget(msgLbl);
    
    successDlg->show();
    
    // Auto-close after 4 seconds
    QTimer::singleShot(4000, successDlg, &QDialog::accept);
    QTimer::singleShot(4000, successDlg, &QDialog::deleteLater);
}
