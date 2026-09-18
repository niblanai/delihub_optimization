#include "services/barcode_auth.h"

#include <QChar>

namespace BarcodeAuth {

QString normalize(const QString& raw)
{
    QString out;
    out.reserve(raw.size());

    for (QChar ch : raw) {
        const ushort u = ch.unicode();

        // Drop everything a scanner or keyboard may add around the payload:
        // CR, LF, Tab, NUL, spaces, non-breaking space.
        if (ch.isSpace() || u < 32 || u == 0x00A0) continue;

        // Arabic-Indic (٠-٩) and Eastern Arabic-Indic (۰-۹) digits -> ASCII.
        if (u >= 0x0660 && u <= 0x0669) { out.append(QChar('0' + (u - 0x0660))); continue; }
        if (u >= 0x06F0 && u <= 0x06F9) { out.append(QChar('0' + (u - 0x06F0))); continue; }

        // Any unicode dash variant (en/em dash, minus sign, Arabic tatweel)
        // folds to the plain hyphen the code is generated with.
        if (u == 0x2010 || u == 0x2011 || u == 0x2012 || u == 0x2013 ||
            u == 0x2014 || u == 0x2015 || u == 0x2212 || u == 0x0640) {
            out.append(QLatin1Char('-'));
            continue;
        }

        out.append(ch.toUpper());
    }
    return out;
}

QString digitsOnly(const QString& raw)
{
    const QString norm = normalize(raw);
    QString out;
    out.reserve(norm.size());
    for (QChar ch : norm)
        if (ch.isDigit()) out.append(ch);
    return out;
}

bool matches(const QString& stored, const QString& entered)
{
    const QString a = normalize(stored);
    const QString b = normalize(entered);
    if (a.isEmpty() || b.isEmpty()) return false;

    if (a == b) return true;

    // Fall back to digits-only so the operator can type the numbers off the
    // card without the "FP-" prefix or the dashes.
    const QString da = digitsOnly(stored);
    const QString db = digitsOnly(entered);
    return !da.isEmpty() && da.size() >= 8 && da == db;
}

bool looksLikeCardCode(const QString& raw)
{
    return digitsOnly(raw).size() >= 8;
}

} // namespace BarcodeAuth
