#include "receipt_printer.h"
#include "services/lang_manager.h"
#include "services/barcode_generator.h"
#include "infra/config_manager.h"
#include <QTextDocument>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QPageSize>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QBuffer>
#include <QPixmap>
#include <QWidget>

using namespace ReceiptTemplateNS;

namespace {

constexpr double kMmToPx = 96.0 / 25.4;   // CSS px at 96dpi, matches QTextDocument's default DPI
constexpr double kSideMarginMm = 3.0;

int mmToPx(double mm) { return static_cast<int>(qRound(mm * kMmToPx)); }

QString escapeHtml(const QString& s) {
    QString out = s;
    out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return out;
}

// One receipt block -> one HTML fragment. Every block is width-constrained to
// the *content* width (paper width minus the side margins), which is the piece
// that was missing before: the barcode used to render at a fixed pixel size
// with no relation to the page, so it either overflowed a 58mm receipt or
// looked oversized against everything else on 80mm.
QString renderBlock(const ReceiptBlock& block, int contentWidthPx,
                    const Order& order, const RegisterSession& session,
                    const QString& paymentMethod, const QString& currency)
{
    const QVariantMap& p = block.properties;

    switch (block.type) {
    case BlockType::Logo: {
        const QString path = ConfigManager::instance().receiptLogoPath();
        if (path.isEmpty() || !QFile::exists(path)) return {};
        const double heightMm = p.value("heightMm", 15.0).toDouble();
        const int hPx = mmToPx(heightMm);
        // Logo width constrained to 60% of content width (40-45mm @ 80mm, 27-30mm @ 58mm)
        const int logoMaxWidthPx = contentWidthPx * 0.6;
        const QString align = p.value("align", "center").toString();

        // Scale the actual pixel data ourselves (same technique already used
        // for the barcode block below) instead of asking the HTML renderer
        // to reconcile a fixed height against a competing max-width via CSS.
        // Qt's rich-text engine doesn't have a real CSS box model, so with
        // both a fixed height AND a max-width on the same <img>, whichever
        // constraint the image actually needs kept winning regardless of
        // the height value — which on a wide logo meant height edits never
        // visibly changed anything. Scaling in C++ removes the ambiguity:
        // the embedded image IS the requested size, full stop.
        QPixmap pix(path);
        if (pix.isNull()) return {};
        pix = pix.scaledToHeight(hPx, Qt::SmoothTransformation);
        if (pix.width() > logoMaxWidthPx)
            pix = pix.scaledToWidth(logoMaxWidthPx, Qt::SmoothTransformation);

        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        pix.save(&buf, "PNG");

        return QString("<div style='text-align:%1; margin-bottom:6px;'>"
                       "<img src='data:image/png;base64,%2'/></div>")
            .arg(align, QString(ba.toBase64()));
    }
    case BlockType::CompanyName: {
        const int pt = p.value("fontSizePt", 16).toInt();
        const bool bold = p.value("bold", true).toBool();
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:%2pt; font-weight:%3; margin-bottom:4px;'>%4</div>")
            .arg(align).arg(pt).arg(bold ? "bold" : "normal", escapeHtml(ConfigManager::instance().companyName()));
    }
    case BlockType::Address: {
        const QString v = ConfigManager::instance().companyAddress();
        if (v.trimmed().isEmpty()) return {};
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:9pt; color:#444;'>%2</div>")
            .arg(align, escapeHtml(v));
    }
    case BlockType::Phone: {
        const QString v = ConfigManager::instance().companyPhone();
        if (v.trimmed().isEmpty()) return {};
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:9pt; color:#444;'>%2: %3</div>")
            .arg(align, escapeHtml(p.value("label", "Tel").toString()), escapeHtml(v));
    }
    case BlockType::InvoiceNumber: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'><b>%2:</b> %3</div>")
            .arg(align, escapeHtml(p.value("label", "Invoice #").toString())).arg(order.id());
    }
    case BlockType::DateTime: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'>%2</div>")
            .arg(align, order.dateTime().toString(p.value("format", "yyyy-MM-dd hh:mm").toString()));
    }
    case BlockType::Cashier: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'><b>%2:</b> %3</div>")
            .arg(align, LangManager::instance().t("Session #")).arg(session.id());
    }
    case BlockType::ItemsTable: {
        const bool showQty = p.value("showQty", true).toBool();
        const bool showPrice = p.value("showPrice", true).toBool();
        QString rows;
        for (const auto& item : order.items()) {
            rows += "<tr><td style='padding:3px 2px; border-bottom:1px solid #eee;'>"
                   + escapeHtml(item.productName) + "</td>";
            if (showQty)
                rows += QString("<td style='padding:3px 2px; text-align:center; border-bottom:1px solid #eee;'>%1</td>")
                        .arg(item.quantity);
            if (showPrice)
                rows += QString("<td style='padding:3px 2px; text-align:right; border-bottom:1px solid #eee;'>%1</td>")
                        .arg(QString::number(item.unitPrice, 'f', 2));
            rows += QString("<td style='padding:3px 2px; text-align:right; border-bottom:1px solid #eee;'>%1</td>")
                    .arg(QString::number(item.unitPrice * item.quantity, 'f', 2));
        }
        QString header = "<th style='text-align:left; padding:3px 2px; border-bottom:2px solid #000;'>"
                         + LangManager::instance().t("Product") + "</th>";
        if (showQty) header += "<th style='text-align:center; padding:3px 2px; border-bottom:2px solid #000;'>"
                              + LangManager::instance().t("Qty") + "</th>";
        if (showPrice) header += "<th style='text-align:right; padding:3px 2px; border-bottom:2px solid #000;'>"
                                + LangManager::instance().t("Price") + "</th>";
        header += "<th style='text-align:right; padding:3px 2px; border-bottom:2px solid #000;'>"
                 + LangManager::instance().t("Total") + "</th>";
        return QString("<table style='width:100%%; border-collapse:collapse; font-size:9.5pt; margin:6px 0;'>"
                       "<thead><tr>%1</tr></thead><tbody>%2</tbody></table>").arg(header, rows);
    }
    case BlockType::Totals: {
        const QString align = p.value("align", "center").toString();
        QString rows;
        auto row = [&](const QString& label, double amount, bool grand=false) {
            const QString style = grand
                ? "font-size:13pt; font-weight:bold; border-top:2px solid #000; padding-top:4px;"
                : "font-size:10pt;";
            // A table row instead of display:flex — Qt's rich-text engine
            // doesn't support flexbox at all (confirmed the same bug in
            // receipt_preview_dialog.cpp), so justify-content:space-between
            // never did anything; label and value just sat side by side
            // with no real spacing logic.
            rows += QString("<tr style='%1'><td>%2</td><td style='text-align:left;'>%3</td></tr>")
                    .arg(style, label, QString::number(amount, 'f', 2) + " " + currency);
        };
        row(LangManager::instance().t("Subtotal"), order.subtotal());
        if (p.value("showDiscount", true).toBool() && order.discountAmount() > 0)
            row(LangManager::instance().t("Discount"), -order.discountAmount());
        if (p.value("showDelivery", false).toBool() && order.deliveryFee() > 0)
            row(LangManager::instance().t("Delivery Fee"), order.deliveryFee());
        row(LangManager::instance().t("TOTAL"), order.grandTotal(), true);
        rows += QString("<tr><td colspan='2' style='font-size:9pt; color:#555; padding-top:4px;'>%1: %2</td></tr>")
                .arg(LangManager::instance().t("Payment Method"), escapeHtml(paymentMethod));

        // Qt's rich-text engine doesn't implement the margin-left/right:auto
        // centering trick (also confirmed with the flex bug above), so the
        // container never actually moved regardless of `align`. The legacy
        // HTML `align` ATTRIBUTE on <table> — as opposed to a CSS property —
        // is part of the older, simpler subset Qt's renderer does support
        // reliably.
        return QString("<table align='%1' style='width:70%%; margin:6px 0; border-collapse:collapse;'>%2</table>")
            .arg(align, rows);
    }
    case BlockType::Barcode: {
        const QString data = order.invoiceBarcode();
        if (data.isEmpty()) return {};
        const int hPx = mmToPx(p.value("heightMm", 12.0).toDouble());
        // FIX: this used to render at a hard-coded pixel width unrelated to the
        // page, which is why it looked oversized/out of proportion. Fitting it
        // to the content width keeps the same ratio on 58mm and 80mm receipts.
        const QImage img = BarcodeGenerator::renderCode128FitWidth(data, contentWidthPx, hPx, 6);
        if (img.isNull()) return {};

        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");

        QString textLine;
        if (p.value("showText", true).toBool())
            textLine = QString("<div style='font-family:\"Courier New\",monospace; font-size:8.5pt; "
                               "font-weight:bold; margin-top:2px;'>%1</div>").arg(escapeHtml(data));

        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; margin:8px 0;'>"
                       "<img src='data:image/png;base64,%2' style='width:%3px; height:%4px;'/>%5</div>")
            .arg(align, QString(ba.toBase64())).arg(contentWidthPx).arg(hPx).arg(textLine);
    }
    case BlockType::Text: {
        const QString text = p.value("text", "").toString();
        if (text.trimmed().isEmpty()) return {};
        const int pt = p.value("fontSizePt", 11).toInt();
        const bool bold = p.value("bold", false).toBool();
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:%2pt; font-weight:%3; margin:3px 0;'>%4</div>")
            .arg(align).arg(pt).arg(bold ? "bold" : "normal", escapeHtml(text).replace("\n", "<br/>"));
    }
    case BlockType::Divider: {
        const QString style = p.value("style", "dashed").toString();
        if (style == "dashed") {
            // Dashed line using repeated dashes (single line)
            return QString("<div style='text-align:center; margin:6px 0; line-height:12px; font-size:10pt; color:#000; overflow:hidden; white-space:nowrap;'>%1</div>")
                .arg(QString("- ").repeated(30));  // Reduced from 50 to 30, single line only
        } else {
            // Solid line using hr
            return QString("<div style='width:100%%; margin:6px 0;'>"
                           "<hr style='border:0; border-top:1px solid #000; margin:0; padding:0;'/>"
                           "</div>");
        }
    }
    case BlockType::Spacer: {
        // Same root cause as the divider bugs: an empty, content-less
        // block's explicit height gets ignored/collapsed by Qt's renderer.
        // A non-breaking space plus a matching line-height forces the box
        // to actually occupy that height instead of collapsing to zero.
        const int hPx = mmToPx(p.value("heightMm", 4.0).toDouble());
        return QString("<div style='height:%1px; line-height:%1px; font-size:1px;'>&nbsp;</div>").arg(hPx);
    }
    }
    return {};
}

// Same block set as renderBlock() above, but fed sample data instead of a real
// Order/RegisterSession — lets the designer preview update live while the
// operator edits blocks, with no order in progress to render from.
QString renderBlockPreview(const ReceiptBlock& block, int contentWidthPx, const QString& currency)
{
    const QVariantMap& p = block.properties;

    switch (block.type) {
    case BlockType::InvoiceNumber: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'><b>%2:</b> INV-1001</div>")
            .arg(align, escapeHtml(p.value("label", "Invoice #").toString()));
    }
    case BlockType::DateTime: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'>%2</div>")
            .arg(align, QDateTime::currentDateTime().toString(p.value("format", "yyyy-MM-dd hh:mm").toString()));
    }
    case BlockType::Cashier: {
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; font-size:10pt; margin:2px 0;'><b>%2:</b> 1</div>")
            .arg(align, LangManager::instance().t("Session #"));
    }
    case BlockType::ItemsTable: {
        struct Sample { QString name; int qty; double price; };
        static const QVector<Sample> samples = {
            {"Sample Product A", 2, 25.00}, {"Sample Product B", 1, 40.00}
        };
        const bool showQty = p.value("showQty", true).toBool();
        const bool showPrice = p.value("showPrice", true).toBool();
        QString rows;
        for (const auto& s : samples) {
            rows += "<tr><td style='padding:3px 2px; border-bottom:1px solid #eee;'>" + s.name + "</td>";
            if (showQty) rows += QString("<td style='padding:3px 2px; text-align:center; border-bottom:1px solid #eee;'>%1</td>").arg(s.qty);
            if (showPrice) rows += QString("<td style='padding:3px 2px; text-align:right; border-bottom:1px solid #eee;'>%1</td>").arg(QString::number(s.price, 'f', 2));
            rows += QString("<td style='padding:3px 2px; text-align:right; border-bottom:1px solid #eee;'>%1</td>").arg(QString::number(s.price * s.qty, 'f', 2));
        }
        QString header = "<th style='text-align:left; padding:3px 2px; border-bottom:2px solid #000;'>" + LangManager::instance().t("Product") + "</th>";
        if (showQty) header += "<th style='text-align:center; padding:3px 2px; border-bottom:2px solid #000;'>" + LangManager::instance().t("Qty") + "</th>";
        if (showPrice) header += "<th style='text-align:right; padding:3px 2px; border-bottom:2px solid #000;'>" + LangManager::instance().t("Price") + "</th>";
        header += "<th style='text-align:right; padding:3px 2px; border-bottom:2px solid #000;'>" + LangManager::instance().t("Total") + "</th>";
        return QString("<table style='width:100%%; border-collapse:collapse; font-size:9.5pt; margin:6px 0;'>"
                       "<thead><tr>%1</tr></thead><tbody>%2</tbody></table>").arg(header, rows);
    }
    case BlockType::Totals: {
        const double subtotal = 90.0, discount = 5.0, delivery = 10.0;
        const double total = subtotal - discount + (p.value("showDelivery", false).toBool() ? delivery : 0);
        const QString align = p.value("align", "center").toString();
        QString rows;
        auto row = [&](const QString& label, double amount, bool grand=false) {
            const QString style = grand
                ? "font-size:13pt; font-weight:bold; border-top:2px solid #000; padding-top:4px;"
                : "font-size:10pt;";
            rows += QString("<tr style='%1'><td>%2</td><td style='text-align:left;'>%3</td></tr>")
                    .arg(style, label, QString::number(amount, 'f', 2) + " " + currency);
        };
        row(LangManager::instance().t("Subtotal"), subtotal);
        if (p.value("showDiscount", true).toBool()) row(LangManager::instance().t("Discount"), -discount);
        if (p.value("showDelivery", false).toBool()) row(LangManager::instance().t("Delivery Fee"), delivery);
        row(LangManager::instance().t("TOTAL"), total, true);

        return QString("<table align='%1' style='width:70%%; margin:6px 0; border-collapse:collapse;'>%2</table>")
            .arg(align, rows);
    }
    case BlockType::Barcode: {
        const QString data = "INV-1001-4821";
        const int hPx = mmToPx(p.value("heightMm", 12.0).toDouble());
        const QImage img = BarcodeGenerator::renderCode128FitWidth(data, contentWidthPx, hPx, 6);
        if (img.isNull()) return {};
        QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly); img.save(&buf, "PNG");
        QString textLine;
        if (p.value("showText", true).toBool())
            textLine = QString("<div style='font-family:\"Courier New\",monospace; font-size:8.5pt; font-weight:bold; margin-top:2px;'>%1</div>").arg(data);
        const QString align = p.value("align", "center").toString();
        return QString("<div style='text-align:%1; margin:8px 0;'>"
                       "<img src='data:image/png;base64,%2' style='width:%3px; height:%4px;'/>%5</div>")
            .arg(align, QString(ba.toBase64())).arg(contentWidthPx).arg(hPx).arg(textLine);
    }
    default:
        // Every block that doesn't depend on order data (Logo, CompanyName,
        // Address, Phone, Text, Divider, Spacer) renders identically in
        // preview and in the real receipt.
        return renderBlock(block, contentWidthPx, Order{}, RegisterSession{}, QString(), currency);
    }
}

} // namespace

QString ReceiptPrinter::generatePreviewHtml(const ReceiptTemplate& tpl)
{
    const bool isArabic = (LangManager::instance().current() == AppLang::Arabic);
    const QString direction = isArabic ? "rtl" : "ltr";
    const QString align = isArabic ? "right" : "left";
    const QString currency = ConfigManager::instance().currencySymbol();

    const int paperWidthPx = mmToPx(tpl.paperWidthMm);
    const int contentWidthPx = mmToPx(tpl.paperWidthMm - 2 * kSideMarginMm);

    QString body;
    for (const auto& block : tpl.blocks)
        body += renderBlockPreview(block, contentWidthPx, currency);

    return QString(R"(
<!DOCTYPE html>
<html dir="%1">
<head><meta charset="UTF-8"><style>
    * { box-sizing: border-box; }
    body { font-family:'Arial','Tahoma',sans-serif; width:%2px; margin:0 auto; padding:%3px;
           direction:%1; text-align:%4; color:#111; background:#fff; }
    th, td { text-align:%4; }
</style></head>
<body>%5</body>
</html>
    )").arg(direction).arg(paperWidthPx).arg(mmToPx(kSideMarginMm)).arg(align).arg(body);
}

QString ReceiptPrinter::generateHtmlFromTemplate(const Order& order, const RegisterSession& session,
                                                 const QString& paymentMethod,
                                                 const ReceiptTemplate& tpl)
{
    const bool isArabic = (LangManager::instance().current() == AppLang::Arabic);
    const QString direction = isArabic ? "rtl" : "ltr";
    const QString align = isArabic ? "right" : "left";
    const QString currency = ConfigManager::instance().currencySymbol();

    const int paperWidthPx = mmToPx(tpl.paperWidthMm);
    const int contentWidthPx = mmToPx(tpl.paperWidthMm - 2 * kSideMarginMm);

    QString body;
    for (const auto& block : tpl.blocks)
        body += renderBlock(block, contentWidthPx, order, session, paymentMethod, currency);

    return QString(R"(
<!DOCTYPE html>
<html dir="%1">
<head>
<meta charset="UTF-8">
<style>
    * { box-sizing: border-box; }
    body {
        font-family: 'Arial', 'Tahoma', sans-serif;
        width: %2px;
        margin: 0 auto;
        padding: %3px;
        direction: %1;
        text-align: %4;
        color: #111;
        background: #fff;
    }
    th, td { text-align: %4; }
</style>
</head>
<body>
%5
</body>
</html>
    )").arg(direction).arg(paperWidthPx).arg(mmToPx(kSideMarginMm)).arg(align).arg(body);
}

bool ReceiptPrinter::printHtml(const QString& html, int paperWidthMm,
                               const QString& printerName, QWidget* dialogParent)
{
    QTextDocument document;
    document.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    const QPageSize pageSize(QSizeF(paperWidthMm, 297), QPageSize::Millimeter, "Thermal");
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 2, 0, 2), QPageLayout::Millimeter);

    if (!printerName.isEmpty()) {
        printer.setPrinterName(printerName);
        if (printer.printerName() != printerName)
            return false;   // requested printer not found/available
    } else {
        QPrintDialog dlg(&printer, dialogParent);
        if (dlg.exec() != QDialog::Accepted) return false;
    }

    document.setPageSize(printer.pageRect(QPrinter::DevicePixel).size());
    document.print(&printer);
    return true;
}

bool ReceiptPrinter::printWithTemplate(const Order& order, const RegisterSession& session,
                                       const QString& paymentMethod,
                                       const ReceiptTemplate& tpl,
                                       const QString& printerName, QWidget* dialogParent)
{
    const QString html = generateHtmlFromTemplate(order, session, paymentMethod, tpl);
    return printHtml(html, tpl.paperWidthMm, printerName, dialogParent);
}

QStringList ReceiptPrinter::getAvailableThermalPrinters()
{
    QStringList names;
    for (const QPrinterInfo& info : QPrinterInfo::availablePrinters())
        names << info.printerName();
    return names;
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy fixed layout (kept for existing callers / backward compatibility).
// New code should use generateHtmlFromTemplate / printWithTemplate instead.
// ─────────────────────────────────────────────────────────────────────────────

QString ReceiptPrinter::generateHtml(const Order& order, const RegisterSession& session, 
                                     const QString& paymentMethod) {
    bool isArabic = (LangManager::instance().current() == AppLang::Arabic);
    QString direction = isArabic ? "rtl" : "ltr";
    QString align = isArabic ? "right" : "left";
    
    QString companyName = ConfigManager::instance().companyName();
    QString companyPhone = ConfigManager::instance().companyPhone();
    QString companyAddress = ConfigManager::instance().companyAddress();
    
    QString html = QString(R"(
<!DOCTYPE html>
<html dir="%1">
<head>
    <meta charset="UTF-8">
    <style>
        body {
            font-family: 'Courier New', monospace;
            font-size: 12px;
            margin: 0;
            padding: 20px;
            max-width: 80mm;
            direction: %1;
        }
        .header {
            text-align: center;
            border-bottom: 2px dashed #000;
            padding-bottom: 10px;
            margin-bottom: 10px;
        }
        .company-name {
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 5px;
        }
        .company-info {
            font-size: 10px;
            color: #666;
        }
        .section {
            margin: 10px 0;
            padding: 5px 0;
        }
        .section-title {
            font-weight: bold;
            border-bottom: 1px solid #000;
            margin-bottom: 5px;
        }
        .row {
            display: flex;
            justify-content: space-between;
            padding: 2px 0;
        }
        .items-table {
            width: 100%;
            border-collapse: collapse;
            margin: 10px 0;
        }
        .items-table th {
            border-bottom: 1px solid #000;
            padding: 5px 2px;
            text-align: %2;
            font-weight: bold;
        }
        .items-table td {
            padding: 5px 2px;
            text-align: %2;
        }
        .total-section {
            border-top: 2px solid #000;
            padding-top: 10px;
            margin-top: 10px;
        }
        .grand-total {
            font-size: 16px;
            font-weight: bold;
            border-top: 1px dashed #000;
            border-bottom: 1px dashed #000;
            padding: 5px 0;
            margin: 5px 0;
        }
        .footer {
            text-align: center;
            margin-top: 20px;
            padding-top: 10px;
            border-top: 2px dashed #000;
            font-size: 10px;
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="company-name">%3</div>
        <div class="company-info">%4</div>
        <div class="company-info">%5</div>
    </div>
    
    <div class="section">
        <div class="row">
            <span><b>%6:</b> %7</span>
        </div>
        <div class="row">
            <span><b>%8:</b> %9</span>
        </div>
        <div class="row">
            <span><b>%10:</b> %11</span>
        </div>
        <div class="row">
            <span><b>%12:</b> %13</span>
        </div>
    </div>
    
    <div class="section">
        <div class="section-title">%14</div>
        <table class="items-table">
            <thead>
                <tr>
                    <th>%15</th>
                    <th>%16</th>
                    <th>%17</th>
                    <th>%18</th>
                </tr>
            </thead>
            <tbody>
                %19
            </tbody>
        </table>
    </div>
    
    <div class="total-section">
        <div class="row">
            <span>%20:</span>
            <span>%21</span>
        </div>
        <div class="row">
            <span>%22:</span>
            <span>%23</span>
        </div>
        <div class="row">
            <span>%24:</span>
            <span>%25</span>
        </div>
        <div class="row grand-total">
            <span>%26:</span>
            <span>%27</span>
        </div>
    </div>
    
    <div class="footer">
        <p>%28</p>
        <p>%29</p>
    </div>
</body>
</html>
    )").arg(direction)
       .arg(align)
       .arg(companyName)
       .arg(companyPhone)
       .arg(companyAddress)
       .arg(LangManager::instance().t("Order #"))
       .arg(order.id())
       .arg(LangManager::instance().t("Date"))
       .arg(order.dateTime().toString("yyyy-MM-dd hh:mm"))
       .arg(LangManager::instance().t("Customer"))
       .arg(order.customerName())
       .arg(LangManager::instance().t("Session #"))
       .arg(session.id())
       .arg(LangManager::instance().t("Items"))
       .arg(LangManager::instance().t("Product"))
       .arg(LangManager::instance().t("Qty"))
       .arg(LangManager::instance().t("Price"))
       .arg(LangManager::instance().t("Total"));
    
    QString itemsHtml;
    for (const auto& item : order.items()) {
        itemsHtml += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
            .arg(item.productName)
            .arg(item.quantity)
            .arg(QString::number(item.unitPrice, 'f', 2))
            .arg(QString::number(item.unitPrice * item.quantity, 'f', 2));
    }
    
    QString currency = ConfigManager::instance().currencySymbol();
    
    html = html.arg(itemsHtml)
        .arg(LangManager::instance().t("Subtotal"))
        .arg(QString::number(order.subtotal(), 'f', 2) + " " + currency)
        .arg(LangManager::instance().t("Discount"))
        .arg(QString::number(order.discountAmount(), 'f', 2) + " " + currency)
        .arg(LangManager::instance().t("Delivery Fee"))
        .arg(QString::number(order.deliveryFee(), 'f', 2) + " " + currency)
        .arg(LangManager::instance().t("TOTAL"))
        .arg(QString::number(order.grandTotal(), 'f', 2) + " " + currency)
        .arg(LangManager::instance().t("Thank you for your business!"))
        .arg(LangManager::instance().t("Payment Method") + ": " + paymentMethod);
    
    return html;
}

bool ReceiptPrinter::print(const Order& order, const RegisterSession& session, 
                          const QString& paymentMethod) {
    QString html = generateHtml(order, session, paymentMethod);
    return printHtml(html, 80, QString());
}

bool ReceiptPrinter::printToFile(const Order& order, const RegisterSession& session,
                                 const QString& paymentMethod, const QString& filename) {
    QString html = generateHtml(order, session, paymentMethod);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << html;
    file.close();
    
    return true;
}
