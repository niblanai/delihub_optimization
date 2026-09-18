#ifndef RETURNS_PAGE_H
#define RETURNS_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class ReturnsPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit ReturnsPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onAdd();
    void onDelete();
    void onSelectionChanged();
    void onPrintCreditNote();
    void onEditReturn(int row);   // Task 8: double-click to edit product fields

private:
    void setupUi();
    void loadReturns();

    IOrderReturnRepository* m_repo        = nullptr;
    IOrderRepository*       m_orderRepo   = nullptr;
    ICustomerRepository*    m_customerRepo= nullptr;
    IProductRepository*     m_productRepo = nullptr;
    QList<OrderReturn>      m_returns;

    QTableWidget* m_table        = nullptr;
    QPushButton*  m_addBtn       = nullptr;
    QPushButton*  m_deleteBtn    = nullptr;
    QPushButton*  m_printBtn     = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QLabel*       m_totalLbl     = nullptr;
};

#endif // RETURNS_PAGE_H
