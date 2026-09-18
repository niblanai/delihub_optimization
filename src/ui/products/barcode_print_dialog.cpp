#include "barcode_print_dialog.h"
#include "services/barcode_generator.h"
#include "services/lang_manager.h"
#include "infra/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPrinter>
#include <QPainter>
#include <QPrintDialog>
#include <QMessageBox>

BarcodePrintDialog::BarcodePrintDialog(const Product& product, QWidget* parent)
    : QDialog(parent), m_product(product)
{
    setupUi();
    onPreview(); // Show initial preview
}

void BarcodePrintDialog::setupUi() {
    auto& L = LangManager::instance();
    setWindowTitle(L.t("Print Barcode"));
    setMinimumWidth(400);
    setModal(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Product info
    auto* infoLabel = new QLabel(QString("<b>%1:</b> %2<br><b>%3:</b> %4")
                                 .arg(L.t("Product"), m_product.name())
                                 .arg(L.t("Barcode"), m_product.barcode().isEmpty() ? L.t("No barcode") : m_product.barcode()));
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Options
    auto* optionsGroup = new QGroupBox(L.t("Print Options"));
    auto* optionsLayout = new QFormLayout(optionsGroup);

    m_includeNameCheck = new QCheckBox(L.t("Include product name"));
    m_includeNameCheck->setChecked(true);
    optionsLayout->addRow("", m_includeNameCheck);

    m_includePriceCheck = new QCheckBox(L.t("Include price"));
    m_includePriceCheck->setChecked(false);
    optionsLayout->addRow("", m_includePriceCheck);

    m_quantitySpin = new QSpinBox;
    m_quantitySpin->setRange(1, 100);
    m_quantitySpin->setValue(1);
    m_quantitySpin->setSuffix(" " + L.t("label(s)"));
    optionsLayout->addRow(L.t("Quantity") + ":", m_quantitySpin);

    mainLayout->addWidget(optionsGroup);

    // Preview
    auto* previewGroup = new QGroupBox(L.t("Preview"));
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(150);
    m_previewLabel->setStyleSheet("QLabel { border: 1px solid #ccc; background: white; padding: 10px; }");
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    m_previewBtn = new QPushButton(L.t("Refresh Preview"));
    m_printBtn = new QPushButton(L.t("Print"));
    m_printBtn->setObjectName("primaryBtn");
    auto* cancelBtn = new QPushButton(L.t("Cancel"));

    btnLayout->addWidget(m_previewBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_printBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_previewBtn, &QPushButton::clicked, this, &BarcodePrintDialog::onPreview);
    connect(m_printBtn, &QPushButton::clicked, this, &BarcodePrintDialog::onPrint);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Auto-refresh preview when options change
    connect(m_includeNameCheck, &QCheckBox::toggled, this, &BarcodePrintDialog::onPreview);
    connect(m_includePriceCheck, &QCheckBox::toggled, this, &BarcodePrintDialog::onPreview);
}

QPixmap BarcodePrintDialog::generateBarcodeImage() {
    auto& L = LangManager::instance();
    
    // Generate barcode (300x150 px base size)
    QString barcodeText = m_product.barcode().isEmpty() ? 
                         QString::number(m_product.id()) : 
                         m_product.barcode();
    
    // Render barcode as QImage (300px width, 100px height)
    QImage barcodeImage = BarcodeGenerator::renderCode128FitWidth(barcodeText, 300, 100);
    
    if (barcodeImage.isNull()) {
        Logger::instance().warn("Failed to generate barcode for: " + barcodeText);
        return QPixmap();
    }
    
    QPixmap barcodePixmap = QPixmap::fromImage(barcodeImage);

    // Calculate total height needed
    int totalHeight = barcodePixmap.height() + 20; // 20px padding
    if (m_includeNameCheck->isChecked()) totalHeight += 30;
    if (m_includePriceCheck->isChecked()) totalHeight += 25;

    // Create final image
    QPixmap finalPixmap(320, totalHeight);
    finalPixmap.fill(Qt::white);

    QPainter painter(&finalPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    int yPos = 10;

    // Draw product name
    if (m_includeNameCheck->isChecked()) {
        QFont nameFont("Arial", 10, QFont::Bold);
        painter.setFont(nameFont);
        painter.drawText(QRect(10, yPos, 300, 25), Qt::AlignCenter, m_product.name());
        yPos += 30;
    }

    // Draw barcode
    painter.drawPixmap((320 - barcodePixmap.width()) / 2, yPos, barcodePixmap);
    yPos += barcodePixmap.height() + 5;

    // Draw price
    if (m_includePriceCheck->isChecked()) {
        QString priceText = QString("%1 %2")
                           .arg(m_product.price(), 0, 'f', 2)
                           .arg(L.t("EGP"));
        QFont priceFont("Arial", 12, QFont::Bold);
        painter.setFont(priceFont);
        painter.drawText(QRect(10, yPos, 300, 20), Qt::AlignCenter, priceText);
    }

    painter.end();
    return finalPixmap;
}

void BarcodePrintDialog::onPreview() {
    QPixmap preview = generateBarcodeImage();
    if (!preview.isNull()) {
        m_previewLabel->setPixmap(preview.scaled(m_previewLabel->size(), 
                                                 Qt::KeepAspectRatio, 
                                                 Qt::SmoothTransformation));
    }
}

void BarcodePrintDialog::onPrint() {
    auto& L = LangManager::instance();
    
    if (m_product.barcode().isEmpty()) {
        QMessageBox::warning(this, L.t("Warning"), 
            L.t("Product has no barcode. Using product ID instead."));
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QSizeF(52, 30), QPageSize::Millimeter)); // Label size
    printer.setPageOrientation(QPageLayout::Landscape);

    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() != QDialog::Accepted) {
        return;
    }

    QPixmap barcodeImage = generateBarcodeImage();
    if (barcodeImage.isNull()) {
        QMessageBox::critical(this, L.t("Error"), L.t("Failed to generate barcode."));
        return;
    }

    QPainter painter;
    if (!painter.begin(&printer)) {
        QMessageBox::critical(this, L.t("Error"), L.t("Failed to start printing."));
        return;
    }

    int quantity = m_quantitySpin->value();
    for (int i = 0; i < quantity; ++i) {
        if (i > 0) {
            if (!printer.newPage()) {
                QMessageBox::warning(this, L.t("Warning"), 
                    L.t("Failed to create new page for label %1").arg(i + 1));
                break;
            }
        }

        // Scale barcode to fit page
        QRect pageRect = painter.viewport();
        QSize scaledSize = barcodeImage.size().scaled(pageRect.size(), Qt::KeepAspectRatio);
        QRect targetRect((pageRect.width() - scaledSize.width()) / 2,
                        (pageRect.height() - scaledSize.height()) / 2,
                        scaledSize.width(), scaledSize.height());
        
        painter.drawPixmap(targetRect, barcodeImage);
    }

    painter.end();

    Logger::instance().info(QString("Printed %1 barcode label(s) for product: %2")
                           .arg(quantity).arg(m_product.name()));
    
    QMessageBox::information(this, L.t("Success"), 
        L.t("Barcode printed successfully!"));
    accept();
}
