#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QJsonDocument>

// ─────────────────────────────────────────────────────────────────────────────
// A receipt is an ordered stack of blocks. Each block renders one horizontal
// strip of the receipt (logo, a line of text, the items table, the barcode...).
// The designer lets the operator add/remove/reorder blocks and edit each
// block's properties; ReceiptPrinter turns the finished list into HTML.
//
// This intentionally is NOT one C++ class per block type. A dozen near-empty
// subclasses (LogoPlugin, PhoneNumberPlugin, DividerPlugin...) would need a
// factory, a registry, and a virtual paintEvent/toHtml each — for blocks that
// are otherwise a type tag plus 1-3 properties. A single struct with a
// QVariantMap payload gives the exact same "drag a block in, edit its
// properties" experience with a fraction of the code to maintain, and adding
// a 13th block type later is a `case` label, not five new files.
// ─────────────────────────────────────────────────────────────────────────────
namespace ReceiptTemplateNS {

enum class BlockType {
    Logo,
    CompanyName,
    Address,
    Phone,
    InvoiceNumber,
    DateTime,
    Cashier,
    ItemsTable,
    Totals,
    Barcode,
    Text,
    Divider,
    Spacer,
};

QString blockTypeName(BlockType t);        // machine name, stored in JSON
QString blockTypeLabel(BlockType t);       // human label for the toolbox
QString blockTypeIcon(BlockType t);        // one emoji for the toolbox/list
BlockType blockTypeFromName(const QString& name);

// Default property values for a freshly-added block of this type.
QVariantMap defaultProperties(BlockType t);

struct ReceiptBlock {
    BlockType type = BlockType::Text;
    QVariantMap properties;
};

struct ReceiptTemplate {
    QString name = "Custom";
    int paperWidthMm = 80;             // 80 or 58
    QVector<ReceiptBlock> blocks;

    QJsonDocument toJson() const;
    static ReceiptTemplate fromJson(const QJsonDocument& doc);
    static ReceiptTemplate fromJson(const QString& jsonText);

    // Ready-to-use starting point for a given paper width.
    static ReceiptTemplate defaultTemplate(int paperWidthMm);
};

} // namespace ReceiptTemplateNS
