#pragma once

#include <QImage>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Renders a compact employee ID card (CR80 / credit-card size: 85.6 x 54 mm)
// containing photo, name, role, a real Code 128 barcode strip at the bottom,
// and the barcode value printed as text underneath for manual entry.
//
// The same function feeds both the on-screen preview and the printer, so what
// the operator sees is exactly what comes out of the printer.
// ─────────────────────────────────────────────────────────────────────────────
namespace IdCardRenderer {

struct CardData {
    QString name;
    QString role;
    QString employeeId;     // e.g. "ID 0042" — optional, drawn under the role
    QString barcode;        // the fingerprint barcode value
    QString photoPath;      // may be empty or missing on disk
    QString orgName;        // header line, e.g. company name — optional
};

// Physical card size in millimetres.
constexpr double kCardWidthMm  = 85.6;
constexpr double kCardHeightMm = 54.0;

// dpi: use 300 (or printer.resolution()) for printing, ~160 for preview.
QImage render(const CardData& data, int dpi);

} // namespace IdCardRenderer
