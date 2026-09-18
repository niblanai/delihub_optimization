#pragma once

#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Shared matching logic for fingerprint barcodes (format: FP-<epoch>-<rand>).
//
// WHY THIS EXISTS
// A hardware scanner and a human keyboard do NOT produce the same string:
//   - scanners often append CR, LF, or a Tab suffix
//   - some scanner profiles emit lowercase, or a non-breaking space
//   - a human may type "fp-1789423193-7454", add spaces, or use a different
//     dash character (– instead of -) depending on keyboard layout
//   - an Arabic keyboard layout emits Arabic-Indic digits (٠١٢…) for the
//     number row
// A raw `stored == typed` comparison fails on every one of those, which is
// exactly why manual entry "never works" while scanning does.
//
// Every place that authenticates a card MUST go through matches() so the
// behaviour is identical in POS, login and the card-test tab.
// ─────────────────────────────────────────────────────────────────────────────
namespace BarcodeAuth {

// Canonical form: control chars and whitespace removed, Arabic-Indic digits
// folded to ASCII, unicode dashes folded to '-', uppercased.
QString normalize(const QString& raw);

// Digits only — lets an operator type just "17894231937454" off the card
// instead of the full "FP-1789423193-7454".
QString digitsOnly(const QString& raw);

// True when 'entered' identifies the same card as 'stored'.
// Accepts the full code, a differently-cased version, or the digits alone.
bool matches(const QString& stored, const QString& entered);

// True if the string looks like a plausible card code at all (used to give a
// better error message than a flat "invalid barcode").
bool looksLikeCardCode(const QString& raw);

} // namespace BarcodeAuth
