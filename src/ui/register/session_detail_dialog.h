#ifndef SESSION_DETAIL_DIALOG_H
#define SESSION_DETAIL_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTableWidget>
#include "core/register_session.h"
#include "data/irepositories.h"

class SessionDetailDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDetailDialog(const RegisterSession& session, QWidget* parent = nullptr);

private:
    void setupUi();
    void loadOrders();
    
    RegisterSession m_session;
    IOrderRepository* m_orderRepo = nullptr;
    
    QLabel* m_sessionInfoLbl = nullptr;
    QLabel* m_summaryLbl = nullptr;
    QTableWidget* m_ordersTable = nullptr;
};

#endif // SESSION_DETAIL_DIALOG_H
