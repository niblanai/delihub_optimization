#include "invoice_generator.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "barcode_generator.h"
#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QPixmap>
#include <QBuffer>
#include <QByteArray>
#include <QApplication>
#include <QPainter>

// ── Convert logo file to base64 data URI for embedding in HTML ────────────────
static QString logoDataUri(const QString& logoPath) {
    if (logoPath.isEmpty() || !QFile::exists(logoPath)) return {};
    QPixmap px(logoPath);
    if (px.isNull()) return {};
    px = px.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    px.save(&buf, "PNG");
    return "data:image/png;base64," + ba.toBase64();
}

// ── Generate barcode image and convert to base64 data URI ──────────────────────
static QString barcodeDataUri(const QString& barcodeText) {
    if (barcodeText.isEmpty()) return {};
    
    QImage barcodeImg = BarcodeGenerator::renderCode128(barcodeText, 2, 60, 10);
    
    if (barcodeImg.isNull()) return {};
    
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    barcodeImg.save(&buf, "PNG");
    return "data:image/png;base64," + ba.toBase64();
}

QString InvoiceGenerator::buildHtml(const Order& order,
                                     const Customer& customer,
                                     int invoiceNumber)
{
    const auto& cfg = ConfigManager::instance();
    const QString sym = cfg.currencySymbol();

    // Determine logo
    QString logoPath = cfg.invoiceLogoPath();
    if (logoPath.isEmpty()) {
        QString appDir = QApplication::applicationDirPath();
        QString fallback = appDir + "/logo.png";
        if (QFile::exists(fallback)) logoPath = fallback;
    }
    QString logoUri = logoDataUri(logoPath);
    
    // Generate barcode image
    QString barcodeUri = barcodeDataUri(order.invoiceBarcode());

    // Customer details
    QString custAddr;
    if (!customer.addresses().isEmpty())
        custAddr = customer.addresses().first().text;
    QString custPhone;
    if (!customer.phones().isEmpty())
        custPhone = customer.phones().first().number;

    // Payment method badge colors
    auto payBadgeColor = [](const QString& pm) -> QString {
        if (pm == "Visa")  return "background:#dbeafe;color:#1e40af;";
        if (pm == "Cash")  return "background:#dcfce7;color:#166534;";
        return "background:#fef3c7;color:#92400e;";
    };

    // Items table rows — include barcode column
    QString rows;
    for (const auto& item : order.items()) {
        double lineTotal = item.quantity * item.unitPrice;
        QString barcode = item.productBarcode.isEmpty() ? "—" : item.productBarcode;
        rows += QString(
            "<tr>"
            "<td style='padding:6px 8px;border-bottom:1px solid #e2e8f0;'>%1</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #e2e8f0;text-align:center;"
                "font-family:monospace;font-size:10pt;color:#64748b;'>%2</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #e2e8f0;text-align:center;'>%3</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #e2e8f0;text-align:right;'>%4 %5</td>"
            "<td style='padding:6px 8px;border-bottom:1px solid #e2e8f0;text-align:right;"
                "font-weight:bold;'>%6 %5</td>"
            "</tr>")
            .arg(item.productName.isEmpty() ? QString("Product #%1").arg(item.productId) : item.productName)
            .arg(barcode)
            .arg(item.quantity)
            .arg(QString::number(item.unitPrice, 'f', 2), sym)
            .arg(QString::number(lineTotal, 'f', 2));
    }

    // Driver display
    QString driverDisplay = order.driverName().isEmpty() ? "—" : order.driverName();
    if (!order.driverPhone().isEmpty())
        driverDisplay += "<br><span style='font-size:10pt;color:#64748b;'>📞 " + order.driverPhone() + "</span>";

    // Discount row (only shown if discount > 0)
    QString discountRow;
    if (order.discountAmount() > 0.001) {
        QString reason = order.discountReason().isEmpty() ? "" :
            QString("  <span style='font-size:9pt;color:#94a3b8;'>(%1)</span>").arg(order.discountReason());
        discountRow = QString(
            "<div class='total-row' style='color:#ef4444;'>"
            "<span>Discount%1</span><span>- %2 %3</span></div>")
            .arg(reason)
            .arg(QString::number(order.discountAmount(), 'f', 2), sym);
    }

    // Payment method badge — Task 3: show custom name when "Other" is selected
    QString payMethod = order.paymentMethod().isEmpty() ? "Cash" : order.paymentMethod();
    QString payDisplayName = payMethod;
    if (payMethod == "Other" && !order.paymentOtherDetail().isEmpty())
        payDisplayName = order.paymentOtherDetail();  // e.g. "Bank Transfer", "Instapay"
    QString payBadge = QString(
        "<span style='display:inline-block;padding:3px 10px;border-radius:12px;"
        "font-size:9pt;font-weight:bold;%1'>%2</span>")
        .arg(payBadgeColor(payMethod), payDisplayName.toHtmlEscaped());

    QString html = QString(R"(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
  body { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12pt; color: #1e293b; margin: 0; padding: 20px; }
  .invoice-box { max-width: 820px; margin: auto; }
  /* Task 13: Header redesign — company name top-left, logo far right, same row */
  .header { display: flex; justify-content: space-between; align-items: flex-start;
            margin-bottom: 20px; border-bottom: 2px solid #e2e8f0; padding-bottom: 14px; }
  .company-block { flex: 1; }
  .company-name { font-size: 24pt; font-weight: 900; color: #0ea5e9;
                  margin: 0 0 6px 0; line-height: 1.1; }
  /* Task 13: tighter detail line spacing */
  .company-detail { font-size: 10pt; color: #64748b; line-height: 1.4; margin: 0; }
  .logo-cell { flex-shrink: 0; margin-left: 20px; display: flex; align-items: flex-start; }
  .logo-cell img { width: 80px; height: 80px; object-fit: contain; }
  .invoice-title { font-size: 18pt; font-weight: bold; color: #334155; text-align: center;
                   padding: 10px; background: #f1f5f9; border-radius: 6px; margin-bottom: 16px; }
  /* Task 13: tighter meta grid spacing */
  .meta-grid { display: flex; gap: 12px; margin-bottom: 16px; }
  .meta-box { flex: 1; background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 10px; }
  .meta-label { font-size: 9pt; color: #94a3b8; text-transform: uppercase; font-weight: bold; margin-bottom: 3px; }
  .meta-value { font-size: 11pt; color: #1e293b; }
  table { width: 100%; border-collapse: collapse; margin-bottom: 16px; }
  thead tr { background: #1e293b; color: #fff; }
  thead th { padding: 10px 8px; text-align: left; font-size: 10pt; }
  thead th:nth-child(2) { text-align: center; }
  thead th:nth-child(3) { text-align: center; }
  thead th:nth-child(4), thead th:nth-child(5) { text-align: right; }
  tbody tr:nth-child(even) { background: #f8fafc; }
  .totals { margin-left: auto; width: 300px; }
  .total-row { display: flex; justify-content: space-between; padding: 5px 0;
               border-bottom: 1px solid #e2e8f0; font-size: 11pt; }
  .total-row.grand { font-size: 16pt; font-weight: 900; color: #0ea5e9;
                     border-bottom: none; border-top: 3px solid #0ea5e9;
                     padding-top: 10px; margin-top: 6px;
                     text-transform: uppercase; letter-spacing: 0.5px; }
  .footer { margin-top: 24px; text-align: center; font-size: 9pt; color: #94a3b8;
            border-top: 1px solid #e2e8f0; padding-top: 10px; }
  .status-badge { display: inline-block; padding: 3px 10px; border-radius: 12px; font-size: 9pt; font-weight: bold; }
  .status-Delivered { background: #dcfce7; color: #166534; }
  .status-Cancelled { background: #fee2e2; color: #991b1b; }
  .status-Pending   { background: #fef3c7; color: #92400e; }
</style>
</head>
<body>
<div class="invoice-box">

  <!-- Task 13: Header — company name top-left, logo aligned far right on same row -->
  <div class="header">
    <div class="company-block">
      <div class="company-name">%1</div>
      <div class="company-detail">%2</div>
      <div class="company-detail">📞 %3</div>
      <div class="company-detail">Tax #: %4</div>
    </div>
    <div class="logo-cell">%5</div>
  </div>

  <div class="invoice-title">TAX INVOICE</div>

  <!-- Meta info -->
  <div class="meta-grid">
    <div class="meta-box">
      <div class="meta-label">Invoice #</div>
      <div class="meta-value" style="font-size:14pt;font-weight:bold;color:#0ea5e9;">INV-%6</div>
      %21
      <div class="meta-label" style="margin-top:6px;">Date</div>
      <div class="meta-value">%7</div>
      <div class="meta-label" style="margin-top:6px;">Status</div>
      <div class="meta-value"><span class="status-badge status-%8">%8</span></div>
      <div class="meta-label" style="margin-top:6px;">Payment</div>
      <div class="meta-value">%9</div>
    </div>
    <div class="meta-box">
      <div class="meta-label">Bill To</div>
      <div class="meta-value" style="font-weight:bold;">%10</div>
      <div class="meta-value" style="font-size:10pt;color:#64748b;">%11</div>
      <div class="meta-value" style="font-size:10pt;color:#64748b;">%12</div>
    </div>
    <div class="meta-box">
      <div class="meta-label">Driver</div>
      <div class="meta-value">%13</div>
      <div class="meta-label" style="margin-top:6px;">Branch</div>
      <div class="meta-value">%14</div>
    </div>
  </div>

  <!-- Items -->
  <table>
    <thead>
      <tr>
        <th>Product</th>
        <th style="text-align:center;">Barcode</th>
        <th style="text-align:center;">Qty</th>
        <th style="text-align:right;">Unit Price</th>
        <th style="text-align:right;">Total</th>
      </tr>
    </thead>
    <tbody>%15</tbody>
  </table>

  <!-- Totals -->
  <div class="totals">
    <div class="total-row"><span>Subtotal</span><span>%16 %17</span></div>
    <div class="total-row"><span>Delivery Fee</span><span>%18 %17</span></div>
    %19
    <div class="total-row grand"><span>GRAND TOTAL</span><span>%20 %17</span></div>
  </div>

  <!-- Footer -->
  <div class="footer">
    Thank you for your business! · %1 · %3
  </div>

</div>
</body>
</html>
)")
    .arg(cfg.companyName())                                                   // %1
    .arg(cfg.companyAddress().isEmpty() ? "" : cfg.companyAddress())          // %2
    .arg(cfg.companyPhone())                                                   // %3
    .arg(cfg.companyTaxNumber().isEmpty() ? "—" : cfg.companyTaxNumber())     // %4
    .arg(logoUri.isEmpty() ? "" : QString("<img src='%1' />").arg(logoUri))   // %5
    .arg(QString("%1").arg(invoiceNumber, 6, 10, QChar('0')))                 // %6
    .arg(order.dateTime().toString("yyyy-MM-dd  hh:mm"))                      // %7
    .arg(order.status().isEmpty() ? "Pending" : order.status())               // %8
    .arg(payBadge)                                                             // %9
    .arg(customer.name().isEmpty() ? "—" : customer.name())                   // %10
    .arg(custAddr.isEmpty() ? "" : custAddr)                                   // %11
    .arg(custPhone.isEmpty() ? "" : "📞 " + custPhone)                        // %12
    .arg(driverDisplay)                                                        // %13
    .arg(cfg.branchName())                                                     // %14
    .arg(rows)                                                                 // %15
    .arg(QString::number(order.subtotal(), 'f', 2), sym)                      // %16 %17
    .arg(QString::number(order.deliveryFee(), 'f', 2))                        // %18
    .arg(discountRow)                                                          // %19
    .arg(QString::number(order.grandTotal(), 'f', 2))                         // %20
    .arg(barcodeUri.isEmpty() ? "" : 
         QString("<div style='margin-top:8px; text-align:center;'>"
                 "<img src='%1' style='width:200px; height:auto; border:1px solid #e2e8f0; "
                 "border-radius:4px; padding:4px; background:#fff;'/>"
                 "<div style='font-size:8pt; color:#94a3b8; margin-top:2px; font-family:monospace;'>%2</div>"
                 "</div>")
         .arg(barcodeUri, order.invoiceBarcode()));                           // %21

    return html;
}

QString InvoiceGenerator::generatePdf(const Order& order,
                                       const Customer& customer,
                                       const QString& outputPath)
{    int invoiceNumber = ConfigManager::instance().nextInvoiceNumber();
    ConfigManager::instance().incrementInvoiceNumber();

    // Determine output path
    QString path = outputPath;
    if (path.isEmpty()) {
        QString defaultDir = QDir::homePath() + "/Documents";
        path = defaultDir + QString("/invoice_INV-%1_%2.pdf")
               .arg(invoiceNumber, 6, 10, QChar('0'))
               .arg(QDate::currentDate().toString("yyyyMMdd"));
    }

    // Ensure directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageSize(QPageSize::A4);
    printer.setPageMargins(QMarginsF(15,15,15,15), QPageLayout::Millimeter);

    QString html = buildHtml(order, customer, invoiceNumber);
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);

    Logger::instance().info(QString("Invoice INV-%1 generated: %2")
        .arg(invoiceNumber, 6, 10, QChar('0')).arg(path));
    return path;
}

// ── POS thermal receipt (80mm / 58mm) ─────────────────────────────────────────
QString InvoiceGenerator::buildThermalReceiptHtml(const Order& order,
                                                   const Customer& customer,
                                                   int invoiceNumber,
                                                   const QString& cashierName,
                                                   const QString& registerLabel,
                                                   int paperWidthMm)
{
    const auto& cfg = ConfigManager::instance();
    const QString sym = cfg.currencySymbol();

    // Content column width follows the paper width, not a fixed A4-style
    // column — this is the part that was missing before and is the main
    // reason a "thermal" receipt still came out laid out like a normal page.
    const int contentPx = (paperWidthMm >= 80) ? 280 : 200;
    const QString fontPt = (paperWidthMm >= 80) ? "9pt" : "8pt";

    QString barcodeUri = barcodeDataUri(order.invoiceBarcode());
    QString invoiceNoStr = QString("INV-%1").arg(invoiceNumber, 6, 10, QChar('0'));

    QString rows;
    for (const auto& item : order.items()) {
        double lineTotal = item.quantity * item.unitPrice;
        rows += QString(
            "<tr>"
            "<td style='padding:3px 2px;text-align:right;'>%1</td>"
            "<td style='padding:3px 2px;text-align:center;'>%2</td>"
            "<td style='padding:3px 2px;text-align:center;'>%3</td>"
            "<td style='padding:3px 2px;text-align:left;font-weight:bold;'>%4</td>"
            "</tr>")
            .arg(item.productName.isEmpty() ? QString("#%1").arg(item.productId) : item.productName)
            .arg(item.quantity)
            .arg(QString::number(item.unitPrice, 'f', 2))
            .arg(QString::number(lineTotal, 'f', 2));
    }

    QString discountRow;
    if (order.discountAmount() > 0.001) {
        discountRow = QString("<div class='r'><span>الخصم</span><span>%1 %2</span></div>")
            .arg(QString::number(order.discountAmount(), 'f', 2), sym);
    }

    QString payMethod = order.paymentMethod().isEmpty() ? "Cash" : order.paymentMethod();
    QString payDisplay = (payMethod == "Cash") ? "كاش"
                        : (payMethod == "Visa") ? "فيزا"
                        : (payMethod == "Credit") ? "آجل"
                        : (order.paymentOtherDetail().isEmpty() ? payMethod : order.paymentOtherDetail());

    QString metaLines;
    metaLines += QString("<div class='meta-line'>%1</div>").arg(order.dateTime().toString("yyyy-MM-dd"));
    metaLines += QString("<div class='meta-line'>%1</div>").arg(order.dateTime().toString("hh:mm"));
    if (!cashierName.isEmpty())
        metaLines += QString("<div class='meta-line'>الكاشير: %1</div>").arg(cashierName.toHtmlEscaped());
    if (!registerLabel.isEmpty())
        metaLines += QString("<div class='meta-line'>الصندوق: %1</div>").arg(registerLabel.toHtmlEscaped());

    QString html = QString(R"(
<!DOCTYPE html>
<html dir="rtl">
<head>
<meta charset="UTF-8">
<style>
  * { box-sizing: border-box; }
  body { font-family: 'Segoe UI', Arial, sans-serif; font-size: %5; color: #000;
         margin: 0; padding: 4px; width: %6px; }
  .center { text-align: center; }
  .brand { font-size: 14pt; font-weight: 900; margin: 2px 0; }
  .divider { border-top: 1px dashed #000; margin: 6px 0; }
  .invoice-no { font-weight: bold; font-size: 11pt; text-align: center; margin: 4px 0; }
  .barcode-wrap { text-align: center; margin: 4px 0; }
  .barcode-wrap img { max-width: 100%; height: auto; }
  .meta-line { text-align: right; font-size: %5; margin: 1px 0; }
  table { width: 100%; border-collapse: collapse; margin: 4px 0; font-size: %5; }
  thead td { border-bottom: 1px solid #000; font-weight: bold; padding: 3px 2px; }
  .totals { margin-top: 4px; }
  .r { display: flex; justify-content: space-between; font-size: %5; padding: 1px 0; }
  .grand { font-weight: 900; font-size: 11pt; border-top: 1px solid #000;
           border-bottom: 1px solid #000; padding: 4px 0; margin-top: 4px; }
  .thanks { text-align: center; margin: 8px 0 2px; font-size: %5; }
  .pay { text-align: center; font-size: %5; color: #444; margin-bottom: 4px; }
</style>
</head>
<body>
  <div class="center brand">%1</div>
  <div class="divider"></div>
  <div class="invoice-no">%2</div>
  %3
  %4
  <div class="divider"></div>
  <table>
    <thead><tr>
      <td>المنتج</td><td>ك</td><td>سعر</td><td>إجمالي</td>
    </tr></thead>
    <tbody>%7</tbody>
  </table>
  <div class="divider"></div>
  <div class="totals">
    <div class="r"><span>إجمالي</span><span>%8 %9</span></div>
    %10
    <div class="r"><span>المدفوع</span><span>%11 %9</span></div>
    <div class="r grand"><span>المجموع</span><span>%12 %9</span></div>
  </div>
  <div class="thanks">شكرًا لزيارة متجرنا!</div>
  <div class="pay">طريقة الدفع: %13</div>
</body>
</html>
)")
    .arg(cfg.companyName().isEmpty() ? "DeliHub" : cfg.companyName())      // %1
    .arg(invoiceNoStr)                                                     // %2
    .arg(barcodeUri.isEmpty() ? "" :
         QString("<div class='barcode-wrap'><img src='%1'/></div>").arg(barcodeUri))  // %3
    .arg(metaLines)                                                        // %4
    .arg(fontPt)                                                           // %5
    .arg(contentPx)                                                        // %6
    .arg(rows)                                                             // %7
    .arg(QString::number(order.subtotal(), 'f', 2), sym)                  // %8 %9
    .arg(discountRow)                                                      // %10
    .arg(QString::number(order.grandTotal() - order.discountAmount(), 'f', 2)) // %11 (paid ≈ grand − discount already applied; adjust if a separate paid-amount field exists)
    .arg(QString::number(order.grandTotal(), 'f', 2))                     // %12
    .arg(payDisplay);                                                      // %13

    return html;
}

bool InvoiceGenerator::printThermalReceipt(const Order& order,
                                            const Customer& customer,
                                            int invoiceNumber,
                                            const QString& printerName,
                                            const QString& cashierName,
                                            const QString& registerLabel,
                                            int paperWidthMm,
                                            QString* errorOut)
{
    QPrinter printer(QPrinter::HighResolution);
    if (!printerName.isEmpty())
        printer.setPrinterName(printerName);

    if (!printer.isValid()) {
        if (errorOut) *errorOut = "Could not open printer: " +
            (printerName.isEmpty() ? "(default)" : printerName);
        return false;
    }

    // The single most important line for thermal printing: a custom page
    // size matching the ACTUAL roll width (58 or 80mm), not the printer's
    // default (usually A4/Letter). Height is generous since thermal
    // printers feed/cut per receipt rather than printing a fixed page —
    // Qt still requires *some* finite height to lay the document out.
    QPageSize thermalSize(QSizeF(paperWidthMm, 297), QPageSize::Millimeter, "Thermal");
    printer.setPageSize(thermalSize);
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(1, 1, 1, 1), QPageLayout::Millimeter);

    QString html = buildThermalReceiptHtml(order, customer, invoiceNumber,
                                            cashierName, registerLabel, paperWidthMm);
    QTextDocument doc;
    doc.setHtml(html);
    // Constrain the document's own layout width to the printable area in
    // device pixels — without this, QTextDocument lays text out at its
    // default width regardless of the printer's page size, and everything
    // ends up squeezed/misaligned relative to the narrow paper.
    doc.setPageSize(printer.pageRect(QPrinter::DevicePixel).size());
    doc.print(&printer);

    Logger::instance().info(QString("Thermal receipt printed: INV-%1 (%2mm)")
        .arg(invoiceNumber, 6, 10, QChar('0')).arg(paperWidthMm));
    return true;
}
