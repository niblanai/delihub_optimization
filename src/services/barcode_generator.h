#pragma once

#include <QImage>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Code 128 barcode generator (real black & white bars).
//
// Backend selection:
//   - If ZXING_AVAILABLE is defined at compile time, ZXing-C++ does the encoding.
//   - Otherwise a self-contained Code 128 (Code Set B) encoder is used.
// Both produce identical, scanner-readable output, so you can add ZXing later
// without touching any calling code.
// ─────────────────────────────────────────────────────────────────────────────
namespace BarcodeGenerator {

// Returns the raw module pattern: one char per module, '1' = bar, '0' = space.
// Empty string if the text cannot be encoded.
QString encodeCode128(const QString& text);

// Renders the barcode as a pure black/white image.
//   moduleWidth  : pixel width of one narrow module (>= 2 for screen, >= 3 for print)
//   barHeight    : pixel height of the bars
//   quietModules : blank margin on each side, in modules (10 minimum per spec)
// Returns a null QImage on failure.
QImage renderCode128(const QString& text,
                     int moduleWidth  = 2,
                     int barHeight    = 70,
                     int quietModules = 10);

// Renders the barcode scaled to fit exactly targetWidth pixels while keeping
// bar edges aligned to whole pixels (prevents blurry / unscannable bars).
QImage renderCode128FitWidth(const QString& text,
                             int targetWidth,
                             int barHeight,
                             int quietModules = 10);

// True if the text can be encoded (Code 128 Set B covers ASCII 32..126).
bool isEncodable(const QString& text);

// Strips/replaces characters Code 128 Set B cannot represent.
QString sanitize(const QString& text);

} // namespace BarcodeGenerator
