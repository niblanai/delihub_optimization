#include "services/barcode_generator.h"

#include <QPainter>
#include <QVector>

#ifdef ZXING_AVAILABLE
  #include <ZXing/BarcodeFormat.h>
  #include <ZXing/BitMatrix.h>
  #include <ZXing/MultiFormatWriter.h>
  #include <string>
#endif

namespace {

// Code 128 symbol table: 107 entries (values 0..106).
// Each entry lists the widths of 6 alternating elements starting with a bar
// (the stop symbol has 7). Total = 11 modules per symbol, 13 for stop.
static const char* kCode128Patterns[107] = {
    "212222","222122","222221","121223","121322","131222","122213","122312","132212","221213",
    "221312","231212","112232","122132","122231","113222","123122","123221","223211","221132",
    "221231","213212","223112","312131","311222","321122","321221","312212","322112","322211",
    "212123","212321","232121","111323","131123","131321","112313","132113","132311","211313",
    "231113","231311","112133","112331","132131","113123","113321","133121","313121","211331",
    "231131","213113","213311","213131","311123","311321","331121","312113","312311","332111",
    "314111","221411","431111","111224","111422","121124","121421","141122","141221","112214",
    "112412","122114","122411","142112","142211","241211","221114","413111","241112","134111",
    "111242","121142","121241","114212","124112","124211","411212","421112","421211","212141",
    "214121","412121","111143","111341","131141","114113","114311","411113","411311","113141",
    "114131","311141","411131","211412","211214","211232","2331112"
};

constexpr int kStartCodeB = 104;
constexpr int kStopCode   = 106;

// Built-in encoder — Code Set B (ASCII 32..126), which covers letters, digits,
// '-' and every character the FP-xxxxx-xxxx format uses.
QString encodeBuiltin(const QString& text)
{
    if (text.isEmpty()) return {};

    QVector<int> values;
    values.reserve(text.size() + 4);
    values.append(kStartCodeB);

    for (const QChar& ch : text) {
        const ushort u = ch.unicode();
        if (u < 32 || u > 126) return {};          // not representable in Set B
        values.append(static_cast<int>(u) - 32);
    }

    // Checksum: start value + sum(position * value), positions start at 1.
    long long sum = kStartCodeB;
    for (int i = 1; i < values.size(); ++i)
        sum += static_cast<long long>(i) * values[i];
    values.append(static_cast<int>(sum % 103));
    values.append(kStopCode);

    QString modules;
    modules.reserve(values.size() * 11 + 2);
    for (int v : values) {
        const char* pattern = kCode128Patterns[v];
        bool isBar = true;                          // every symbol starts with a bar
        for (const char* p = pattern; *p; ++p) {
            const int width = *p - '0';
            modules.append(QString(width, isBar ? QLatin1Char('1') : QLatin1Char('0')));
            isBar = !isBar;
        }
    }
    return modules;
}

#ifdef ZXING_AVAILABLE
// ZXing backend — asks for the minimum-width symbol, then we read it column by
// column so we control the exact module width ourselves (crisp, whole-pixel bars).
QString encodeZXing(const QString& text)
{
    try {
        auto writer = ZXing::MultiFormatWriter(ZXing::BarcodeFormat::Code128).setMargin(0);
        const ZXing::BitMatrix matrix = writer.encode(text.toStdString(), 1, 1);

        QString modules;
        modules.reserve(matrix.width());
        for (int x = 0; x < matrix.width(); ++x)
            modules.append(matrix.get(x, 0) ? QLatin1Char('1') : QLatin1Char('0'));
        return modules;
    } catch (...) {
        return {};                                  // fall back to the built-in encoder
    }
}
#endif

} // namespace

namespace BarcodeGenerator {

bool isEncodable(const QString& text)
{
    if (text.isEmpty()) return false;
    for (const QChar& ch : text) {
        const ushort u = ch.unicode();
        if (u < 32 || u > 126) return false;
    }
    return true;
}

QString sanitize(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar& ch : text) {
        const ushort u = ch.unicode();
        out.append((u >= 32 && u <= 126) ? ch : QLatin1Char('-'));
    }
    return out;
}

QString encodeCode128(const QString& text)
{
    const QString clean = sanitize(text);
    if (clean.isEmpty()) return {};

#ifdef ZXING_AVAILABLE
    const QString viaZXing = encodeZXing(clean);
    if (!viaZXing.isEmpty()) return viaZXing;
#endif
    return encodeBuiltin(clean);
}

QImage renderCode128(const QString& text, int moduleWidth, int barHeight, int quietModules)
{
    const QString modules = encodeCode128(text);
    if (modules.isEmpty()) return {};

    moduleWidth  = qMax(1, moduleWidth);
    barHeight    = qMax(4, barHeight);
    quietModules = qMax(0, quietModules);

    const int totalModules = modules.size() + quietModules * 2;
    const int imageWidth   = totalModules * moduleWidth;

    QImage image(imageWidth, barHeight, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    // Draw consecutive '1' modules as a single rectangle — fewer, cleaner edges.
    int x = quietModules * moduleWidth;
    int i = 0;
    while (i < modules.size()) {
        if (modules.at(i) == QLatin1Char('1')) {
            int run = 0;
            while (i + run < modules.size() && modules.at(i + run) == QLatin1Char('1')) ++run;
            painter.drawRect(x, 0, run * moduleWidth, barHeight);
            x += run * moduleWidth;
            i += run;
        } else {
            x += moduleWidth;
            ++i;
        }
    }
    painter.end();
    return image;
}

QImage renderCode128FitWidth(const QString& text, int targetWidth, int barHeight, int quietModules)
{
    const QString modules = encodeCode128(text);
    if (modules.isEmpty() || targetWidth <= 0) return {};

    const int totalModules = modules.size() + qMax(0, quietModules) * 2;
    // Round DOWN so the barcode never overflows, but never below 1px per module.
    const int moduleWidth  = qMax(1, targetWidth / totalModules);

    return renderCode128(text, moduleWidth, barHeight, quietModules);
}

} // namespace BarcodeGenerator
