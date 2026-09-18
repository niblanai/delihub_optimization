#ifndef INVOICE_GENERATOR_H
#define INVOICE_GENERATOR_H

#include "core/order.h"
#include "core/customer.h"
#include <QString>

class InvoiceGenerator {
public:
    // Generate a PDF invoice for the given order + customer.
    // Returns the path of the saved PDF, or empty string on failure.
    static QString generatePdf(const Order& order,
                                const Customer& customer,
                                const QString& outputPath = QString());

    // Build the HTML body used for both PDF and print preview
    static QString buildHtml(const Order& order,
                              const Customer& customer,
                              int invoiceNumber);

    // ── POS thermal receipt (80mm / 58mm roll printers) ─────────────────────
    // Separate from buildHtml()/generatePdf() above, which target a full A4
    // tax invoice. This is the compact receipt format for POS sales: no
    // logo/meta-grid, narrow single-column layout, and a REAL graphical
    // Code128 barcode (not just the printed text) under the invoice number.
    //
    // cashierName / registerLabel are passed in directly by the POS caller
    // rather than read off Order, since not every branch's Order data
    // necessarily carries a cashier/register reference yet.
    static QString buildThermalReceiptHtml(const Order& order,
                                            const Customer& customer,
                                            int invoiceNumber,
                                            const QString& cashierName = QString(),
                                            const QString& registerLabel = QString(),
                                            int paperWidthMm = 80);

    // Prints the thermal receipt directly, sized to paperWidthMm (58 or 80)
    // instead of falling back to the printer's default page (usually A4) —
    // that mismatch is what makes thermal receipts print at the wrong scale
    // and look broken.
    static bool printThermalReceipt(const Order& order,
                                     const Customer& customer,
                                     int invoiceNumber,
                                     const QString& printerName,
                                     const QString& cashierName = QString(),
                                     const QString& registerLabel = QString(),
                                     int paperWidthMm = 80,
                                     QString* errorOut = nullptr);
};

#endif // INVOICE_GENERATOR_H
