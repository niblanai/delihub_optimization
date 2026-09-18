#ifndef RECEIPT_PRINTER_H
#define RECEIPT_PRINTER_H

#include <QString>
#include <QStringList>
#include "core/order.h"
#include "core/register_session.h"
#include "core/receipt_template.h"

class ReceiptPrinter {
public:
    // Legacy fixed layout — kept so any existing caller keeps compiling.
    static bool print(const Order& order, const RegisterSession& session, const QString& paymentMethod);
    static bool printToFile(const Order& order, const RegisterSession& session, 
                           const QString& paymentMethod, const QString& filename);

    // Template-driven rendering — this is what the designer and the POS now use.
    // Builds HTML sized exactly to template.paperWidthMm, so what you see in the
    // designer preview is the same layout that reaches the printer.
    static QString generateHtmlFromTemplate(const Order& order, const RegisterSession& session,
                                            const QString& paymentMethod,
                                            const ReceiptTemplateNS::ReceiptTemplate& tpl);

    // Prints pre-built HTML at the template's physical paper width.
    // printerName empty -> shows the OS print dialog; otherwise prints silently
    // to that printer (used by "Test Print" and normal POS checkout once a
    // default thermal printer is configured).
    static bool printHtml(const QString& html, int paperWidthMm,
                          const QString& printerName, QWidget* dialogParent = nullptr);

    static bool printWithTemplate(const Order& order, const RegisterSession& session,
                                  const QString& paymentMethod,
                                  const ReceiptTemplateNS::ReceiptTemplate& tpl,
                                  const QString& printerName, QWidget* dialogParent = nullptr);

    // Renders the template with sample data (no real Order/RegisterSession
    // needed) — used for the live preview in the receipt designer.
    static QString generatePreviewHtml(const ReceiptTemplateNS::ReceiptTemplate& tpl);

    // Names of printers Qt can see (QPrinterInfo::availablePrinters()).
    // Not filtered to "thermal" specifically — Windows/CUPS don't reliably expose
    // that as a queryable property — but the label makes the intent clear and any
    // printer that accepts a custom small page size works fine for a receipt.
    static QStringList getAvailableThermalPrinters();

private:
    static QString generateHtml(const Order& order, const RegisterSession& session, 
                               const QString& paymentMethod);
};

#endif // RECEIPT_PRINTER_H
