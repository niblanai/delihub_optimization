#ifndef PROMOTION_H
#define PROMOTION_H

#include <QString>
#include <QDateTime>

struct Promotion {
    enum class Type {
        Percentage,    // خصم نسبة مئوية
        FixedAmount,   // خصم مبلغ ثابت
        BuyXGetY,      // اشتري X واحصل على Y
        FreeShipping   // شحن مجاني
    };
    
    enum class Status {
        Active,
        Inactive,
        Scheduled,
        Expired
    };
    
    int id = 0;
    QString title;
    QString description;
    Type type = Type::Percentage;
    double discountValue = 0.0;
    QDateTime startDate;
    QDateTime endDate;
    Status status = Status::Inactive;
    QString imagePath;
    int productId = 0;      // 0 = كل المنتجات
    int categoryId = 0;     // 0 = كل الفئات
    double minPurchaseAmount = 0.0;
};

#endif // PROMOTION_H
