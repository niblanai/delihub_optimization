#include "services/invoice_parser.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
#include <QtMath>

// ─────────────────────────────────────────────────────────────────────────────
// Arabic character check — MUST use \x{HHHH} not \uHHHH.
// Qt uses PCRE which requires hex escapes; \u0600 in a raw C++ string literal
// is a literal backslash-u-0600, NOT a Unicode code point.
// ─────────────────────────────────────────────────────────────────────────────
static bool hasArabic(const QString& s) {
    for (const QChar& c : s) {
        ushort u = c.unicode();
        if (u >= 0x0600 && u <= 0x06FF) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Locate a tool exe: first next to DeliHub.exe (dist/), then MSYS2 fallback.
// ─────────────────────────────────────────────────────────────────────────────
static QString findTool(const QString& name) {
    QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(name),
        "C:/msys64/ucrt64/bin/" + name
    };
    for (const QString& c : candidates)
        if (QFile::exists(c)) return c;
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────
QList<InvoiceItem> InvoiceParser::parse(const QString& pdfPath, QString& errorMsg) {
    // Pipeline:
    //   1. pdftoppm  — convert PDF page to PNG at 300 DPI
    //   2. magick    — crop to table area + grayscale + sharpen
    //   3. tesseract — OCR with Arabic, PSM 4
    //   4. parseText — extract name + qty

    // ── Step 1: PDF → PNG ────────────────────────────────────────────────────
    QString pdftoppm = findTool("pdftoppm.exe");
    if (pdftoppm.isEmpty()) {
        errorMsg = "pdftoppm.exe not found next to DeliHub.exe.";
        return {};
    }

    QString tmpDir  = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString uid     = QUuid::createUuid().toString(QUuid::Id128).left(8);
    QString imgBase = tmpDir + "/dh_inv_" + uid;
    QString imgFile = imgBase + "-1.png";

    QProcess ppm;
    // Ensure the application directory is in PATH so pdftoppm can find its DLLs
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", QCoreApplication::applicationDirPath() + ";" + env.value("PATH"));
    ppm.setProcessEnvironment(env);
    ppm.start(pdftoppm, { "-r", "300", "-png", "-f", "1", "-l", "1", pdfPath, imgBase });
    if (!ppm.waitForFinished(20000) || ppm.exitCode() != 0) {
        errorMsg = "pdftoppm failed: " + QString::fromUtf8(ppm.readAllStandardError());
        return {};
    }
    if (!QFile::exists(imgFile)) {
        errorMsg = "pdftoppm did not produce output image.";
        return {};
    }

    // ── Step 2: Enhance with ImageMagick ────────────────────────────────────
    // Create a shared environment with appDir in PATH for all child processes
    QProcessEnvironment toolEnv = QProcessEnvironment::systemEnvironment();
    toolEnv.insert("PATH", QCoreApplication::applicationDirPath() + ";" + toolEnv.value("PATH"));

    QString magick  = findTool("magick.exe");
    QString procImg = imgFile;
    if (!magick.isEmpty()) {
        QString enhanced = imgBase + "_enh.png";
        QProcess mg;
        mg.setProcessEnvironment(toolEnv);
        mg.start(magick, { imgFile,
            "-colorspace", "Gray",
            "-contrast-stretch", "0",
            "-sharpen", "0x1.5",
            "-crop", "2481x1400+0+450", "+repage",
            enhanced });
        if (mg.waitForFinished(15000) && mg.exitCode() == 0 && QFile::exists(enhanced))
            procImg = enhanced;
    }

    // ── Step 3: Tesseract OCR ────────────────────────────────────────────────
    QString tesseract = findTool("tesseract.exe");
    if (tesseract.isEmpty()) {
        errorMsg = "tesseract.exe not found next to DeliHub.exe.";
        QFile::remove(imgFile);
        return {};
    }
    QString tessdata;
    for (const QString& c : QStringList{
            QDir(QCoreApplication::applicationDirPath()).filePath("tessdata"),
            "C:/msys64/ucrt64/share/tessdata" })
        if (QDir(c).exists()) { tessdata = c; break; }

    QString ocrBase = imgBase + "_ocr";
    // Arabic-only (-l ara) gives the cleanest product names.
    // Using ara+eng causes Tesseract to convert some Arabic characters to
    // Latin letters (e.g. "بلاك" → "aa", "كوكاو" gets eaten), because the
    // engine mixes both scripts. Numbers and barcodes are read correctly
    // even with Arabic-only since digits are universal.
    // PSM 4 (single column) works best for these vertical receipt layouts.
    QStringList tessArgs = { procImg, ocrBase,
        "-l", "ara",    // Arabic only — cleanest Arabic product names
        "--psm", "4",   // single-column receipt
        "--oem", "1"    // LSTM engine
    };
    if (!tessdata.isEmpty()) tessArgs << "--tessdata-dir" << tessdata;

    QProcess tess;
    tess.setProcessEnvironment(toolEnv);
    tess.start(tesseract, tessArgs);
    if (!tess.waitForFinished(30000) || tess.exitCode() != 0) {
        errorMsg = "tesseract failed: " + QString::fromUtf8(tess.readAllStandardError());
        QFile::remove(imgFile);
        return {};
    }

    // ── Step 4: Read OCR text ────────────────────────────────────────────────
    QFile ocrFile(ocrBase + ".txt");
    if (!ocrFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Cannot read tesseract output.";
        QFile::remove(imgFile);
        return {};
    }
    QTextStream ts(&ocrFile);
    ts.setEncoding(QStringConverter::Utf8);
    QString ocrText = ts.readAll();
    ocrFile.close();

    // Cleanup
    QFile::remove(imgFile);
    if (procImg != imgFile) QFile::remove(procImg);
    QFile::remove(ocrBase + ".txt");

    if (ocrText.trimmed().isEmpty()) {
        errorMsg = "OCR produced no output.";
        return {};
    }

    return parseText(cleanOcrText(ocrText));
}

// ─────────────────────────────────────────────────────────────────────────────
// cleanOcrText
// ─────────────────────────────────────────────────────────────────────────────
QString InvoiceParser::cleanOcrText(const QString& raw) {
    // Remove BiDi + zero-width control characters
    static const QRegularExpression reBiDi(
        "[\u200E\u200F\u202A\u202B\u202C\u202D\u202E\u2066\u2067\u2068\u2069"
        "\u200B\u200C\u200D\uFEFF]");

    // Pure punctuation/symbol standalone tokens to drop
    static const QRegularExpression reNoiseToken(
        R"(^[>«»•\*\+\-\|"'`^~\\/<>{}()\u060C\u061F!@#$%^&_=\u061B\u060C]+$)");

    QStringList cleanLines;
    for (const QString& rawLine : raw.split('\n')) {
        QString line = rawLine;

        // Remove BiDi marks
        line.remove(reBiDi);
        line = line.trimmed();
        if (line.isEmpty()) { cleanLines.append(""); continue; }

        // Remove isolated Latin noise tokens in Arabic lines
        // (≤4 chars, no digits, no decimal) — e.g. "KS", "SDL", "cls", "ar"
        if (hasArabic(line)) {
            QStringList tokens = line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
            QStringList cleaned;
            for (const QString& t : tokens) {
                bool hasAr    = hasArabic(t);
                bool hasDigit = t.contains(QRegularExpression(R"(\d)"));
                bool hasDot   = t.contains('.');
                bool isLatinNoise = (!hasAr && !hasDigit && !hasDot
                                     && t.length() <= 4
                                     && QRegularExpression(R"(^[A-Za-z#@*/\\+\-|‎‏]+$)").match(t).hasMatch());
                bool isSymNoise   = reNoiseToken.match(t).hasMatch();
                if (!isLatinNoise && !isSymNoise) cleaned.append(t);
            }
            line = cleaned.join(' ').trimmed();
        }

        cleanLines.append(line);
    }
    return cleanLines.join('\n');
}

// ─────────────────────────────────────────────────────────────────────────────
// parseText
//
// Invoice layout (3 numbers at end of each product line):
//   [اسم الصنف]  [كمية]  [سعر الوحدة]  [إجمالى]
//
// OCR may mangle qty=1.00 → "00" or "100".
// Numbers inside product name (e.g. "19" in "جالون 19 لتر", "200" in "استيك 200") are preserved.
// Only DECIMAL numbers (X.XX format) at the END of the line are treated as price/qty columns.
// Pure-integer tokens embedded in the name stay as part of the name.
//
// Price format variants handled:
//   5.00   88.60   1,092.00  (comma thousands separator → strip comma)
//   ]26.00  ]130.00 (OCR noise prefix)
//   00  100  (OCR-mangled 1.00 → "00" or "100") — only recognized after a decimal
// ─────────────────────────────────────────────────────────────────────────────
QList<InvoiceItem> InvoiceParser::parseText(const QString& text) {
    QList<InvoiceItem> items;

    const QStringList lines = text.split('\n');
    bool inTable = false;

    // Matches a token that is a DECIMAL number (has a dot or comma+dot):
    //   optional noise prefix + digits + "." + digits + optional noise suffix
    static const QRegularExpression reDecimal(
        R"(^[\]\[|/\\]?[\d,]+(\.[\d,]+)[\]\[|/\\]?$)");
    // Pure integer (no decimal point)
    static const QRegularExpression reDigitsOnly(R"(^[\d,]+$)");
    // Noise characters to strip before parsing
    static const QRegularExpression reNoise(R"([\]\[|/\\])");

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        // ── Detect table header ──────────────────────────────────────────
        if (!inTable) {
            if (line.contains(QString::fromUtf8("إسم الصنف")) ||
                line.contains(QString::fromUtf8("اسم الصنف")) ||
                line.contains(QString::fromUtf8("الصنف")) ||
                line.contains(QString::fromUtf8("كمية"))) {
                inTable = true;
            }
            continue;
        }

        // ── Must contain Arabic ──────────────────────────────────────────
        if (!hasArabic(line)) continue;

        // ── Footer stop ──────────────────────────────────────────────────
        if (isSkippedLine(line)) {
            if (!items.isEmpty()) break;
            continue;
        }

        // ── Tokenize ─────────────────────────────────────────────────────
        const QStringList tokens = line.split(
            QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);

        struct Tok {
            QString raw;
            bool    isDecimal;   // true = X.XX format (price/qty/total column)
            bool    isInteger;   // true = pure digits (might be in name OR mangled qty)
            double  val;
        };
        QList<Tok> toks;

        for (const QString& t : tokens) {
            Tok tok;
            tok.raw       = t;
            tok.isDecimal = false;
            tok.isInteger = false;
            tok.val       = 0;

            QString clean = t;
            clean.remove(reNoise);
            // Handle thousand-separator comma: "1,092.00" → "1092.00"
            // But not Arabic decimal comma "," alone without a dot
            if (clean.contains('.')) {
                // Remove ALL commas (thousand separators)
                clean.remove(',');
            }
            clean = clean.trimmed();

            bool ok = false;
            double v = clean.toDouble(&ok);

            if (ok && v >= 0 && clean.contains('.')) {
                // Has a decimal point → proper price/qty token
                tok.isDecimal = true;
                tok.val = v;
            } else if (reDigitsOnly.match(clean).hasMatch() && !clean.isEmpty()) {
                // Pure integer — could be in product name (like "200" or "19")
                // or a mangled OCR qty. We classify as integer only; the
                // trailing-block logic will decide.
                tok.isInteger = true;
                tok.val = clean.toDouble();
            }
            toks.append(tok);
        }

        // ── Find trailing DECIMAL block (walk from end) ──────────────────
        // ONLY decimal tokens form the trailing price/qty/total columns.
        // A SINGLE leading integer is accepted ONLY if it looks like a mangled
        // qty (small value: 1, 2, 3, ..., up to ~99).
        // Product name parts like "200" or "300" (packaging sizes) stay in name.
        int lastNameIdx = toks.size() - 1;
        QList<double> trail;

        // Walk backwards collecting only decimal tokens first
        for (int i = toks.size() - 1; i >= 0; --i) {
            if (toks[i].isDecimal) {
                trail.prepend(toks[i].val);
                lastNameIdx = i - 1;
            } else {
                break;  // stop at first non-decimal
            }
        }

        // Check if the token just before the decimal block is an integer
        // that looks like a mangled qty (very small: 1-20, no Arabic chars)
        // Product packaging sizes like "100", "200", "300" stay in the name.
        if (!trail.isEmpty() && lastNameIdx >= 0) {
            const Tok& candidate = toks[lastNameIdx];
            if (candidate.isInteger && candidate.val >= 1 && candidate.val <= 20
                && !hasArabic(candidate.raw)
                && !candidate.raw.contains('.')) {
                // Looks like a realistic qty (1-20) — add to trail
                trail.prepend(candidate.val);
                lastNameIdx--;
            }
        }

        if (trail.isEmpty()) continue;

        // ── Build name from tokens BEFORE the trailing numbers ───────────
        QString name;
        for (int i = 0; i <= lastNameIdx; ++i) {
            if (!name.isEmpty()) name += ' ';
            name += toks[i].raw;
        }
        name = name.simplified();
        if (name.isEmpty() || !hasArabic(name)) continue;

        // ── Determine quantity ───────────────────────────────────────────
        // Layout: [qty][unit_price][total]
        // trail[0] = qty, trail[1] = price, trail[2] = total
        // OCR mangle cases:
        //   1.00 → "00" (val=0) or "100" (large integer, price is smaller)
        double qty = 1.0;
        if (trail.size() >= 3) {
            double candidate = trail[0];
            double price     = trail[1];
            // Mangled qty: integer ≥50 AND clearly not a qty (price << candidate)
            bool mangled = (candidate >= 50.0
                            && candidate == qFloor(candidate)
                            && price < candidate * 0.5);
            qty = mangled ? 1.0 : (candidate > 0 ? candidate : 1.0);
        } else if (trail.size() == 2) {
            // Could be [price, total] (qty mangled) or [qty, price]
            if (trail[0] > 0 && trail[0] < trail[1] * 0.5 && trail[0] < 50.0)
                qty = trail[0];
            else
                qty = 1.0;
        } else {
            qty = 1.0;  // only total survived
        }
        if (qty <= 0 || qty > 9999) qty = 1.0;

        items.append({ name, qty });
    }

    return items;
}

// ─────────────────────────────────────────────────────────────────────────────
// parseXls — read product name + qty directly from .xls spreadsheet
// ─────────────────────────────────────────────────────────────────────────────
QList<InvoiceItem> InvoiceParser::parseXls(const QString& xlsPath, QString& errorMsg) {
    // ALWAYS use the bundled python.exe next to DeliHub.exe first.
    // Never rely on system Python — it may not have xlrd and causes
    // "xlrd not installed" errors on fresh machines.
    const QString bundledPy = QDir(QCoreApplication::applicationDirPath()).filePath("python.exe");

    QStringList pyCandidates = {
        bundledPy,                              // bundled — always first
        "C:/msys64/ucrt64/bin/python.exe",      // MSYS2 dev fallback
        "C:/Python314/python.exe",
        "C:/Python313/python.exe",
        "C:/Python312/python.exe",
        "C:/Python311/python.exe"
        // NOTE: intentionally NOT adding `where python.exe` — that would
        // pick up the user's system Python which may not have xlrd.
    };

    QString pyExe;
    for (const QString& c : pyCandidates)
        if (QFile::exists(c)) { pyExe = c; break; }

    if (pyExe.isEmpty()) {
        errorMsg = "Python غير مثبت. لم يتم العثور على python.exe بجانب DeliHub.exe.";
        return {};
    }

    // Locate xls_reader script
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList scriptCandidates = {
        QDir(appDir).filePath("xls_reader.pyz"),
        QDir(appDir).filePath("xls_reader.py"),
        QDir(appDir + "/../").filePath("xls_reader.pyz"),
        QDir(appDir + "/../").filePath("xls_reader.py"),
        "E:/tifany/xls_reader.pyz",
        "E:/tifany/xls_reader.py"
    };
    QString scriptArg;
    for (const QString& c : scriptCandidates)
        if (QFile::exists(c)) { scriptArg = c; break; }

    if (scriptArg.isEmpty()) {
        errorMsg = "xls_reader.pyz not found next to DeliHub.exe.";
        return {};
    }

    // Skip xlrd check — bundled Python always has xlrd embedded in xls_reader.pyz

    QProcessEnvironment toolEnv = QProcessEnvironment::systemEnvironment();
    toolEnv.insert("PATH", appDir + ";" + toolEnv.value("PATH"));

    QProcess proc;
    proc.setProcessEnvironment(toolEnv);
    proc.start(pyExe, { scriptArg, xlsPath });
    if (!proc.waitForFinished(15000)) {
        errorMsg = "XLS reader timed out.";
        return {};
    }
    if (proc.exitCode() != 0) {
        errorMsg = "XLS reader failed: " + QString::fromUtf8(proc.readAllStandardError());
        return {};
    }

    QByteArray jsonData = proc.readAllStandardOutput().trimmed();
    QByteArray stderrData = proc.readAllStandardError().trimmed();
    
    if (jsonData.isEmpty()) {
        if (!stderrData.isEmpty()) {
            errorMsg = "XLS reader error: " + QString::fromUtf8(stderrData);
        } else {
            errorMsg = "لم يتم العثور على منتجات في ملف Excel.";
        }
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        QString debugInfo = QString::fromUtf8(jsonData.left(200));
        if (!stderrData.isEmpty()) {
            debugInfo += " | stderr: " + QString::fromUtf8(stderrData.left(200));
        }
        errorMsg = "XLS reader returned invalid JSON: " + debugInfo;
        return {};
    }

    QList<InvoiceItem> items;
    for (const QJsonValue& v : doc.array()) {
        QJsonObject obj = v.toObject();
        QString name = obj["name"].toString().trimmed();
        double  qty  = obj["qty"].toDouble(1.0);
        if (!name.isEmpty() && qty > 0)
            items.append({ name, qty });
    }

    if (items.isEmpty())
        errorMsg = "لم يتم العثور على منتجات في ملف Excel.";

    return items;
}

// ─────────────────────────────────────────────────────────────────────────────
bool InvoiceParser::isProductRow(const QString& line) {
    static const QRegularExpression reNumber(R"(\d+[\.,]\d+)");
    return hasArabic(line) && reNumber.match(line).hasMatch();
}

bool InvoiceParser::isSkippedLine(const QString& line) {
    // Footer/header lines that mark end of product table.
    // Do NOT include product lines like "خدمة دليفرى".
    static const QStringList stop = {
        "\u0625\u062c\u0645\u0627\u0644\u064a \u0639\u0627\u0645",   // إجمالي عام
        "\u0627\u062c\u0645\u0627\u0644\u064a \u0639\u0627\u0645",   // اجمالي عام
        "\u0625\u062c\u0645\u0627\u0644\u0649 \u0639\u0627\u0645",   // إجمالى عام
        "\u0627\u0644\u0645\u062f\u0641\u0648\u0639",                // المدفوع
        "\u0627\u0644\u0645\u062a\u0628\u0642\u0649",                // المتبقى
        "\u0627\u0644\u0645\u062a\u0628\u0642\u064a",                // المتبقي
        "\u0641\u0642\u0637",                                         // فقط
        "\u0645\u0628\u0644\u063a \u0648 \u0642\u062f\u0631\u0647",  // مبلغ و قدره
        "\u0627\u0644\u0633\u062c\u0644 \u0627\u0644\u062a\u062c\u0627\u0631\u064a", // السجل التجاري
        "\u0627\u0644\u0628\u0637\u0627\u0642\u0629 \u0627\u0644\u0636\u0631\u064a\u0628\u064a\u0629", // البطاقة الضريبية
        "\u0633\u064a\u0627\u0633\u0629 \u0627\u0644\u0625\u0631\u062c\u0627\u0639", // سياسة الإرجاع
    };
    for (const QString& kw : stop)
        if (line.contains(kw)) return true;
    return false;
}
