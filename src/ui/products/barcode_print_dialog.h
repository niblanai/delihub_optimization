#ifndef BARCODE_PRINT_DIALOG_H
#define BARCODE_PRINT_DIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include "core/product.h"

class BarcodePrintDialog : public QDialog {
    Q_OBJECT
public:
    explicit BarcodePrintDialog(const Product& product, QWidget* parent = nullptr);

private slots:
    void onPrint();
    void onPreview();

private:
    void setupUi();
    QPixmap generateBarcodeImage();

    Product m_product;
    
    QCheckBox* m_includePriceCheck = nullptr;
    QCheckBox* m_includeNameCheck = nullptr;
    QSpinBox*  m_quantitySpin = nullptr;
    QLabel*    m_previewLabel = nullptr;
    QPushButton* m_printBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;
};

#endif // BARCODE_PRINT_DIALOG_H
