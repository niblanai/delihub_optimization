#ifndef CUSTOMERS_PAGE_H
#define CUSTOMERS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QLabel>
#include <QTimer>
#include "ui/customers/customer_table_model.h"
#include "ui/translatable_page.h"
#include "ui/pagination_bar.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"

class CustomersPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit CustomersPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onSearch(const QString& text);
    void onAddCustomer();
    void onEditCustomer();
    void onDeleteCustomer();
    void onSelectionChanged();
    void onManageRegions();
    void onImportCustomers();
    void runStatusAutomation();   // Task 9: auto-update Hesitant/Inactive status

private:
    void setupUi();
    void loadCustomers();
    void loadRegionsAndProducts();

    // UI
    QLineEdit*           m_searchEdit       = nullptr;
    QPushButton*         m_addBtn           = nullptr;
    QPushButton*         m_editBtn          = nullptr;
    QPushButton*         m_deleteBtn        = nullptr;
    QPushButton*         m_manageRegionsBtn = nullptr;
    QPushButton*         m_importBtn        = nullptr;
    QLabel*              m_pageTitleLabel   = nullptr;
    QTableView*          m_tableView        = nullptr;
    QLabel*              m_statusLabel      = nullptr;
    CustomerTableModel*  m_model            = nullptr;
    PaginationBar*       m_pagination       = nullptr;

    // Data
    ICustomerRepository* m_repo        = nullptr;
    IRegionRepository*   m_regionRepo  = nullptr;
    IProductRepository*  m_productRepo = nullptr;
    IOrderRepository*    m_orderRepo   = nullptr;   // Task 9: for last-order-date lookup
    QList<Region>        m_regions;
    QList<Product>       m_products;
    QTimer*              m_statusTimer = nullptr;   // Task 9: periodic status automation
};

#endif // CUSTOMERS_PAGE_H
