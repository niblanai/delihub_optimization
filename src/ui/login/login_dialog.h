#ifndef LOGIN_DIALOG_H
#define LOGIN_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QSvgRenderer>
#include "data/irepositories.h"

// Forward declarations
class QSvgWidget;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(IUserRepository*  userRepo,
                         IRoleRepository*  roleRepo,
                         QWidget* parent = nullptr);
    ~LoginDialog() override;   // explicit destructor — disconnects LangManager signal
    
    bool isPosMode() const { return m_isPosMode; }

protected:
    // Task 10: block window close (X button / Alt-F4) until an admin account exists
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onLogin();
    void onForgotPassword();
    void onThemeChanged(int index);
    void onLangChanged(int index);

private:
    void setupUi();
    bool showFirstRunSetup();  // returns true only if account actually created
    void processBarcode(const QString& barcode);  // Process attendance barcode
    void showBarcodeFeedback(const QString& html, bool success, int holdMs);

    IUserRepository* m_userRepo;
    IRoleRepository* m_roleRepo;
    bool             m_ownedRepos          = false;
    bool             m_isFirstRun          = false;  // true only on branch with zero users
    bool             m_isPosMode           = false;  // Phase 1: POS interface selected
    QString          m_lastCreatedUsername;           // pre-fill login field after setup
    int              m_failedAttempts      = 0;

    QLineEdit*   m_userEdit      = nullptr;
    QLineEdit*   m_passEdit      = nullptr;
    QLabel*      m_errorLbl      = nullptr;
    QLabel*      m_attemptsLabel = nullptr;
    QLabel*      m_brandLbl      = nullptr;
    QLabel*      m_subLbl        = nullptr;
    QPushButton* m_loginBtn      = nullptr;
    QComboBox*   m_themeCombo    = nullptr;
    QComboBox*   m_langCombo     = nullptr;
    QComboBox*   m_branchCombo   = nullptr;
    QComboBox*   m_interfaceTypeCombo = nullptr;  // Phase 1: Admin/POS interface selection
    
    // Attendance barcode capture.
    // A scanner emits its whole payload in a few ms; anything slower is a human
    // typing, so the buffer is dropped instead of swallowing their keystrokes.
    QString      m_barcodeBuffer;
    qint64       m_lastBarcodeKeyMs = 0;
    static constexpr qint64 kBarcodeKeyGapMs = 120;
    QSvgWidget*  m_barcodeSvgWidget = nullptr;
    QSvgRenderer* m_barcodeSvgRenderer = nullptr;  // Renderer for recolored SVG
    QLabel*      m_feedbackLabel = nullptr;

    // Helper to recolor SVG data based on icon mode
    void recolorSvgData(QByteArray& svg, const QColor& color);

    // Stored connection handle so we can disconnect explicitly in destructor
    QMetaObject::Connection m_langConn;
};

#endif // LOGIN_DIALOG_H
