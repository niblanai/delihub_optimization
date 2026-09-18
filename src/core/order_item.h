#ifndef ORDER_ITEM_H
#define ORDER_ITEM_H

#include <QString>

struct OrderItem {
    int productId = 0;
    QString productName; // Helper field for UI display
    QString productBarcode; // for invoice display
    int quantity = 0;
    double unitPrice = 0.0;

    double totalPrice() const {
        return quantity * unitPrice;
    }
};

#endif // ORDER_ITEM_H
