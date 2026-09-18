#ifndef PRODUCT_DIALOG_H
#define PRODUCT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QTableWidget>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include "core/product.h"
#include "core/category.h"
#include "core/product_price_history.h"
#include "data/irepositories.h"

class ProductDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProductDialog(const QList<Category>& categories, 
                          IProductSubUnitRepository* subUnitRepo = nullptr,
                          QWidget* parent = nullptr);

    void    setProduct(const Product& product, const QList<ProductPriceHistory>& history = {});
    Product getProduct() const;
    void    saveSubUnits(int productId);  // Save sub-units after product is saved

private slots:
    void validate();
    void onSelectImage();
    void onClearImage();

private:
    void setupUi(const QList<Category>& categories);
    QString resizeAndSaveImage(const QString& sourcePath);
    static QString productImagesDir();

    QLineEdit*        m_nameEdit        = nullptr;
    QLineEdit*        m_barcodeEdit     = nullptr;
    QComboBox*        m_catCombo        = nullptr;
    QDoubleSpinBox*   m_priceSpin       = nullptr;
    QDoubleSpinBox*   m_costPriceSpin   = nullptr;  // Cost price (سعر الشراء)
    QCheckBox*        m_hasTaxCheck     = nullptr;
    QRadioButton*     m_solidRadio      = nullptr;   // Product type: Solid
    QRadioButton*     m_weightedRadio   = nullptr;   // Product type: Weighted
    QLineEdit*        m_pluBarcodeEdit  = nullptr;   // PLU barcode for weighted
    QSpinBox*         m_stockQtySpin    = nullptr;   // Quantity (current stock)
    QSpinBox*         m_safetyMarginSpin = nullptr;  // Safety Margin (reorder threshold)
    QComboBox*        m_statusCombo     = nullptr;
    QDateEdit*        m_manufactureDateEdit = nullptr;
    QDateEdit*        m_expiryDateEdit  = nullptr;
    QTableWidget*     m_historyTable    = nullptr;
    QTableWidget*     m_subUnitsTable   = nullptr;  // Sub-units table
    QDialogButtonBox* m_buttons         = nullptr;
    
    // Image upload
    QLabel*           m_imagePreview    = nullptr;
    QPushButton*      m_selectImageBtn  = nullptr;
    QPushButton*      m_clearImageBtn   = nullptr;
    QString           m_imagePath;

    int m_productId = 0;
    IProductSubUnitRepository* m_subUnitRepo = nullptr;
};

#endif // PRODUCT_DIALOG_H
