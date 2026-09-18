#ifndef CLOSE_SESSION_DIALOG_H
#define CLOSE_SESSION_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include "core/register_session.h"
#include "data/irepositories.h"

class CloseSessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloseSessionDialog(const RegisterSession& session, QWidget* parent = nullptr);
    
    double getClosingCash() const { return m_closingCash; }

private slots:
    void onActualCashChanged();
    void onActualCardChanged();
    void calculateVariance();

private:
    void setupUi();
    void loadOrdersSummary();
    
    RegisterSession m_session;
    IOrderRepository* m_orderRepo = nullptr;
    
    double m_totalCash = 0.0;
    double m_totalCard = 0.0;
    double m_totalAmount = 0.0;
    double m_closingCash = 0.0;
    
    QTableWidget* m_ordersTable = nullptr;
    QLabel* m_orderCountLbl = nullptr;
    QLabel* m_expectedCashLbl = nullptr;
    QLabel* m_expectedCardLbl = nullptr;
    QLabel* m_totalExpectedLbl = nullptr;
    
    QDoubleSpinBox* m_actualCashSpin = nullptr;
    QDoubleSpinBox* m_actualCardSpin = nullptr;
    
    QLabel* m_cashVarianceLbl = nullptr;
    QLabel* m_cardVarianceLbl = nullptr;
    QLabel* m_totalVarianceLbl = nullptr;
};

#endif // CLOSE_SESSION_DIALOG_H
