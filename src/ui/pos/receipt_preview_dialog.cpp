#include "receipt_preview_dialog.h"
#include "services/lang_manager.h"
#include "services/receipt_printer.h"
#include "infra/config_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPrinter>
#include <QPageSize>
#include <QPrintDialog>
#include <QTextDocument>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>

using namespace ReceiptTemplateNS;

// Helper function to find sidebar icons
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

ReceiptPreviewDialog::ReceiptPreviewDialog(const Order& order, const RegisterSession& session,
                                           const QString& paymentMethod, QWidget* parent)
    : QDialog(parent), m_order(order), m_session(session), m_paymentMethod(paymentMethod)
{
    setWindowTitle(LangManager::instance().t("معاينة الفاتورة"));
    resize(450, 700);
    setupUi();
}

void ReceiptPreviewDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    
    // Browser to display receipt
    m_browser = new QTextBrowser(this);
    m_html = generateHtml();
    m_browser->setHtml(m_html);
    layout->addWidget(m_browser, 1);
    
    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    auto* closeBtn = new QPushButton(LangManager::instance().t("إغلاق"), this);
    closeBtn->setObjectName("secondaryBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    m_printBtn = new QPushButton(this);
    QIcon printIcon = SvgIconHelper::icon(sidebarSvgPath("print-svgrepo-com.svg"), 
                                          QColor("#3B82F6"), 20);
    m_printBtn->setIcon(printIcon);
    m_printBtn->setIconSize(QSize(20, 20));
    m_printBtn->setText(" " + LangManager::instance().t("طباعة الفاتورة"));
    m_printBtn->setObjectName("accentButton");
    connect(m_printBtn, &QPushButton::clicked, this, &ReceiptPreviewDialog::onPrint);
    
    btnLayout->addWidget(closeBtn);
    btnLayout->addWidget(m_printBtn);
    layout->addLayout(btnLayout);
}

QString ReceiptPreviewDialog::generateHtml() {
    // FIX: this used to build one fixed-size HTML document by hand — a
    // hard-coded 280px barcode image regardless of paper width, and an
    // items table with no column sizing. Neither scaled with the page, which
    // is why the barcode dominated the receipt and the layout never lined up.
    //
    // Now it renders through the same template the Receipt Designer edits, so
    // the barcode, logo, and every block are sized as a proportion of the
    // configured paper width (58mm or 80mm) and this preview always matches
    // what actually prints.
    const int paperWidthMm = ConfigManager::instance().receiptPaperWidthMm();
    const QString json = ConfigManager::instance().receiptTemplateJson(paperWidthMm);
    const ReceiptTemplate tpl = json.isEmpty() ? ReceiptTemplate::defaultTemplate(paperWidthMm)
                                                : ReceiptTemplate::fromJson(json);

    return ReceiptPrinter::generateHtmlFromTemplate(m_order, m_session, m_paymentMethod, tpl);
}

void ReceiptPreviewDialog::onPrint() {
    // Print the EXACT html shown in the preview (m_html), and at the exact
    // paper width it was built for — both used to be a separate, inconsistent
    // hard-coded 80mm, which is part of why print output didn't match preview.
    const int paperWidthMm = ConfigManager::instance().receiptPaperWidthMm();
    const QString printerName = ConfigManager::instance().receiptPrinterName();

    if (!ReceiptPrinter::printHtml(m_html, paperWidthMm, printerName, this)) {
        if (printerName.isEmpty()) return;   // user cancelled the print dialog
        QMessageBox::warning(this, LangManager::instance().t("خطأ"),
            LangManager::instance().t("تعذّرت الطباعة. تأكد من اتصال الطابعة."));
        return;
    }

    QMessageBox::information(this,
        LangManager::instance().t("نجح"),
        LangManager::instance().t("تم طباعة الفاتورة بنجاح"));
}
