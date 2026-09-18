#ifndef PRODUCT_SUPPLIER_H
#define PRODUCT_SUPPLIER_H

#include <QString>

struct ProductSupplier {
    int productId = 0;
    int supplierId = 0;
    double purchasePrice = 0.0;
    bool isPreferred = false;
    QString lastPurchaseDate;
    
    // Populated from joins (not stored in ProductSuppliers table)
    QString supplierName;
    QString productName;
};

#endif // PRODUCT_SUPPLIER_H
