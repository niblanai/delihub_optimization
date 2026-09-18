#ifndef MISSING_PRODUCT_H
#define MISSING_PRODUCT_H

#include <QString>
#include <QDate>

struct MissingProduct {
    int id = 0;
    int productId = 0;
    QString productName;
    QString barcode;
    int quantityNeeded = 0;
    QString unitLabel;   // Issues 4&5: "كرتونة" / "علبة" / "قطعة" etc.
    int branchId = 0;
    QDate dateAdded;
    bool purchased = false;

    // "manual"  — user-entered request (never auto-removed)
    // "auto_low_stock" — created automatically when qty <= safety margin
    QString source = "manual";

    // Populated only for auto_low_stock entries — the product's real current stock
    int currentQty = 0;
};

#endif // MISSING_PRODUCT_H
