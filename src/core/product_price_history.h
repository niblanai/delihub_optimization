#ifndef PRODUCT_PRICE_HISTORY_H
#define PRODUCT_PRICE_HISTORY_H

#include <QDateTime>

struct ProductPriceHistory {
    int productId = 0;
    double price = 0.0;
    QDateTime effectiveDate;
};

#endif // PRODUCT_PRICE_HISTORY_H
