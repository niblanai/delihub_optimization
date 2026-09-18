#ifndef PURCHASES_PAGE_H
#define PURCHASES_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class PurchasesPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit PurchasesPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onAdd();
    void onView();
    void onDelete();
    void onSelectionChanged();

private:
    void setupUi();
    void loadInvoices();

    IPurchaseInvoiceRepository* m_repo = nullptr;
    ISupplierRepository* m_supplierRepo = nullptr;
    QList<PurchaseInvoice> m_invoices;

    QTableWidget* m_table       = nullptr;
    QPushButton*  m_addBtn      = nullptr;
    QPushButton*  m_viewBtn     = nullptr;
    QPushButton*  m_deleteBtn   = nullptr;
    QLabel*       m_statusLbl   = nullptr;
};

#endif // PURCHASES_PAGE_H
