#include "core/receipt_template.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ReceiptTemplateNS {

QString blockTypeName(BlockType t) {
    switch (t) {
        case BlockType::Logo:          return "Logo";
        case BlockType::CompanyName:   return "CompanyName";
        case BlockType::Address:       return "Address";
        case BlockType::Phone:         return "Phone";
        case BlockType::InvoiceNumber: return "InvoiceNumber";
        case BlockType::DateTime:      return "DateTime";
        case BlockType::Cashier:       return "Cashier";
        case BlockType::ItemsTable:    return "ItemsTable";
        case BlockType::Totals:        return "Totals";
        case BlockType::Barcode:       return "Barcode";
        case BlockType::Text:          return "Text";
        case BlockType::Divider:       return "Divider";
        case BlockType::Spacer:        return "Spacer";
    }
    return "Text";
}

BlockType blockTypeFromName(const QString& name) {
    static const QMap<QString, BlockType> map = {
        {"Logo", BlockType::Logo}, {"CompanyName", BlockType::CompanyName},
        {"Address", BlockType::Address}, {"Phone", BlockType::Phone},
        {"InvoiceNumber", BlockType::InvoiceNumber}, {"DateTime", BlockType::DateTime},
        {"Cashier", BlockType::Cashier}, {"ItemsTable", BlockType::ItemsTable},
        {"Totals", BlockType::Totals}, {"Barcode", BlockType::Barcode},
        {"Text", BlockType::Text}, {"Divider", BlockType::Divider},
        {"Spacer", BlockType::Spacer},
    };
    return map.value(name, BlockType::Text);
}

QString blockTypeLabel(BlockType t) {
    switch (t) {
        case BlockType::Logo:          return "Company Logo";
        case BlockType::CompanyName:   return "Company Name";
        case BlockType::Address:       return "Company Address";
        case BlockType::Phone:         return "Company Phone";
        case BlockType::InvoiceNumber: return "Invoice Number";
        case BlockType::DateTime:      return "Date & Time";
        case BlockType::Cashier:       return "Cashier / Session";
        case BlockType::ItemsTable:    return "Items Table";
        case BlockType::Totals:        return "Totals";
        case BlockType::Barcode:       return "Barcode";
        case BlockType::Text:          return "Custom Text";
        case BlockType::Divider:       return "Divider Line";
        case BlockType::Spacer:        return "Blank Space";
    }
    return "Text";
}

QString blockTypeIcon(BlockType t) {
    switch (t) {
        case BlockType::Logo:          return "🏢";
        case BlockType::CompanyName:   return "📝";
        case BlockType::Address:       return "📍";
        case BlockType::Phone:         return "☎️";
        case BlockType::InvoiceNumber: return "🔢";
        case BlockType::DateTime:      return "📅";
        case BlockType::Cashier:       return "🧾";
        case BlockType::ItemsTable:    return "📋";
        case BlockType::Totals:        return "💰";
        case BlockType::Barcode:       return "📊";
        case BlockType::Text:          return "✏️";
        case BlockType::Divider:       return "➖";
        case BlockType::Spacer:        return "⬜";
    }
    return "❔";
}

QVariantMap defaultProperties(BlockType t) {
    QVariantMap p;
    switch (t) {
        case BlockType::Logo:
            p["heightMm"] = 15.0;
            break;
        case BlockType::CompanyName:
            p["fontSizePt"] = 16;
            p["bold"] = true;
            break;
        case BlockType::Phone:
            p["label"] = QStringLiteral("Tel");
            break;
        case BlockType::InvoiceNumber:
            p["label"] = QStringLiteral("Invoice #");
            break;
        case BlockType::DateTime:
            p["format"] = QStringLiteral("yyyy-MM-dd hh:mm");
            break;
        case BlockType::ItemsTable:
            p["showPrice"] = true;
            p["showQty"] = true;
            break;
        case BlockType::Totals:
            p["showDiscount"] = true;
            p["showDelivery"] = false;
            break;
        case BlockType::Barcode:
            p["heightMm"] = 12.0;
            p["showText"] = true;
            break;
        case BlockType::Text:
            p["text"] = QStringLiteral("Thank you for your business!");
            p["fontSizePt"] = 11;
            p["bold"] = false;
            p["align"] = QStringLiteral("center");
            break;
        case BlockType::Divider:
            p["style"] = QStringLiteral("dashed");
            break;
        case BlockType::Spacer:
            p["heightMm"] = 4.0;
            break;
        default:
            break;
    }
    return p;
}

QJsonDocument ReceiptTemplate::toJson() const {
    QJsonObject root;
    root["name"] = name;
    root["paperWidthMm"] = paperWidthMm;

    QJsonArray arr;
    for (const auto& b : blocks) {
        QJsonObject bo;
        bo["type"] = blockTypeName(b.type);
        bo["properties"] = QJsonObject::fromVariantMap(b.properties);
        arr.append(bo);
    }
    root["blocks"] = arr;
    return QJsonDocument(root);
}

ReceiptTemplate ReceiptTemplate::fromJson(const QJsonDocument& doc) {
    ReceiptTemplate tpl;
    if (!doc.isObject()) return defaultTemplate(80);

    const QJsonObject root = doc.object();
    tpl.name = root.value("name").toString("Custom");
    tpl.paperWidthMm = root.value("paperWidthMm").toInt(80);

    for (const QJsonValue& v : root.value("blocks").toArray()) {
        const QJsonObject bo = v.toObject();
        ReceiptBlock b;
        b.type = blockTypeFromName(bo.value("type").toString());
        b.properties = bo.value("properties").toObject().toVariantMap();
        tpl.blocks.append(b);
    }
    return tpl;
}

ReceiptTemplate ReceiptTemplate::fromJson(const QString& jsonText) {
    if (jsonText.trimmed().isEmpty()) return defaultTemplate(80);
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return defaultTemplate(80);
    return fromJson(doc);
}

ReceiptTemplate ReceiptTemplate::defaultTemplate(int paperWidthMm) {
    ReceiptTemplate tpl;
    tpl.name = (paperWidthMm <= 58) ? "Default 58mm" : "Default 80mm";
    tpl.paperWidthMm = (paperWidthMm <= 58) ? 58 : 80;

    auto add = [&](BlockType t, QVariantMap overrides = {}) {
        ReceiptBlock b;
        b.type = t;
        b.properties = defaultProperties(t);
        for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it)
            b.properties[it.key()] = it.value();
        tpl.blocks.append(b);
    };

    add(BlockType::CompanyName);
    add(BlockType::Phone);
    add(BlockType::Divider);
    add(BlockType::InvoiceNumber);
    add(BlockType::DateTime);
    add(BlockType::Cashier);
    add(BlockType::Divider);
    add(BlockType::ItemsTable);
    add(BlockType::Divider);
    add(BlockType::Totals);
    add(BlockType::Barcode);
    add(BlockType::Spacer);
    add(BlockType::Text, {{"text", QStringLiteral("Thank you for your business!")}, {"bold", true}});

    return tpl;
}

} // namespace ReceiptTemplateNS
