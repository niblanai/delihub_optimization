#ifndef REPORTS_PAGE_H
#define REPORTS_PAGE_H

#include <QWidget>
#include <QDateEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QFrame>
#include <QStackedWidget>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class ReportsPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit ReportsPage(QWidget* parent = nullptr);
    void retranslateUi() override;
    void refresh() {}  // called by navigateTo — no-op, report runs on user request

private slots:
    void onGenerateReport();
    void onExportPDF();
    void onExportExcel();

private:
    void setupUi();
    void generateSalesReport();
    void generateTopProductsReport();
    void generateTopCustomersReport();
    void generateInactiveCustomers();
    void generateDeliveryFeeReport();
    void generateMissingProductsReport();
    void generateExpiredProductsReport();
    void generateCancelledOrdersReport();
    void generateTopProductsPerCustomer();   // Report 8: top products for a given customer

    IOrderRepository*          m_orderRepo    = nullptr;
    ICustomerRepository*       m_customerRepo = nullptr;
    IProductRepository*        m_productRepo  = nullptr;
    IMissingProductRepository* m_missingRepo  = nullptr;

    QMap<int, QString> m_customerMap;
    QMap<int, QString> m_productMap;

    // UI
    QComboBox*     m_reportTypeCombo     = nullptr;
    QComboBox*     m_missingStatusCombo  = nullptr;
    QLabel*        m_missingStatusLabel  = nullptr;
    // Customer filter (report 8)
    QLabel*        m_customerFilterLabel = nullptr;
    QLineEdit*     m_customerFilterEdit  = nullptr;  // search by name/phone/address
    QDateEdit*     m_dateFromEdit        = nullptr;
    QDateEdit*     m_dateToEdit          = nullptr;
    QPushButton*   m_generateBtn         = nullptr;
    QPushButton*   m_exportPdfBtn        = nullptr;
    QPushButton*   m_exportXlsxBtn       = nullptr;
    QStackedWidget* m_resultStack        = nullptr;
    QTableWidget*  m_resultTable         = nullptr;
    QTextBrowser*  m_invoiceView         = nullptr;
    QLabel*        m_summaryLabel        = nullptr;
    QString        m_cancelledHtml;  // Task 11 BUG FIX: store generated HTML for reliable export
};

#endif // REPORTS_PAGE_H
