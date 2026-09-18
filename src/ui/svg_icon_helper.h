#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SvgIconHelper — theme-aware icon loading.
//
// SVGs from svgrepo.com use either fill="..." or stroke="..." (or both) to
// define their color.  This helper replaces ALL color-carrying attributes
// with the desired theme color so icons always match the sidebar navText.
//
// Rendering:  Qt6Svg's QSvgRenderer (requires qsvg.dll in imageformats/).
// Fallback:   empty QIcon — caller should handle gracefully.
// ─────────────────────────────────────────────────────────────────────────────
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QFile>
#include <QByteArray>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QSvgRenderer>
#include "../infra/config_manager.h"

struct SvgIconHelper {

    // ── Theme-aware icon loading ──────────────────────────────────────────────
    // Automatically determines icon color based on svgIconMode setting:
    //   - "light" mode: black icons (#000000)
    //   - "dark" mode:  white icons (#FFFFFF)
    //   - custom hex:   specified color (e.g. "#FF5733")
    // Use this for UI icons that should respect the global icon mode.
    static QIcon icon(const QString& path, int size = 18) {
        QColor color = getIconColor();
        return icon(path, color, size);
    }

    // ── Manual color override ─────────────────────────────────────────────────
    // Load SVG from a file path, replace ALL strokes/fills with `color`,
    // render at `size` logical pixels (2× for HiDPI), return QIcon.
    // Use this when you need a specific color regardless of icon mode.
    static QIcon icon(const QString& path, const QColor& color, int size = 18) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QByteArray svg = f.readAll();
        f.close();

        recolorSvg(svg, color);

        QSvgRenderer renderer(svg);
        if (!renderer.isValid()) return {};

        const int px = size * 2;   // 2× for HiDPI
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p, QRectF(0, 0, px, px));
        p.end();
        pm.setDevicePixelRatio(2.0);
        return QIcon(pm);
    }

    // ── Get icon color based on current svgIconMode setting ───────────────────
    static QColor getIconColor() {
        QString mode = ConfigManager::instance().svgIconMode();
        
        if (mode == "light") {
            return QColor("#000000");  // Black icons for light mode
        } else if (mode == "dark") {
            return QColor("#FFFFFF");  // White icons for dark mode
        } else {
            // Custom color (hex string like "#FF5733")
            if (mode.startsWith("#") && (mode.length() == 7 || mode.length() == 9)) {
                return QColor(mode);
            }
            // Fallback to black if invalid format
            return QColor("#000000");
        }
    }

private:
    // ── Replace every color attribute in the SVG with the desired color ───────
    // Handles:
    //   fill="#XXXXXX"         stroke="#XXXXXX"
    //   fill="black"           stroke="black"
    //   fill="white"           stroke="white"
    //   fill:#XXXXXX           stroke:#XXXXXX   (inside style="")
    //   fill:black             stroke:black
    //   CSS class block:       .classname{fill:#XXXXXX;}
    // Does NOT replace fill="none" or stroke="none" (transparent areas kept).
    static void recolorSvg(QByteArray& svg, const QColor& color) {
        const QByteArray hex = color.name(QColor::HexRgb).toUtf8(); // e.g. "#8a8fbf"

        // ── CSS <style> block — replace fill:#XXXXXX inside CSS ───────────────
        // Pattern: .classname{fill:#XXXXXX;} or fill:#XXXXXX in <style> block
        {
            // Replace any fill:#XXXXXX (6 hex digits) in CSS
            QByteArray result;
            result.reserve(svg.size());
            int pos = 0;
            static const QByteArray cssFill   = "fill:#";
            static const QByteArray cssStroke  = "stroke:#";
            while (pos < svg.size()) {
                // Look for fill:# or stroke:#
                int fi = svg.indexOf(cssFill,   pos);
                int si = svg.indexOf(cssStroke, pos);
                int next = -1;
                QByteArray prop;
                if (fi >= 0 && (si < 0 || fi <= si)) { next = fi; prop = cssFill; }
                else if (si >= 0)                     { next = si; prop = cssStroke; }
                if (next < 0) { result.append(svg.mid(pos)); break; }
                result.append(svg.mid(pos, next - pos));
                result.append(prop);
                result.append(hex.mid(1)); // skip the leading # since prop already has it
                // Skip old color value (3 or 6 hex digits, plus optional alpha = up to 8)
                int colorStart = next + prop.size();
                int colorEnd   = colorStart;
                while (colorEnd < svg.size() && colorEnd < colorStart + 8 &&
                       isxdigit(svg[colorEnd])) ++colorEnd;
                pos = colorEnd;
            }
            svg = result;
        }

        // ── Attribute-style replacements ──────────────────────────────────────
        replaceAttrColor(svg, "fill",   hex);
        replaceAttrColor(svg, "stroke", hex);

        // Named colors
        for (const QByteArray& named : {"black", "white", "#000", "#fff",
                                         "#000000", "#ffffff", "#030819",
                                         "#1a1a1a", "#333", "#333333",
                                         "#111918", "#0B1719", "#0b1719"}) {
            svg.replace("fill=\""   + named + "\"", "fill=\""   + hex + "\"");
            svg.replace("stroke=\"" + named + "\"", "stroke=\"" + hex + "\"");
            // Inside style=""
            svg.replace("fill:"   + named + ";", "fill:"   + hex + ";");
            svg.replace("fill:"   + named + "}", "fill:"   + hex + "}");
            svg.replace("stroke:" + named + ";", "stroke:" + hex + ";");
            svg.replace("stroke:" + named + "}", "stroke:" + hex + "}");
        }
    }

    // Replace fill="#XXXXXX" or stroke="#XXXXXX" (any hex color) with new hex.
    // Skips fill="none" and stroke="none".
    static void replaceAttrColor(QByteArray& svg,
                                  const QByteArray& attr,
                                  const QByteArray& newHex)
    {
        // We iterate manually to avoid replacing "none"
        QByteArray search = attr + "=\"#";
        int pos = 0;
        while ((pos = svg.indexOf(search, pos)) != -1) {
            int start = pos + search.size();          // points at first hex digit
            int end   = svg.indexOf('"', start);
            if (end < 0) break;
            QByteArray oldColor = svg.mid(start - 1, end - start + 2); // "#XXXXXX"
            // Don't touch "none"
            if (oldColor.toLower() != "\"none\"") {
                svg.replace(pos, end - pos + 1,
                            attr + "=\"" + newHex + "\"");
                pos += attr.size() + 3 + newHex.size(); // skip past replacement
            } else {
                pos = end + 1;
            }
        }
    }
};
