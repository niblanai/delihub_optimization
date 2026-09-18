# نظام مجلة العروض - DeliHub Magazine System

## المشكلة الحالية

المجلة بتتحفظ صح في قاعدة البيانات، لكن POS **مش بيلاقي المجلة النشطة** وبالتالي مش بيطبق سعر المجلة.

### الأعراض:
- ✅ المجلة بتتحفظ في database (Promotions table)
- ✅ المنتجات بتتحفظ مع أسعار المجلة (PromotionProducts table)
- ❌ POS بيعرض السعر الأصلي بدل سعر المجلة
- ❌ الشريط الأحمر 🏷️ مش بيظهر على كارد المنتج

### المتوقع:
- منتج "قلبة زيت 1 لتر": السعر الأصلي 89 جنيه
- سعر المجلة: **150 جنيه**
- المفروض يظهر في POS: **171 جنيه** (150 + ضريبة 14%)
- الفعلي: بيظهر **101.46 جنيه** (89 + ضريبة)

---

## الملفات المرفقة

### 1. Core (النماذج الأساسية)
- `src/core/promotion.h` - تعريف Promotion struct

### 2. Data Layer (قاعدة البيانات)
- `src/data/irepositories.h` - IPromotionRepository interface
- `src/data/sqlite_repositories.h/cpp` - SQLite implementation
- `src/data/access_repositories.h/cpp` - Access/PostgreSQL implementation

### 3. UI Layer
**Products Page:**
- `src/ui/products/products_page.h/cpp` - صفحة المنتجات (فيها tab المجلات)
- `src/ui/products/magazine_dialog.h/cpp` - نافذة إنشاء/تعديل المجلة

**POS Window:**
- `src/ui/pos/pos_window.h/cpp` - نقطة البيع (المفروض تطبق سعر المجلة)

### 4. Database
- `database/delivery.db` - قاعدة البيانات الحالية

### 5. SQL Scripts
- `sql/create_promotions_tables.sql` - إنشاء جداول المجلات
- `sql/add_magazineprice_column.sql` - إضافة عمود MagazinePrice
- `sql/test_query.sql` - استعلامات للتحقق من البيانات

---

## نقاط التحقق المهمة

### 1. التحقق من البيانات في Database:

افتح `database/delivery.db` واعمل الاستعلام ده:

```sql
SELECT 
    p.Id, p.Title, p.Status, 
    datetime(p.StartDate) as Start,
    datetime(p.EndDate) as End,
    datetime('now') as Now,
    pp.ProductId, pp.MagazinePrice,
    prod.Name
FROM Promotions p
JOIN PromotionProducts pp ON pp.PromotionId = p.Id
JOIN Products prod ON pp.ProductId = prod.Id
WHERE p.Status = 1;
```

**المتوقع:**
- Status = 1 (Active)
- StartDate أقل من الوقت الحالي
- EndDate أكبر من الوقت الحالي
- MagazinePrice > 0

### 2. المشكلة المحتملة في الكود:

**ملف: `src/data/sqlite_repositories.cpp`**
**دالة: `SQLitePromotionRepository::getActive()`**
**السطر: ~2763**

المشكلة: الـ datetime comparison في SQLite ممكن مش بيشتغل صح مع QDateTime binding.

```cpp
// المشكلة المحتملة:
q.addBindValue(now); // QDateTime
// SQLite مش بيفهم QDateTime مباشرة

// الحل:
QString nowStr = now.toString(Qt::ISODate);
q.addBindValue(nowStr);
```

### 3. نقطة التطبيق في POS:

**ملف: `src/ui/pos/pos_window.cpp`**
**دالة: `refreshProducts()`**
**السطر: ~650**

الكود بيعمل:
1. يجيب كل المجلات النشطة: `getActive()`
2. يشوف لو المنتج موجود في المجلة
3. يجيب سعر المجلة: `getProductMagazinePrice()`
4. يطبق السعر على الكارد

**لو `getActive()` بترجع list فاضي، يبقى المشكلة في:**
- Query مش بيشتغل صح
- أو Status مش = 1
- أو التواريخ مش مظبوطة

---

## خطوات التصليح المقترحة

### الخطوة 1: تحقق من البيانات
```bash
cd e:\tifany
sqlite3 database/delivery.db < sql/test_query.sql
```

### الخطوة 2: لو البيانات صح، المشكلة في getActive()

جرب تبسيط الـ query:
```cpp
// بدل ما تشيك على التاريخ، ارجع كل المجلات اللي Status = 1
q.prepare("SELECT ... FROM Promotions WHERE Status=1");
```

### الخطوة 3: لو لسه مش شغال

استخدم approach مختلف - بدل ما تجيب المجلات النشطة كلها، اعمل query مباشر لكل منتج:

```cpp
double getPromotionPriceForProduct(int productId) {
    QSqlQuery q(db());
    q.prepare(
        "SELECT pp.MagazinePrice FROM PromotionProducts pp "
        "JOIN Promotions p ON pp.PromotionId = p.Id "
        "WHERE pp.ProductId = ? AND p.Status = 1 "
        "AND date('now') BETWEEN date(p.StartDate) AND date(p.EndDate) "
        "ORDER BY pp.MagazinePrice ASC LIMIT 1"
    );
    q.addBindValue(productId);
    if (q.exec() && q.next()) {
        return q.value(0).toDouble();
    }
    return 0;
}
```

---

## ملاحظات إضافية

1. **الضرائب:** اتصلحت - دلوقت بس المنتجات اللي `hasTax = true` بتظهر بضريبة
2. **تاريخ انتهاء المجلة:** المفروض تلقائي - لما EndDate يعدي، Status يبقى Expired
3. **عرض السعر القديم:** لو في promotion، الكارد بيعرض السعر القديم مشطوب + السعر الجديد

---

## للاتصال

لو عايز مساعدة في تطبيق الحل، ابعتلي:
1. نتيجة test_query.sql
2. Screenshot من console output لما تفتح POS
3. تفاصيل أي error messages

Good luck! 🚀
