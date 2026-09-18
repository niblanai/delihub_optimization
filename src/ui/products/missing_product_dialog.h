#ifndef MISSING_PRODUCT_DIALOG_H
#define MISSING_PRODUCT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include "core/missing_product.h"
#include "core/product.h"

// Issues 4 & 5: unified Add / Edit dialog for missing products.
// - Search field finds products from the catalog; selecting one auto-fills name/barcode.
// - Qty + unit label (كرتونة / علبة / قطعة / other).
// - Can also be used to edit an existing MissingProduct (call setMissingProduct()).
class MissingProductDialog : public QDialog {
    Q_OBJECT
public:
    explicit MissingProductDialog(const QList<Product>&        products,
                                  const QList<MissingProduct>& existingMissing = {},
                                  QWidget* parent = nullptr);

    // Pre-fill for editing an existing record
    void setMissingProduct(const MissingProduct& mp);

    MissingProduct getMissingProduct() const;

    // Result of the conflict resolution (set when validate() handles a duplicate)
    enum class ConflictAction { None, Replace, Add };
    ConflictAction conflictAction() const { return m_conflictAction; }
    int            conflictExistingId() const { return m_conflictExistingId; }

private slots:
    void validate();
    void onSearchChanged(const QString& text);
    void onProductSelected(QListWidgetItem* item);

private:
    void setupUi();

    QList<Product>          m_products;
    QList<MissingProduct>   m_existingMissing;   // for duplicate detection
    int               m_editId             = 0;
    int               m_productId          = 0;
    ConflictAction    m_conflictAction     = ConflictAction::None;
    int               m_conflictExistingId = 0;

    // Search row
    QLineEdit*        m_searchEdit  = nullptr;
    QListWidget*      m_searchPopup = nullptr;

    // Read-only display of selected product
    QLabel*           m_productNameLbl = nullptr;
    QLabel*           m_barcodeValLbl  = nullptr;
    QLabel*           m_stockValLbl    = nullptr;   // Issue 1: current stock from catalog

    // Editable fields
    QSpinBox*         m_qtySpin    = nullptr;
    QComboBox*        m_unitCombo  = nullptr;

    QDialogButtonBox* m_buttons    = nullptr;
};

#endif // MISSING_PRODUCT_DIALOG_H
