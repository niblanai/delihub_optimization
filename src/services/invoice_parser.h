#pragma once
#include <QString>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// InvoiceItem — one product line extracted from an invoice (PDF or XLS).
// ─────────────────────────────────────────────────────────────────────────────
struct InvoiceItem {
    QString name;       // product name
    double  qty = 1.0;  // quantity (always positive)
};

// ─────────────────────────────────────────────────────────────────────────────
// InvoiceParser
//
// Pipeline (works with Telerik-generated PDFs that embed Arabic as CID fonts):
//   1. pdftoppm.exe  — render first PDF page to PNG at 300 DPI
//   2. magick.exe    — crop to table area, grayscale, sharpen
//   3. tesseract.exe — OCR with ara+eng, PSM 4
//   4. parseText()   — extract product name + quantity from OCR output
//
// All tools are expected next to DeliHub.exe (bundled in dist/) with MSYS2
// path as fallback during development.
// ─────────────────────────────────────────────────────────────────────────────
class InvoiceParser {
public:
    // Parse a PDF invoice (OCR pipeline)
    static QList<InvoiceItem> parse(const QString& pdfPath, QString& errorMsg);

    // Parse an XLS/XLSX invoice (Python xlrd pipeline)
    // Returns items directly from spreadsheet — no OCR, 100% accurate names.
    static QList<InvoiceItem> parseXls(const QString& xlsPath, QString& errorMsg);

private:
    static QList<InvoiceItem> parseText(const QString& ocrText);
    static QString            cleanOcrText(const QString& raw);
    static bool isProductRow(const QString& line);
    static bool isSkippedLine(const QString& line);
};
