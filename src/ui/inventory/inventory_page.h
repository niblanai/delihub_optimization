#ifndef INVENTORY_PAGE_H
#define INVENTORY_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QDateEdit>
#include <QComboBox>
#include "ui/translatable_page.h"
#include "core/stock_count.h"
#include "data/irepositories.h"

class InventoryPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit InventoryPage(QWidget* parent = nullptr);
    ~InventoryPage() override;
    void refresh();
    void retranslateUi() override;

private slots:
    void onAddStockCount();
    void onViewStockCount();
    void onDeleteStockCount();
    void onSelectionChanged();
    void loadStockCounts();

private:
    void setupUi();

    IStockCountRepository* m_repo = nullptr;
    IProductRepository*    m_productRepo = nullptr;
    
    QTableWidget*  m_table         = nullptr;
    QPushButton*   m_addBtn        = nullptr;
    QPushButton*   m_viewBtn       = nullptr;
    QPushButton*   m_deleteBtn     = nullptr;
    QLabel*        m_statusLbl     = nullptr;
    QDateEdit*     m_dateFromEdit  = nullptr;
    QDateEdit*     m_dateToEdit    = nullptr;
    QComboBox*     m_statusFilter  = nullptr;
    
    QList<StockCount> m_stockCounts;
};

#endif // INVENTORY_PAGE_H
