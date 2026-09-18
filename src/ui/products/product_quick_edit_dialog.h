#ifndef PRODUCT_QUICK_EDIT_DIALOG_H
#define PRODUCT_QUICK_EDIT_DIALOG_H

// ProductQuickEditDialog — lightweight inline editor for a product's
// name, barcode, and stock quantity.  Used by:
//   • ProductsPage  (Task 1)  — double-click on any row in the Products table
//   • ReturnsPage   (Task 8)  — double-click on any row in the Returns table
//
// Pass the product ID; the dialog loads fresh data from the repo itself so
// callers do not need to fetch first.

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include "data/irepositories.h"

class ProductQuickEditDialog : public QDialog {
    Q_OBJECT
public:
    // productId  — existing product to edit (must be > 0)
    // repo       — product repository; dialog does NOT take ownership
    explicit ProductQuickEditDialog(int productId,
                                    IProductRepository* repo,
                                    QWidget* parent = nullptr);

    // Returns false if the product was not found or save failed.
    // Call after exec() == QDialog::Accepted to check outcome.
    bool saveSucceeded() const { return m_saved; }

private slots:
    void onSave();

private:
    void setupUi();

    int                 m_productId = 0;
    IProductRepository* m_repo      = nullptr;
    Product             m_product;          // loaded in constructor
    bool                m_saved     = false;

    QLineEdit*        m_nameEdit    = nullptr;
    QLineEdit*        m_barcodeEdit = nullptr;
    QSpinBox*         m_qtySpin     = nullptr;
    QDialogButtonBox* m_buttons     = nullptr;
};

#endif // PRODUCT_QUICK_EDIT_DIALOG_H
