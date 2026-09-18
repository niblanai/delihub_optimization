#include "ui/id_card_renderer.h"
#include "services/barcode_generator.h"

#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

namespace {

constexpr const char* kAccent     = "#10B981";
constexpr const char* kTextDark   = "#111827";
constexpr const char* kTextMuted  = "#6B7280";
constexpr const char* kPhotoFill  = "#E5E7EB";
constexpr const char* kHairline   = "#D1D5DB";

// Shrinks the font until the text fits the given width (keeps the card tidy
// no matter how long an employee name is).
void fitFont(QPainter& p, QFont& font, const QString& text, int maxWidth, int minPixel)
{
    while (font.pixelSize() > minPixel) {
        QFontMetrics fm(font, p.device());
        if (fm.horizontalAdvance(text) <= maxWidth) return;
        font.setPixelSize(font.pixelSize() - 1);
    }
}

} // namespace

namespace IdCardRenderer {

QImage render(const CardData& data, int dpi)
{
    dpi = qBound(72, dpi, 1200);
    const double pxPerMm = dpi / 25.4;
    auto mm = [pxPerMm](double v) { return static_cast<int>(qRound(v * pxPerMm)); };

    const int W = mm(kCardWidthMm);
    const int H = mm(kCardHeightMm);

    QImage card(W, H, QImage::Format_RGB32);
    card.fill(Qt::white);

    QPainter p(&card);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // ── Card outline ────────────────────────────────────────────────────────
    const int border = qMax(1, mm(0.5));
    QRectF frame(border / 2.0, border / 2.0, W - border, H - border);
    QPainterPath outline;
    outline.addRoundedRect(frame, mm(3), mm(3));
    p.fillPath(outline, Qt::white);
    p.setPen(QPen(QColor(kAccent), border));
    p.drawPath(outline);

    // ── Header strip ────────────────────────────────────────────────────────
    const int headerH = mm(8.0);
    if (!data.orgName.trimmed().isEmpty()) {
        p.save();
        p.setClipPath(outline);
        p.fillRect(QRect(0, 0, W, headerH), QColor(kAccent));
        QFont hf("Arial", -1, QFont::Bold);
        hf.setPixelSize(mm(3.4));
        p.setFont(hf);
        p.setPen(Qt::white);
        p.drawText(QRect(mm(4), 0, W - mm(8), headerH),
                   Qt::AlignVCenter | Qt::AlignLeft, data.orgName.trimmed());
        p.restore();
    }

    const int contentTop = (data.orgName.trimmed().isEmpty() ? mm(4.0) : headerH + mm(3.0));

    // ── Photo ───────────────────────────────────────────────────────────────
    const int photoX = mm(5.0);
    const int photoW = mm(19.0);
    const int photoH = mm(23.0);
    QRect photoRect(photoX, contentTop, photoW, photoH);

    QPainterPath photoClip;
    photoClip.addRoundedRect(photoRect, mm(1.5), mm(1.5));

    QPixmap photo;
    if (!data.photoPath.isEmpty() && QFile::exists(data.photoPath))
        photo.load(data.photoPath);

    if (!photo.isNull()) {
        p.save();
        p.setClipPath(photoClip);
        // Cover-crop so the face is never squashed.
        QPixmap scaled = photo.scaled(photoRect.size(), Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
        p.drawPixmap(photoRect.x() - (scaled.width()  - photoRect.width())  / 2,
                     photoRect.y() - (scaled.height() - photoRect.height()) / 2,
                     scaled);
        p.restore();
    } else {
        p.fillPath(photoClip, QColor(kPhotoFill));
        QFont pf("Arial");
        pf.setPixelSize(mm(9));
        p.setFont(pf);
        p.setPen(QColor(kTextMuted));
        p.drawText(photoRect, Qt::AlignCenter, QStringLiteral("?"));
    }
    p.setPen(QPen(QColor(kHairline), qMax(1, mm(0.25))));
    p.drawPath(photoClip);

    // ── Name / role / id ────────────────────────────────────────────────────
    const int infoX = photoRect.right() + mm(4.0);
    const int infoW = W - infoX - mm(5.0);
    int y = contentTop + mm(1.0);

    QFont nameFont("Arial", -1, QFont::Bold);
    nameFont.setPixelSize(mm(4.6));
    fitFont(p, nameFont, data.name, infoW, mm(3.0));
    p.setFont(nameFont);
    p.setPen(QColor(kTextDark));
    {
        QFontMetrics fm(nameFont, p.device());
        const QString elided = fm.elidedText(data.name, Qt::ElideRight, infoW);
        p.drawText(QRect(infoX, y, infoW, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, elided);
        y += fm.height() + mm(1.2);
    }

    QFont roleFont("Arial");
    roleFont.setPixelSize(mm(3.4));
    p.setFont(roleFont);
    p.setPen(QColor(kTextMuted));
    {
        QFontMetrics fm(roleFont, p.device());
        const QString elided = fm.elidedText(data.role, Qt::ElideRight, infoW);
        p.drawText(QRect(infoX, y, infoW, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, elided);
        y += fm.height() + mm(0.8);
    }

    if (!data.employeeId.trimmed().isEmpty()) {
        QFont idFont("Arial");
        idFont.setPixelSize(mm(3.0));
        p.setFont(idFont);
        p.setPen(QColor(kTextMuted));
        QFontMetrics fm(idFont, p.device());
        p.drawText(QRect(infoX, y, infoW, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, data.employeeId.trimmed());
    }

    // ── Barcode strip + human-readable code ─────────────────────────────────
    const int codeTextH   = mm(4.2);
    const int bottomPad   = mm(3.0);
    const int barHeight   = mm(9.0);
    const int barcodeW    = W - mm(10.0);
    const int barcodeX    = mm(5.0);
    const int barcodeY    = H - bottomPad - codeTextH - barHeight;

    const QImage bars = BarcodeGenerator::renderCode128FitWidth(
        data.barcode, barcodeW, barHeight, /*quietModules=*/10);

    if (!bars.isNull()) {
        // Centre it: fit-width rounds down, so it may be a few pixels narrower.
        const int bx = barcodeX + (barcodeW - bars.width()) / 2;
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);  // keep bars crisp
        p.drawImage(bx, barcodeY, bars);
        p.restore();
    } else {
        p.setPen(QColor("#EF4444"));
        QFont ef("Arial");
        ef.setPixelSize(mm(3.0));
        p.setFont(ef);
        p.drawText(QRect(barcodeX, barcodeY, barcodeW, barHeight),
                   Qt::AlignCenter, QStringLiteral("Invalid barcode value"));
    }

    // The code as text, so it can be typed in manually if the scanner fails.
    QFont codeFont("Courier New", -1, QFont::Bold);
    codeFont.setPixelSize(mm(3.2));
    codeFont.setLetterSpacing(QFont::AbsoluteSpacing, mm(0.35));
    fitFont(p, codeFont, data.barcode, barcodeW, mm(2.2));
    p.setFont(codeFont);
    p.setPen(QColor(kTextDark));
    p.drawText(QRect(barcodeX, barcodeY + barHeight, barcodeW, codeTextH),
               Qt::AlignCenter, data.barcode);

    p.end();
    return card;
}

} // namespace IdCardRenderer
