#ifndef PURCHASE_INVOICE_DIALOG_H
#define PURCHASE_INVOICE_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include "core/purchase_invoice.h"
#include "core/supplier.h"
#include "core/product.h"
#include "data/irepositories.h"

class PurchaseInvoiceDialog : public QDialog {
    Q_OBJECT
public:
    explicit PurchaseInvoiceDialog(IPurchaseInvoiceRepository* invoiceRepo,
                                   ISupplierRepository* supplierRepo,
                                   IProductRepository* productRepo,
                                   IStockMovementRepository* stockMovementRepo,
                                   IProductCostHistoryRepository* costHistoryRepo,
                                   QWidget* parent = nullptr);
    
    void setInvoice(const PurchaseInvoice& invoice);
    PurchaseInvoice getInvoice() const;

private slots:
    void onAddItem();
    void onRemoveItem();
    void onConfirm();
    void updateTotals();

private:
    void setupUi();
    void loadSuppliers();
    void loadProducts();
    QString generateInvoiceNumber();
    
    IPurchaseInvoiceRepository* m_invoiceRepo;
    ISupplierRepository* m_supplierRepo;
    IProductRepository* m_productRepo;
    IStockMovementRepository* m_stockMovementRepo;
    IProductCostHistoryRepository* m_costHistoryRepo;
    
    QList<Supplier> m_suppliers;
    QList<Product> m_products;
    QList<PurchaseInvoiceItem> m_items;
    int m_invoiceId = 0;
    
    QLineEdit* m_invoiceNumberEdit = nullptr;
    QComboBox* m_supplierCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QTableWidget* m_itemsTable = nullptr;
    QTextEdit* m_notesEdit = nullptr;
    QLineEdit* m_paidAmountEdit = nullptr;
    QLabel* m_totalLbl = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

#endif // PURCHASE_INVOICE_DIALOG_H
