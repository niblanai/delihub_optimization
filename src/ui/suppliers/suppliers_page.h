#ifndef SUPPLIERS_PAGE_H
#define SUPPLIERS_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class SuppliersPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit SuppliersPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onAdd();
    void onSearch();

private:
    void setupUi();
    void loadSuppliers();

    ISupplierRepository* m_repo = nullptr;
    QList<Supplier> m_suppliers;

    QLineEdit*    m_searchEdit  = nullptr;
    QTableWidget* m_table       = nullptr;
    QPushButton*  m_addBtn      = nullptr;
    QLabel*       m_statusLbl   = nullptr;
};

#endif // SUPPLIERS_PAGE_H
