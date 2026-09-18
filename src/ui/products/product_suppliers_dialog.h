#ifndef PRODUCT_SUPPLIERS_DIALOG_H
#define PRODUCT_SUPPLIERS_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include "core/product.h"
#include "core/product_supplier.h"
#include "data/product_supplier_repository.h"
#include "data/irepositories.h"
#include <vector>

class ProductSuppliersDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProductSuppliersDialog(const Product& product,
                                    IProductSupplierRepository* psRepo,
                                    ISupplierRepository* supplierRepo,
                                    QWidget* parent = nullptr);
    ~ProductSuppliersDialog();

private slots:
    void onAddSupplier();
    void onRemoveSupplier();
    void onSetPreferred();
    void onSave();
    void refreshTable();

private:
    void setupUi();
    void loadSuppliers();
    void loadProductSuppliers();

    Product m_product;
    IProductSupplierRepository* m_psRepo;
    ISupplierRepository* m_supplierRepo;
    
    std::vector<ProductSupplier> m_productSuppliers;
    QList<Supplier> m_allSuppliers;
    
    QTableWidget* m_table;
    QComboBox* m_supplierCombo;
    QDoubleSpinBox* m_priceSpin;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_preferredBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
};

#endif // PRODUCT_SUPPLIERS_DIALOG_H
