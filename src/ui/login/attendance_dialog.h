#ifndef ATTENDANCE_DIALOG_H
#define ATTENDANCE_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "core/user.h"
#include "core/attendance.h"
#include "data/irepositories.h"

class AttendanceDialog : public QDialog {
    Q_OBJECT
public:
    explicit AttendanceDialog(IUserRepository* userRepo, IAttendanceRepository* attendanceRepo, QWidget* parent = nullptr);

private slots:
    void onBarcodeEntered();
    void onManualEntry();

private:
    void setupUi();
    void processBarcode(const QString& barcode);
    void showSuccessMessage(const User& user, Attendance::Type type, const QDateTime& timestamp);

    IUserRepository* m_userRepo;
    IAttendanceRepository* m_attendanceRepo;
    
    QLabel* m_titleLbl = nullptr;
    QLabel* m_animationLbl = nullptr;
    QLineEdit* m_barcodeEdit = nullptr;
    QPushButton* m_manualBtn = nullptr;
};

#endif // ATTENDANCE_DIALOG_H
