#ifndef REGISTER_PAGE_H
#define REGISTER_PAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class RegisterPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit RegisterPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onOpenSession();
    void onCloseSession();
    void onSessionCardDoubleClick(int sessionId);
    void onBarcodeSearch();

private:
    void setupUi();
    void loadSessions();
    RegisterSession getCurrentOpenSession();
    
    // Helper to calculate session totals
    struct SessionTotals {
        double cash = 0.0;
        double card = 0.0;
        double total = 0.0;
        int orderCount = 0;
    };
    SessionTotals calculateSessionTotals(int sessionId);

    IRegisterSessionRepository* m_repo = nullptr;
    ICashMovementRepository* m_cashRepo = nullptr;
    IOrderRepository* m_orderRepo = nullptr;
    QList<RegisterSession> m_sessions;

    QGridLayout*  m_sessionsGrid = nullptr;
    QPushButton*  m_openBtn      = nullptr;
    QPushButton*  m_closeBtn     = nullptr;
    QPushButton*  m_barcodeBtn   = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QLabel*       m_currentLbl   = nullptr;
};

#endif // REGISTER_PAGE_H
