#include "lang_manager.h"

LangManager& LangManager::instance() {
    static LangManager inst;
    return inst;
}

LangManager::LangManager() : QObject(nullptr) {
    loadArabic();
    loadArabicExtra();
    loadArabicComprehensive();
    loadArabicFinal();
    loadArabicColumns();
    loadArabicButtons();
    loadArabicDialogs();
}

void LangManager::setLanguage(AppLang lang) {
    if (m_lang == lang) return;
    m_lang = lang;
    emit languageChanged();
}

void LangManager::setLanguage(const QString& code) {
    setLanguage((code == "ar") ? AppLang::Arabic : AppLang::English);
}

AppLang LangManager::current() const { return m_lang; }
QString LangManager::currentCode() const {
    return (m_lang == AppLang::Arabic) ? "ar" : "en";
}

QString LangManager::t(const QString& key) const {
    if (m_lang == AppLang::Arabic && m_ar.contains(key))
        return m_ar.value(key);
    
    // For English, convert underscores to spaces and capitalize words
    QString result = key;
    result.replace('_', ' ');
    
    // Capitalize first letter of each word
    QStringList words = result.split(' ');
    for (QString& word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    result = words.join(' ');
    
    return result;
}

void LangManager::loadArabic() {
    // Navigation
    m_ar["Dashboard"]           = "لوحة التحكم";
    m_ar["Customers"]           = "العملاء";
    m_ar["Products"]            = "المنتجات";
    m_ar["Orders"]              = "الطلبات";
    m_ar["Scheduled Orders"]    = "الطلبات المجدولة";
    m_ar["Reports"]             = "التقارير";
    m_ar["Users"]               = "المستخدمون";
    m_ar["Settings"]            = "الإعدادات";
    m_ar["Logout"]              = "تسجيل الخروج";

    // Common actions
    m_ar["Add"]                 = "إضافة";
    m_ar["Edit"]                = "تعديل";
    m_ar["Delete"]              = "حذف";
    m_ar["Save"]                = "حفظ";
    m_ar["Cancel"]              = "إلغاء";
    m_ar["Search"]              = "بحث";
    m_ar["Refresh"]             = "تحديث";
    m_ar["Generate"]            = "إنشاء";
    m_ar["Export PDF"]          = "تصدير PDF";
    m_ar["Export Excel"]        = "تصدير Excel";
    m_ar["Filter"]              = "تصفية";

    // Orders
    m_ar["New Order"]           = "طلب جديد";
    m_ar["Order Details"]       = "تفاصيل الطلب";
    m_ar["Customer History"]    = "سجل العميل";
    m_ar["Status"]              = "الحالة";
    m_ar["Pending"]             = "قيد الانتظار";
    m_ar["Out for Delivery"]    = "جاري التوصيل";
    m_ar["Delivered"]           = "تم التوصيل";
    m_ar["Cancelled"]           = "ملغي";
    m_ar["Cancel Reason"]       = "سبب الإلغاء";
    m_ar["Grand Total"]         = "الإجمالي الكلي";
    m_ar["Subtotal"]            = "الإجمالي الفرعي";
    m_ar["Delivery Fee"]        = "رسوم التوصيل";

    // Products
    m_ar["Barcode"]             = "الباركود";
    m_ar["Price"]               = "السعر";
    m_ar["Category"]            = "الفئة";
    m_ar["Missing Products"]    = "المنتجات الناقصة";
    m_ar["Price History"]       = "سجل الأسعار";

    // Customers
    m_ar["Region"]              = "المنطقة";
    m_ar["Distance"]            = "المسافة";
    m_ar["Notes"]               = "ملاحظات";
    m_ar["Phones"]              = "الهواتف";
    m_ar["Addresses"]           = "العناوين";
    m_ar["Favorites"]           = "المفضلة";

    // Users / Roles
    m_ar["Username"]            = "اسم المستخدم";
    m_ar["Password"]            = "كلمة المرور";
    m_ar["Forgot Password?"]    = "نسيت كلمة المرور؟";
    m_ar["Forgot Password"]     = "استعادة كلمة المرور";
    
    // New OTP-based password recovery translations
    m_ar["forgot_password_title"] = "استعادة كلمة المرور";
    m_ar["enter_your_email"]    = "أدخل بريدك الإلكتروني";
    m_ar["email_recovery_desc"] = "سيتم إرسال رمز التحقق إلى بريدك الإلكتروني المسجل.";
    m_ar["email_address"]       = "البريد الإلكتروني";
    m_ar["send_verification_code"] = "إرسال رمز التحقق";
    m_ar["sending"]             = "جاري الإرسال...";
    m_ar["invalid_email_format"] = "صيغة البريد الإلكتروني غير صحيحة";
    m_ar["please_wait_seconds"] = "يرجى الانتظار %1 ثانية قبل المحاولة مرة أخرى";
    m_ar["otp_sent_generic_message"] = "إذا كان البريد الإلكتروني مسجلاً، سيتم إرسال رمز التحقق إليه.";
    m_ar["failed_to_send_email_generic"] = "فشل إرسال البريد الإلكتروني. يرجى المحاولة لاحقاً.";
    
    m_ar["enter_verification_code"] = "أدخل رمز التحقق";
    m_ar["otp_sent_desc"]       = "تم إرسال رمز مكون من 6 أرقام إلى بريدك الإلكتروني.";
    m_ar["verification_code"]   = "رمز التحقق";
    m_ar["verify_code"]         = "تحقق من الرمز";
    m_ar["resend_code"]         = "إعادة إرسال الرمز";
    m_ar["time_remaining"]      = "الوقت المتبقي: %1:%2";
    m_ar["remaining_attempts"]  = "عدد المحاولات المتبقية: %1";
    m_ar["otp_must_be_6_digits"] = "يجب أن يكون الرمز مكون من 6 أرقام";
    m_ar["incorrect_otp_remaining"] = "رمز خاطئ. المحاولات المتبقية: %1";
    m_ar["otp_attempts_exhausted"] = "نفدت المحاولات المتاحة. يرجى طلب رمز جديد.";
    m_ar["otp_expired"]         = "انتهت صلاحية الرمز";
    m_ar["please_wait"]         = "يرجى الانتظار";
    
    m_ar["set_new_password"]    = "تعيين كلمة مرور جديدة";
    m_ar["new_password_desc"]   = "اختر كلمة مرور قوية لحسابك (8 أحرف على الأقل).";
    m_ar["new_password"]        = "كلمة المرور الجديدة";
    m_ar["confirm_password"]    = "تأكيد كلمة المرور";
    m_ar["min_8_chars"]         = "8 أحرف على الأقل";
    m_ar["retype_password"]     = "أعد كتابة كلمة المرور";
    m_ar["reset_password"]      = "إعادة تعيين كلمة المرور";
    m_ar["password_required"]   = "كلمة المرور مطلوبة";
    m_ar["password_min_8_chars"] = "كلمة المرور يجب أن تكون 8 أحرف على الأقل";
    m_ar["passwords_dont_match"] = "كلمات المرور غير متطابقة";
    m_ar["password_reset_failed"] = "فشل إعادة تعيين كلمة المرور";
    m_ar["password_reset_success"] = "تم إعادة تعيين كلمة المرور بنجاح! يمكنك الآن تسجيل الدخول.";
    
    m_ar["Email *"]             = "البريد الإلكتروني *";
    m_ar["Email"]               = "البريد الإلكتروني";
    m_ar["Role"]                = "الدور";
    m_ar["Edit Product"]        = "تعديل المنتج";
    m_ar["Print Barcode"]       = "طباعة الباركود";
    m_ar["Manage Suppliers"]    = "إدارة الموردين";
    m_ar["Delete Product"]      = "حذف المنتج";
    m_ar["Print Options"]       = "خيارات الطباعة";
    m_ar["Include product name"] = "تضمين اسم المنتج";
    m_ar["Include price"]       = "تضمين السعر";
    m_ar["label(s)"]            = "ملصق(ات)";
    m_ar["Quantity"]            = "الكمية";
    m_ar["Preview"]             = "معاينة";
    m_ar["Refresh Preview"]     = "تحديث المعاينة";
    m_ar["Print"]               = "طباعة";
    m_ar["Product has no barcode. Using product ID instead."] = "المنتج ليس له باركود. سيتم استخدام معرّف المنتج بدلاً من ذلك.";
    m_ar["Failed to generate barcode."] = "فشل إنشاء الباركود.";
    m_ar["Failed to start printing."] = "فشل بدء الطباعة.";
    m_ar["Failed to create new page for label %1"] = "فشل إنشاء صفحة جديدة للملصق %1";
    m_ar["Barcode printed successfully!"] = "تم طباعة الباركود بنجاح!";
    m_ar["No barcode"]          = "لا يوجد باركود";
    
    // Product Suppliers Dialog
    m_ar["product_suppliers_title"] = "موردي المنتج";
    m_ar["current_suppliers"]   = "الموردون الحاليون";
    m_ar["supplier_name"]       = "اسم المورد";
    m_ar["purchase_price"]      = "سعر الشراء";
    m_ar["last_purchase_date"]  = "تاريخ آخر شراء";
    m_ar["preferred"]           = "مفضل";
    m_ar["best_price_indicator"] = "أفضل سعر";
    m_ar["best_price"]          = "أفضل سعر";
    m_ar["set_as_preferred"]    = "تعيين كمفضل";
    m_ar["remove_supplier"]     = "إزالة المورد";
    m_ar["add_supplier"]        = "إضافة مورد";
    m_ar["supplier"]            = "المورد";
    m_ar["please_select_supplier"] = "يرجى اختيار مورد";
    m_ar["supplier_already_added"] = "المورد مضاف بالفعل لهذا المنتج";
    m_ar["supplier_added_successfully"] = "تم إضافة المورد بنجاح";
    m_ar["failed_to_add_supplier"] = "فشل إضافة المورد";
    m_ar["please_select_supplier_to_remove"] = "يرجى اختيار مورد لإزالته";
    m_ar["confirm_remove_supplier"] = "هل أنت متأكد من إزالة هذا المورد؟";
    m_ar["supplier_removed_successfully"] = "تم إزالة المورد بنجاح";
    m_ar["failed_to_remove_supplier"] = "فشل إزالة المورد";
    m_ar["please_select_supplier_to_set_preferred"] = "يرجى اختيار مورد لتعيينه كمفضل";
    m_ar["preferred_supplier_set_successfully"] = "تم تعيين المورد المفضل بنجاح";
    m_ar["failed_to_set_preferred_supplier"] = "فشل تعيين المورد المفضل";
    m_ar["please_select_product"] = "يرجى اختيار منتج";
    
    // Login and branch selection
    m_ar["please_select_branch_first"] = "يرجى اختيار الفرع أولاً";
    m_ar["failed_to_connect_database"] = "فشل الاتصال بقاعدة البيانات";
    m_ar["warning"]             = "تحذير";
    m_ar["error"]               = "خطأ";
    
    m_ar["Roles"]               = "الأدوار";
    m_ar["Permissions"]         = "الصلاحيات";
    m_ar["Audit Log"]           = "سجل المراجعة";
    
    // Pages
    m_ar["Register/Till"]       = "الخزنة";

    // Dashboard
    m_ar["Today's Orders"]      = "طلبات اليوم";
    m_ar["Today's Sales"]       = "مبيعات اليوم";
    m_ar["Total Customers"]     = "إجمالي العملاء";
    m_ar["Best Selling Product"]= "الأكثر مبيعاً";
    m_ar["Scheduled Due Today"] = "مجدول اليوم";

    // Settings
    m_ar["Backup"]              = "نسخ احتياطي";
    m_ar["Restore"]             = "استعادة";
    m_ar["Language"]            = "اللغة";
    m_ar["Theme"]               = "المظهر";
    m_ar["Branch"]              = "الفرع";
    m_ar["Currency"]            = "العملة";

    // App branding
    m_ar["By Eng-Hosam Hassan"] = "بقلم م. حسام حسن";
}

// ── Additional UI strings ─────────────────────────────────────────────────────
// (appended for complete coverage)
void LangManager::loadArabicExtra() {
    // Common buttons
    m_ar["＋ Add Customer"]           = "＋ إضافة عميل";
    m_ar["＋ New Schedule"]           = "＋ جدولة جديدة";
    m_ar["＋ New Order"]              = "＋ طلب جديد";
    m_ar["＋ Add Product"]            = "＋ إضافة منتج";
    m_ar["＋ Report Missing Product"] = "＋ إضافة منتج ناقص";
    m_ar["＋ Add User"]               = "＋ إضافة مستخدم";
    m_ar["＋ Add Role"]               = "＋ إضافة دور";
    m_ar["＋ Add Driver"]             = "＋ إضافة مندوب";
    m_ar["✏️ Edit"]                   = "✏️ تعديل";
    m_ar["🗑 Delete"]                  = "🗑 حذف";
    m_ar["✓ Toggle Resolved"]         = "✓ تبديل الحالة";
    m_ar["🔄 Generate"]               = "🔄 توليد";
    m_ar["📄  PDF"]                   = "📄  PDF";
    m_ar["📊  Excel"]                 = "📊  Excel";
    m_ar["🔄 Refresh"]                = "🔄 تحديث";
    m_ar["💾  Save Settings"]         = "💾  حفظ الإعدادات";
    m_ar["📦  Backup Database"]       = "📦  نسخ احتياطي";
    m_ar["♻️  Restore from Backup"]   = "♻️  استعادة من النسخة";
    m_ar["🚪  Logout"]                = "🚪  تسجيل الخروج";

    // Page titles
    m_ar["Products"]                  = "المنتجات";
    m_ar["Customers"]                 = "العملاء";
    m_ar["Orders"]                    = "الطلبات";
    m_ar["Scheduled Orders"]          = "الطلبات المجدولة";
    m_ar["Reports"]                   = "التقارير";
    m_ar["Users"]                     = "المستخدمون";
    m_ar["Settings"]                  = "الإعدادات";
    m_ar["Dashboard"]                 = "لوحة التحكم";
    m_ar["Delivery Drivers"]          = "مناديب التوصيل";

    // Fields / labels
    m_ar["Customer"]                  = "العميل";
    m_ar["Date & Time"]               = "التاريخ والوقت";
    m_ar["Delivery Fee"]              = "رسوم التوصيل";
    m_ar["Order Items"]               = "عناصر الطلب";
    m_ar["Order Details"]             = "تفاصيل الطلب";
    m_ar["Delivery Driver"]           = "مندوب التوصيل";
    m_ar["Product Name"]              = "اسم المنتج";
    m_ar["Barcode"]                   = "الباركود";
    m_ar["Category"]                  = "الفئة";
    m_ar["Price"]                     = "السعر";
    m_ar["Status"]                    = "الحالة";
    m_ar["Manufacture Date"]          = "تاريخ الإنتاج";
    m_ar["Expiry Date"]               = "تاريخ الانتهاء";
    m_ar["Name"]                      = "الاسم";
    m_ar["Phone"]                     = "الهاتف";
    m_ar["National ID"]               = "الرقم القومي";
    m_ar["Region"]                    = "المنطقة";
    m_ar["Distance"]                  = "المسافة";
    m_ar["Notes"]                     = "ملاحظات";
    m_ar["Username"]                  = "اسم المستخدم";
    m_ar["Password"]                  = "كلمة المرور";
    m_ar["Role"]                      = "الدور";
    m_ar["Report Type"]               = "نوع التقرير";
    m_ar["From"]                      = "من";
    m_ar["To"]                        = "إلى";
    m_ar["Sales by Date Range"]       = "المبيعات حسب التاريخ";
    m_ar["Top Selling Products"]      = "أكثر المنتجات مبيعاً";
    m_ar["Top Customers"]             = "أفضل العملاء";
    m_ar["Inactive Customers (30+ days)"] = "العملاء غير النشطين (+30 يوم)";
    m_ar["Delivery Fee by Day"]       = "رسوم التوصيل اليومية";
    m_ar["Missing Products"]          = "المنتجات الناقصة";
    m_ar["Customer History"]          = "سجل العميل";
    m_ar["All Orders"]                = "كل الطلبات";
    m_ar["Recent Orders (Today)"]     = "الطلبات الحديثة (اليوم)";
    m_ar["Scheduled Orders Due Today"]= "المجدول لليوم";
    m_ar["Products Directory"]        = "دليل المنتجات";
    m_ar["Missing Product Requests"]  = "طلبات المنتجات الناقصة";
    m_ar["Today's Orders"]            = "طلبات اليوم";
    m_ar["Today's Sales"]             = "مبيعات اليوم";
    m_ar["Today's Delivery Fees"]     = "رسوم توصيل اليوم";
    m_ar["Total Customers"]           = "إجمالي العملاء";
    m_ar["Best Selling Product"]      = "الأكثر مبيعاً";
    m_ar["Scheduled Due Today"]       = "مجدول اليوم";

    // Login
    m_ar["🔐  Login"]                 = "🔐  تسجيل الدخول";

    // Tabs
    m_ar["ℹ️  Info"]                  = "ℹ️  معلومات";
    m_ar["📞  Phones"]                = "📞  الهواتف";
    m_ar["📍  Addresses"]             = "📍  العناوين";
    m_ar["⭐  Favorites"]             = "⭐  المفضلة";
    m_ar["👤 Users"]                  = "👤 المستخدمون";
    m_ar["🔐 Roles"]                  = "🔐 الأدوار";
    m_ar["📋 Audit Log"]              = "📋 سجل المراجعة";
    m_ar["🚴 Delivery Drivers"]       = "🚴 مناديب التوصيل";
    m_ar["📦 Products Directory"]     = "📦 دليل المنتجات";
    m_ar["⚠️ Missing Products"]       = "⚠️ منتجات ناقصة";
    m_ar["🛒 All Orders"]             = "🛒 كل الطلبات";
    m_ar["📋 Customer History"]       = "📋 سجل العميل";

    // Settings tabs
    m_ar["🗄️  Database"]              = "🗄️  قاعدة البيانات";
    m_ar["🏢  Branch"]                = "🏢  الفرع";
    m_ar["💾  Backup"]                = "💾  النسخ الاحتياطي";
    m_ar["Database Type:"]            = "نوع قاعدة البيانات:";
    m_ar["Branch Name:"]              = "اسم الفرع:";
    m_ar["Currency Symbol:"]          = "رمز العملة:";
    m_ar["Theme"]                     = "المظهر";
    m_ar["Language"]                  = "اللغة";

    // Additional UI strings
    m_ar["No customer selected"]      = "لم يتم اختيار عميل";
    m_ar["Cancel Reason"]             = "سبب الإلغاء";
    m_ar["Repeat on Weekdays *"]      = "تكرار في أيام الأسبوع *";
    m_ar["Fixed Products for this Order"] = "المنتجات الثابتة لهذا الطلب";
    m_ar["Recurring Order Details"]   = "تفاصيل الطلب المتكرر";
    m_ar["Product Details"]           = "تفاصيل المنتج";
    m_ar["Price Change History"]      = "سجل تغيير السعر";
    m_ar["Manage Regions"]            = "إدارة المناطق";
    m_ar["📁 Categories"]             = "📁 الفئات";
    m_ar["🗺 Regions"]                = "🗺 المناطق";
    m_ar["All Categories"]            = "جميع الفئات";
    m_ar["Filter by date:"]           = "فلتر بالتاريخ:";
    m_ar["🔄 Filter"]                 = "🔄 تصفية";
    m_ar["From:"]                     = "من:";
    m_ar["To:"]                       = "إلى:";
    m_ar["Report:"]                   = "التقرير:";
    m_ar["Press Generate to run a report."] = "اضغط توليد لتشغيل التقرير.";
    m_ar["Database Connection"]       = "اتصال قاعدة البيانات";
    m_ar["Branch Information"]        = "معلومات الفرع";
    m_ar["Database Backup & Restore"] = "نسخ احتياطي واستعادة";
    m_ar["Permissions"]               = "الصلاحيات";
    m_ar["Access"]                    = "الوصول";
    m_ar["Add"]                       = "إضافة";
    m_ar["Edit"]                      = "تعديل";
    m_ar["Delete"]                    = "حذف";
    m_ar["View Reports"]              = "عرض التقارير";
    m_ar["Manage Users"]              = "إدارة المستخدمين";
    m_ar["v1.0.0  |  By Eng-Hosam Hassan"] = "v1.0.0  |  بقلم م. حسام حسن";
    m_ar["🔐  Login"]                 = "🔐  تسجيل الدخول";
    m_ar["Missing Product Requests"]  = "طلبات المنتجات الناقصة";
    m_ar["Recent Orders (Today)"]     = "طلبات اليوم الحديثة";
    m_ar["Scheduled Due Today"]       = "مجدول لليوم";
    m_ar["Delivery Drivers"]          = "مناديب التوصيل";
    m_ar["Roles & Permissions"]       = "الأدوار والصلاحيات";
    m_ar["Roles"]                     = "الأدوار";
    m_ar["Audit Log"]                 = "سجل المراجعة";
    m_ar["Order History"]             = "سجل الطلبات";
    m_ar["Customer *"]                = "العميل *";
    m_ar["Date & Time *"]             = "التاريخ والوقت *";
    m_ar["Delivery Time *"]           = "وقت التوصيل *";
    m_ar["Customer *"]                = "العميل *";
    m_ar["Days *"]                    = "الأيام *";
    m_ar["Name *"]                    = "الاسم *";
    m_ar["Price *"]                   = "السعر *";
    m_ar["Category *"]                = "الفئة *";
    m_ar["Role Name *"]               = "اسم الدور *";
    m_ar["Username *"]                = "اسم المستخدم *";
    m_ar["Active"]                    = "نشط";
    m_ar["Hidden"]                    = "مخفي";
    m_ar["Pending"]                   = "قيد الانتظار";
    m_ar["Out for Delivery"]          = "جاري التوصيل";
    m_ar["Delivered"]                 = "تم التوصيل";
    m_ar["Cancelled"]                 = "ملغي";
    m_ar["Qty Needed"]                = "الكمية المطلوبة";
    m_ar["Date Added"]                = "تاريخ الإضافة";
    m_ar["✓ Purchased"]               = "✓ تم الشراء";
    m_ar["⏳ Pending"]                 = "⏳ قيد الانتظار";
    m_ar["Grand Total"]               = "الإجمالي الكلي";
    m_ar["Subtotal"]                  = "الإجمالي الفرعي";
    m_ar["Order #"]                   = "رقم الطلب";
    m_ar["Items"]                     = "العناصر";
    m_ar["Customer"]                  = "العميل";
    m_ar["Date & Time"]               = "التاريخ والوقت";
    m_ar["Delivery Fee"]              = "رسوم التوصيل";
    m_ar["Product (name or barcode)"] = "المنتج (اسم أو باركود)";
    m_ar["Unit Price"]                = "سعر الوحدة";
    m_ar["Line Total"]                = "إجمالي السطر";
    m_ar["Quantity"]                  = "الكمية";
    m_ar["Product"]                   = "المنتج";
    m_ar["🔄  Generate"]              = "🔄  توليد";
    m_ar["Save"]                      = "حفظ";
    m_ar["Close"]                     = "إغلاق";

    m_ar["Delivery Management System"] = "نظام إدارة التوصيل";
}

// ── Comprehensive Arabic translations — all pages ─────────────────────────────
void LangManager::loadArabicComprehensive() {

    // ── order_dialog ─────────────────────────────────────────────────────────
    m_ar["Order"]                              = "طلب";
    m_ar["Customer"]                           = "العميل";
    m_ar["Order Details"]                      = "تفاصيل الطلب";
    m_ar["Date & Time *"]                      = "التاريخ والوقت *";
    m_ar["Delivery Fee"]                       = "رسوم التوصيل";
    m_ar["Delivery Driver"]                    = "مندوب التوصيل";
    m_ar["💳 Payment Method *"]               = "💳 طريقة الدفع *";
    m_ar["Specify method:"]                    = "حدد الطريقة:";
    m_ar["🏷 Discount"]                        = "🏷 خصم";
    m_ar["Discount Reason"]                    = "سبب الخصم";
    m_ar["🏷  Activate Coupon"]               = "🏷  تفعيل كوبون";
    m_ar["🏷  Hide Coupon"]                   = "🏷  إخفاء الكوبون";
    m_ar["Coupon Code:"]                       = "كود الكوبون:";
    m_ar["Apply"]                              = "تطبيق";
    m_ar["Order Items"]                        = "عناصر الطلب";
    m_ar["＋ Add Item"]                        = "＋ إضافة عنصر";
    m_ar["－ Remove"]                          = "－ حذف";
    m_ar["Product (name or barcode)"]          = "المنتج (اسم أو باركود)";
    m_ar["Barcode"]                            = "الباركود";
    m_ar["Qty"]                                = "الكمية";
    m_ar["Unit Price"]                         = "سعر الوحدة";
    m_ar["Line Total"]                         = "إجمالي السطر";
    m_ar["Subtotal"]                           = "الإجمالي الفرعي";
    m_ar["Grand Total"]                        = "الإجمالي الكلي";
    m_ar["— No Driver Assigned —"]            = "— لا يوجد مندوب —";
    m_ar["Type name or barcode…  (scanner auto-detects)"]
                                               = "اكتب الاسم أو الباركود… (الماسح تلقائي)";
    m_ar["🔍  Type to search customer by name..."]
                                               = "🔍  ابحث عن العميل بالاسم...";
    m_ar["No customer selected"]               = "لم يتم اختيار عميل";
    m_ar["Reason for cancellation..."]         = "سبب الإلغاء...";
    m_ar["Reason for discount (e.g. Loyalty, Damage)..."]
                                               = "سبب الخصم (مثل: ولاء، تلف)...";
    m_ar["Enter coupon code..."]               = "أدخل كود الكوبون...";
    m_ar["e.g. Bank transfer, Instapay..."]    = "مثل: تحويل بنكي، إنستاباي...";
    m_ar["No discount"]                        = "بدون خصم";
    m_ar["Your role does not allow changing the order date/time."]
                                               = "صلاحيتك لا تتيح تغيير تاريخ/وقت الطلب.";
    m_ar["Coupon gives 0 discount on current items."]
                                               = "الكوبون لا يعطي خصماً على العناصر الحالية.";
    m_ar["Please enter a code."]               = "الرجاء إدخال كود.";
    m_ar["Code not found."]                    = "الكود غير موجود.";
    m_ar["Coupon system not available."]       = "نظام الكوبونات غير متاح.";

    // ── products_page ─────────────────────────────────────────────────────────
    m_ar["📦 Products Directory"]              = "📦 دليل المنتجات";
    m_ar["⚠️ Missing Products"]               = "⚠️ منتجات ناقصة";
    m_ar["Expired Products"]                   = "المنتجات المنتهية";
    m_ar["Missing Product Requests"]           = "طلبات المنتجات الناقصة";
    m_ar["🔍  Search products or categories..."]
                                               = "🔍  ابحث عن منتج أو فئة...";
    m_ar["📁 Categories"]                      = "📁 الفئات";
    m_ar["＋ Add Product"]                     = "＋ إضافة منتج";
    m_ar["＋ Report Missing Product"]          = "＋ الإبلاغ عن منتج ناقص";
    m_ar["✓ Toggle Resolved"]                  = "✓ تبديل الحالة";
    m_ar["📥 Import"]                          = "📥 استيراد";
    m_ar["📥 Import Data"]                     = "📥 استيراد بيانات";
    m_ar["♻️ Restore to Active"]              = "♻️ استعادة إلى نشط";
    m_ar["🔄 Refresh"]                         = "🔄 تحديث";
    m_ar["Product Requested"]                  = "المنتج المطلوب";
    m_ar["Qty Required"]                       = "الكمية المطلوبة";
    m_ar["Current Stock"]                      = "المخزون الحالي";
    m_ar["Date Added"]                         = "تاريخ الإضافة";
    m_ar["Status"]                             = "الحالة";
    m_ar["ID"]                                 = "المعرّف";
    m_ar["Name"]                               = "الاسم";
    m_ar["Category"]                           = "الفئة";
    m_ar["Price"]                              = "السعر";
    m_ar["Expired On"]                         = "انتهى في";
    m_ar["Import products from Excel file"]    = "استيراد المنتجات من ملف Excel";
    m_ar["Reload expired products list"]       = "إعادة تحميل قائمة المنتجات المنتهية";
    m_ar["Mark selected product as Active again"]
                                               = "تعيين المنتج المحدد كنشط مجدداً";

    // ── customers_page ────────────────────────────────────────────────────────
    m_ar["🔍  Search by name, phone, or address..."]
                                               = "🔍  ابحث بالاسم أو الهاتف أو العنوان...";
    m_ar["🗺 Regions"]                         = "🗺 المناطق";
    m_ar["Manage Regions"]                     = "إدارة المناطق";
    m_ar["0 customers"]                        = "0 عملاء";
    m_ar["customer(s) total"]                  = "إجمالي العملاء";
    m_ar["ℹ️  Info"]                           = "ℹ️  معلومات";
    m_ar["📞  Phones"]                         = "📞  الهواتف";
    m_ar["📍  Addresses"]                      = "📍  العناوين";
    m_ar["⭐  Favorites"]                      = "⭐  المفضلة";

    // ── users_page ────────────────────────────────────────────────────────────
    m_ar["Role Name *"]                        = "اسم الدور *";
    m_ar["Customers"]                          = "العملاء";
    m_ar["Orders"]                             = "الطلبات";
    m_ar["Products"]                           = "المنتجات";
    m_ar["Access"]                             = "الوصول";
    m_ar["Add"]                                = "إضافة";
    m_ar["Edit"]                               = "تعديل";
    m_ar["Delete"]                             = "حذف";
    m_ar["Edit Date"]                          = "تعديل التاريخ";
    m_ar["Cancel"]                             = "إلغاء";
    m_ar["Dashboard"]                          = "لوحة التحكم";
    m_ar["View Reports"]                       = "عرض التقارير";
    m_ar["Manage Regions"]                     = "إدارة المناطق";
    m_ar["Settings"]                           = "الإعدادات";
    m_ar["Manage Users"]                       = "إدارة المستخدمين";
    m_ar["#"]                                  = "#";
    m_ar["Role Name"]                          = "اسم الدور";
    m_ar["Phone"]                              = "الهاتف";
    m_ar["National ID"]                        = "الرقم القومي";
    m_ar["User"]                               = "مستخدم";
    m_ar["Validation"]                         = "تحقق";
    m_ar["Name and Username are required."]    = "الاسم واسم المستخدم مطلوبان.";
    m_ar["Role name is required."]             = "اسم الدور مطلوب.";
    m_ar["User Details"]                       = "بيانات المستخدم";

    // ── reports_page ──────────────────────────────────────────────────────────
    m_ar["Sales by Date Range"]                = "المبيعات حسب التاريخ";
    m_ar["Top Selling Products"]               = "أكثر المنتجات مبيعاً";
    m_ar["Top Customers"]                      = "أفضل العملاء";
    m_ar["Inactive Customers (30+ days)"]      = "العملاء غير النشطين (+30 يوم)";
    m_ar["Delivery Fee by Day"]                = "رسوم التوصيل اليومية";
    m_ar["Missing Products"]                   = "المنتجات الناقصة";
    m_ar["Expired Products"]                   = "المنتجات المنتهية";
    m_ar["Cancelled Orders"]                   = "الطلبات الملغاة";
    m_ar["Top Products per Customer"]          = "أكثر المنتجات لعميل معين";
    m_ar["Report:"]                            = "التقرير:";
    m_ar["From:"]                              = "من:";
    m_ar["To:"]                                = "إلى:";
    m_ar["Status:"]                            = "الحالة:";
    m_ar["All"]                                = "الكل";
    m_ar["Pending"]                            = "قيد الانتظار";
    m_ar["Resolved"]                           = "تم الحل";
    m_ar["🔄  Generate"]                       = "🔄  توليد";
    m_ar["📄  PDF"]                            = "📄  PDF";
    m_ar["📊  Excel"]                          = "📊  Excel";
    m_ar["Press Generate to run a report."]    = "اضغط توليد لتشغيل التقرير.";
    m_ar["Product"]                            = "المنتج";
    m_ar["Qty Sold"]                           = "الكمية المباعة";
    m_ar["Revenue"]                            = "الإيراد";
    m_ar["Total Spent"]                        = "إجمالي الإنفاق";
    m_ar["Days Inactive"]                      = "أيام عدم النشاط";
    m_ar["Last Order"]                         = "آخر طلب";
    m_ar["Total Delivery Fees"]                = "إجمالي رسوم التوصيل";
    m_ar["Date"]                               = "التاريخ";
    m_ar["Invoice #"]                          = "رقم الفاتورة";
    m_ar["Order #"]                            = "رقم الطلب";
    m_ar["Customer"]                           = "العميل";
    m_ar["Payment"]                            = "الدفع";
    m_ar["Items"]                              = "العناصر";
    m_ar["Discount"]                           = "الخصم";
    m_ar["Grand Total"]                        = "الإجمالي الكلي";

    // ── settings_page ─────────────────────────────────────────────────────────
    m_ar["💾  Save Settings"]                  = "💾  حفظ الإعدادات";
    m_ar["🎨  App Theme"]                      = "🎨  مظهر التطبيق";
    m_ar["Switch between Dark, Light, Teal, Luxury, Emerald, Abyss, and Noir themes instantly."]
                                               = "قم بالتبديل الفوري بين الثيمات: داكن، فاتح، تيل، فاخر، زمردي، هاوية، نوار.";
    m_ar["📝  Notes Board Background"]         = "📝  خلفية لوحة الملاحظات";
    m_ar["Choose whether the Notes board uses the current theme color or a custom image."]
                                               = "اختر خلفية لوحة الملاحظات: لون الثيم أو صورة مخصصة.";
    m_ar["Background:"]                        = "الخلفية:";
    m_ar["Solid color (follows theme)"]        = "لون صلب (يتبع الثيم)";
    m_ar["Custom image"]                       = "صورة مخصصة";
    m_ar["Path to background image (PNG / JPG)"]
                                               = "مسار صورة الخلفية (PNG / JPG)";
    m_ar["Choose image…"]                      = "اختر صورة…";
    m_ar["Company Details (appear on invoices)"]
                                               = "بيانات الشركة (تظهر على الفواتير)";
    m_ar["Company Name:"]                      = "اسم الشركة:";
    m_ar["Address:"]                           = "العنوان:";
    m_ar["Phone:"]                             = "الهاتف:";
    m_ar["Tax Number:"]                        = "الرقم الضريبي:";
    m_ar["Logo:"]                              = "الشعار:";
    m_ar["Browse…"]                            = "استعراض…";
    m_ar["Current Branch Info"]                = "معلومات الفرع الحالي";
    m_ar["Branch Name:"]                       = "اسم الفرع:";
    m_ar["Currency Symbol:"]                   = "رمز العملة:";
    m_ar["Multi-Branch Configuration"]         = "إعداد الفروع المتعددة";
    m_ar["＋ Add Branch"]                      = "＋ إضافة فرع";
    m_ar["Branch Name"]                        = "اسم الفرع";
    m_ar["Database Path / URL"]                = "مسار/رابط قاعدة البيانات";
    m_ar["🌐  Branches"]                       = "🌐  الفروع";
    m_ar["🏭  Company"]                        = "🏭  الشركة";
    m_ar["🏢  Branch"]                         = "🏢  الفرع";
    m_ar["☁️  Deploy Schema to Selected Cloud Branch"]
                                               = "☁️  نشر مخطط قاعدة البيانات للفرع السحابي";
    m_ar["Database Connection (default branch)"]
                                               = "اتصال قاعدة البيانات (الفرع الافتراضي)";
    m_ar["Database Type:"]                     = "نوع قاعدة البيانات:";
    m_ar["SQLite (local)"]                     = "SQLite (محلي)";
    m_ar["Access via ODBC"]                    = "Access عبر ODBC";
    m_ar["PostgreSQL/Supabase"]                = "PostgreSQL/Supabase";
    m_ar["SQLite Path:"]                       = "مسار SQLite:";
    m_ar["Connection String:"]                 = "نص الاتصال:";
    m_ar["🗄️  Database"]                       = "🗄️  قاعدة البيانات";
    m_ar["💾  Backup"]                         = "💾  النسخ الاحتياطي";
    m_ar["Database Backup & Restore"]          = "النسخ الاحتياطي والاستعادة";
    m_ar["📦  Backup Database"]               = "📦  نسخ احتياطي";
    m_ar["♻️  Restore from Backup"]           = "♻️  استعادة من النسخة";
    m_ar["Auto-Backup"]                        = "نسخ احتياطي تلقائي";
    m_ar["Daily backup at:"]                   = "نسخ احتياطي يومي في:";
    m_ar["Backup folder:"]                     = "مجلد النسخ الاحتياطي:";
    m_ar["Select Folder"]                      = "اختر مجلداً";
    m_ar["Order Reminders"]                    = "تذكيرات الطلبات";
    m_ar["Enable due-today reminders"]         = "تفعيل تذكيرات اليوم";
    m_ar["Remind"]                             = "تذكير";
    m_ar["minutes before scheduled time"]      = "دقيقة قبل الوقت المجدول";
    m_ar["Check every"]                        = "فحص كل";
    m_ar["minutes"]                            = "دقائق";
    m_ar["⏰  Reminders"]                      = "⏰  التذكيرات";

    // ── audit_log_page ────────────────────────────────────────────────────────
    m_ar["Audit Log"]                          = "سجل المراجعة";
    m_ar["🔄  Refresh"]                        = "🔄  تحديث";
    m_ar["↩️  Undo Selected"]                 = "↩️  تراجع عن المحدد";
    m_ar["Undo the selected operation (requires dual authentication)"]
                                               = "التراجع عن العملية (يتطلب مصادقة مزدوجة)";
    m_ar["🔍  Search details..."]              = "🔍  ابحث في التفاصيل...";
    m_ar["All Users"]                          = "جميع المستخدمين";
    m_ar["All Actions"]                        = "جميع الإجراءات";
    m_ar["All Entities"]                       = "جميع الكيانات";
    m_ar["Apply"]                              = "تطبيق";
    m_ar["Clear"]                              = "مسح";
    m_ar["User:"]                              = "المستخدم:";
    m_ar["Action:"]                            = "الإجراء:";
    m_ar["Entity:"]                            = "الكيان:";
    m_ar["From:"]                              = "من:";
    m_ar["ID"]                                 = "المعرّف";
    m_ar["Timestamp"]                          = "الوقت";
    m_ar["User"]                               = "المستخدم";
    m_ar["Action"]                             = "الإجراء";
    m_ar["Entity"]                             = "الكيان";
    m_ar["Entity ID"]                          = "معرّف الكيان";
    m_ar["Details"]                            = "التفاصيل";
    m_ar["ℹ️  Undo requires authentication from both the operation owner and an admin."]
                                               = "ℹ️  التراجع يتطلب مصادقة من مالك العملية ومدير.";

    // ── returns_page ──────────────────────────────────────────────────────────
    m_ar["Returns & Refunds"]                  = "المرتجعات والمبالغ المستردة";
    m_ar["＋ New Return"]                      = "＋ مرتجع جديد";
    m_ar["🖨 Credit Note"]                     = "🖨 إشعار دائن";
    m_ar["Order #"]                            = "رقم الطلب";
    m_ar["Type"]                               = "النوع";
    m_ar["Refund"]                             = "المبلغ المسترد";
    m_ar["Reason"]                             = "السبب";
    m_ar["Full"]                               = "كامل";
    m_ar["Partial"]                            = "جزئي";
    m_ar["return(s)"]                          = "مرتجع(ات)";
    m_ar["Total Refunded:"]                    = "إجمالي المبالغ المستردة:";
    m_ar["New Return / Refund"]                = "مرتجع / مبلغ مسترد جديد";
    m_ar["Search by customer name or order number..."]
                                               = "ابحث بالاسم أو رقم الطلب...";
    m_ar["Search Order *"]                     = "بحث عن طلب *";
    m_ar["Select Order *"]                     = "اختر طلباً *";
    m_ar["— Enter search above to find orders —"]
                                               = "— أدخل بحثاً أعلاه للعثور على طلبات —";
    m_ar["Reason *"]                           = "السبب *";
    m_ar["Full return (all items)"]            = "مرتجع كامل (كل العناصر)";
    m_ar["Original Qty"]                       = "الكمية الأصلية";
    m_ar["Return Qty"]                         = "كمية الإرجاع";

    // ── dashboard_page ────────────────────────────────────────────────────────
    m_ar["📊  Dashboard"]                      = "📊  لوحة التحكم";
    m_ar["🔄  Refresh"]                        = "🔄  تحديث";
    m_ar["Today's Orders"]                     = "طلبات اليوم";
    m_ar["Pending Orders"]                     = "الطلبات قيد الانتظار";
    m_ar["Cancelled Today"]                    = "ملغي اليوم";
    m_ar["Scheduled Due Today"]                = "مجدول لليوم";
    m_ar["Today's Revenue"]                    = "إيرادات اليوم";
    m_ar["Today's Profit"]                     = "أرباح اليوم";
    m_ar["This Month Revenue"]                 = "إيرادات الشهر";
    m_ar["Delivery Fees Today"]                = "رسوم التوصيل اليوم";
    m_ar["Low Stock Items"]                    = "منتجات منخفضة المخزون";
    m_ar["Expired Products"]                   = "منتجات منتهية الصلاحية";
    m_ar["Best Product"]                       = "أفضل منتج";
    m_ar["Total Customers"]                    = "إجمالي العملاء";
    m_ar["Top Customer"]                       = "أفضل عميل";
    m_ar["Orders"]                             = "الطلبات";
    m_ar["Revenue"]                            = "الإيرادات";
    m_ar["Inventory Alerts"]                   = "تنبيهات المخزون";
    m_ar["Customer Status"]                    = "حالة العملاء";
    m_ar["Analytics"]                          = "التحليلات";
    m_ar["Name"]                               = "الاسم";
    m_ar["Phone"]                              = "الهاتف";
    m_ar["😴  Inactive  (14+ days)"]           = "😴  غير نشط  (+14 يوم)";
    m_ar["🤔  Hesitant  (7–14 days)"]          = "🤔  متردد  (7-14 يوم)";

    // ── scheduled_orders_page ─────────────────────────────────────────────────
    m_ar["Scheduled Orders"]                   = "الطلبات المجدولة";
    m_ar["＋ New Schedule"]                    = "＋ جدولة جديدة";
    m_ar["Schedule"]                           = "الجدول";
    m_ar["Time"]                               = "الوقت";
    m_ar["Action"]                             = "إجراء";
    m_ar["0 recurring orders"]                 = "0 طلبات متكررة";
    m_ar["Convert to Order"]                   = "تحويل إلى طلب";
    m_ar["One-time"]                           = "مرة واحدة";

    // ── notes_page ────────────────────────────────────────────────────────────
    m_ar["📝  Notes  /  ملاحظات"]             = "📝  ملاحظات";
    m_ar["🔍  Search notes…"]                  = "🔍  ابحث في الملاحظات…";
    m_ar["＋  New Note"]                       = "＋  ملاحظة جديدة";
    m_ar["Note Editor"]                        = "محرر الملاحظات";
    m_ar["Note title…"]                        = "عنوان الملاحظة…";
    m_ar["Color:"]                             = "اللون:";
    m_ar["📌 Pin"]                             = "📌 تثبيت";
    m_ar["📍 Pinned"]                          = "📍 مثبت";
    m_ar["Write your note here…"]             = "اكتب ملاحظتك هنا…";
    m_ar["👥  Mention Customer"]              = "👥  ذكر عميل";
    m_ar["📦  Mention Product"]               = "📦  ذكر منتج";
    m_ar["Name or phone…"]                     = "الاسم أو الهاتف…";
    m_ar["Name or barcode…"]                   = "الاسم أو الباركود…";
    m_ar["@ Insert Mention"]                   = "@ إدراج ذكر";
    m_ar["💾  Save Note"]                      = "💾  حفظ الملاحظة";
    m_ar["No notes yet.\n\nClick  ＋ New Note  to get started."]
                                               = "لا توجد ملاحظات بعد.\n\nاضغط ＋ ملاحظة جديدة للبدء.";
    m_ar["Page %1 of %2"]                      = "صفحة %1 من %2";
    m_ar["◀  Previous"]                        = "◀  السابق";
    m_ar["Next  ▶"]                            = "التالي  ▶";
    m_ar["note(s)"]                            = "ملاحظة/ملاحظات";
    m_ar["📄 Attach file"]                     = "📄 إرفاق ملف";
    m_ar["🖼 Attach image"]                    = "🖼 إرفاق صورة";
    m_ar["📎 Attachments:"]                    = "📎 المرفقات:";
    m_ar["Double-click to open full view"]     = "انقر مرتين لعرض كامل";

    // ── coupons_page ──────────────────────────────────────────────────────────
    m_ar["Coupons"]                            = "الكوبونات";
    m_ar["＋ Add Coupon"]                      = "＋ إضافة كوبون";
    m_ar["Code"]                               = "الكود";
    m_ar["Type"]                               = "النوع";
    m_ar["Value"]                              = "القيمة";
    m_ar["Min. Order"]                         = "الحد الأدنى";
    m_ar["Max Uses"]                           = "الحد الأقصى للاستخدام";
    m_ar["Used"]                               = "المستخدم";
    m_ar["Expiry"]                             = "تاريخ الانتهاء";
    m_ar["Active"]                             = "نشط";
    m_ar["Percentage"]                         = "نسبة مئوية";
    m_ar["Fixed Amount"]                       = "مبلغ ثابت";

    // ── login_dialog ──────────────────────────────────────────────────────────
    m_ar["Delivery Management System"]        = "نظام إدارة التوصيل";
    m_ar["Welcome back"]                      = "مرحباً بعودتك";
    m_ar["Username *"]                        = "اسم المستخدم *";
    m_ar["Password *"]                        = "كلمة المرور *";
    m_ar["🔐  Login"]                         = "🔐  تسجيل الدخول";
    m_ar["Select Branch"]                     = "اختر الفرع";

    // ── common messages ───────────────────────────────────────────────────────
    m_ar["Access Denied"]                      = "الوصول مرفوض";
    m_ar["Error"]                              = "خطأ";
    m_ar["Success"]                            = "نجاح";
    m_ar["Confirm"]                            = "تأكيد";
    m_ar["Confirm Delete"]                     = "تأكيد الحذف";
    m_ar["Yes"]                                = "نعم";
    m_ar["No"]                                 = "لا";
    m_ar["Save"]                               = "حفظ";
    m_ar["Close"]                              = "إغلاق";
    m_ar["Cancel"]                             = "إلغاء";
    m_ar["Search"]                             = "بحث";
    m_ar["order(s)"]                           = "طلب/طلبات";
    m_ar["order(s) total"]                     = "إجمالي الطلبات";
    m_ar["product(s)"]                         = "منتج/منتجات";
    m_ar["invoice"]                            = "فاتورة";
    m_ar["Filter by date:"]                    = "تصفية بالتاريخ:";
    m_ar["to"]                                 = "إلى";
    m_ar["Order History"]                      = "سجل الطلبات";
    m_ar["Invoice #"]                          = "رقم الفاتورة";
    m_ar["Date & Time"]                        = "التاريخ والوقت";
    m_ar["Subtotal"]                           = "الإجمالي الفرعي";
    m_ar["Delivery"]                           = "التوصيل";
    m_ar["Search customer by name, phone, or address..."]
                                               = "ابحث عن العميل بالاسم أو الهاتف أو العنوان...";
    m_ar["🔍  Search by customer name or invoice # (e.g. INV-0042)..."]
                                               = "🔍  ابحث بالاسم أو رقم الفاتورة...";
}

// ── Final missing translations ────────────────────────────────────────────────
void LangManager::loadArabicFinal() {
    // products_page tabs
    m_ar["📦 Products Directory"]          = "📦 دليل المنتجات";
    m_ar["⚠️ Missing Products"]           = "⚠️ منتجات ناقصة";
    m_ar["⌛ Expired Products"]            = "⌛ منتجات منتهية";

    // order_dialog
    m_ar["Order"]                          = "طلب";
    m_ar["＋ Add Item"]                    = "＋ إضافة عنصر";
    m_ar["－ Remove"]                      = "－ حذف";
    m_ar["Qty"]                            = "الكمية";

    // users_page tabs
    m_ar["👤 Users"]                       = "👤 المستخدمون";
    m_ar["🔐 Roles"]                       = "🔐 الأدوار";
    m_ar["🚴 Delivery Drivers"]            = "🚴 مناديب التوصيل";

    // orders_page tabs
    m_ar["🛒 All Orders"]                  = "🛒 كل الطلبات";
    m_ar["📋 Customer History"]            = "📋 سجل العميل";

    // sidebar
    m_ar["Returns"]                        = "المرتجعات";
    m_ar["Coupons"]                        = "الكوبونات";
    m_ar["Notes"]                          = "ملاحظات";
    m_ar["Audit Log"]                      = "سجل المراجعة";

    // product table model headers
    m_ar["Product Name"]                   = "اسم المنتج";
    m_ar["Quantity"]                       = "الكمية";
    m_ar["Safety Margin"]                  = "هامش الأمان";
    m_ar["Manufacture Date"]               = "تاريخ الإنتاج";
    m_ar["Expiry Date"]                    = "تاريخ الانتهاء";

    // customer table
    m_ar["Region"]                         = "المنطقة";
    m_ar["Distance"]                       = "المسافة";
    m_ar["Address"]                        = "العنوان";
    m_ar["Notes"]                          = "ملاحظات";

    // order table
    m_ar["Invoice"]                        = "الفاتورة";
    m_ar["Invoice #"]                      = "رقم الفاتورة";
    m_ar["Payment"]                        = "الدفع";
    m_ar["Total"]                          = "الإجمالي";
    m_ar["Items"]                          = "العناصر";

    // reports
    m_ar["Expired Products"]               = "المنتجات المنتهية";
    m_ar["Top Products per Customer"]      = "أكثر المنتجات لعميل";

    // coupon toggle text
    m_ar["🏷  Hide Coupon"]               = "🏷  إخفاء الكوبون";
    m_ar["🏷  Activate Coupon"]           = "🏷  تفعيل كوبون";
    
    // ── Phase 1 Translations ──────────────────────────────────────────────────
    // Navigation/Sidebar
    m_ar["Suppliers"]                      = "الموردين";
    m_ar["Purchases"]                      = "المشتريات";
    m_ar["Register"]                       = "الصندوق";
    m_ar["Expenses"]                       = "المصروفات";
    m_ar["Inventory"]                      = "الجرد";
    m_ar["POS"]                            = "نقطة البيع";
    
    // Suppliers page
    m_ar["＋ Add Supplier"]                = "＋ إضافة مورد";
    m_ar["Add Supplier"]                   = "إضافة مورد";
    m_ar["Edit Supplier"]                  = "تعديل مورد";
    m_ar["Search suppliers..."]            = "بحث عن موردين...";
    m_ar["Contact Person"]                 = "الشخص المسؤول";
    m_ar["Phone"]                          = "الهاتف";
    m_ar["Email"]                          = "البريد الإلكتروني";
    m_ar["Active"]                         = "نشط";
    m_ar["Inactive"]                       = "غير نشط";
    m_ar["Name"]                           = "الاسم";
    
    // Purchases page
    m_ar["＋ New Purchase"]                = "＋ مشترى جديد";
    m_ar["Invoice Number"]                 = "رقم الفاتورة";
    m_ar["Supplier"]                       = "المورد";
    m_ar["Purchase Date"]                  = "تاريخ الشراء";
    m_ar["Unit Cost"]                      = "سعر الوحدة";
    m_ar["Cost Price"]                     = "سعر التكلفة";
    m_ar["Paid Amount"]                    = "المبلغ المدفوع";
    
    // Register/Till page
    m_ar["🔓 Open Session"]                = "🔓 فتح وردية";
    m_ar["🔒 Close Session"]               = "🔒 إغلاق وردية";
    m_ar["👁 View"]                        = "👁 عرض";
    m_ar["Session #"]                      = "رقم الوردية";
    m_ar["Opened At"]                      = "وقت الفتح";
    m_ar["Closed At"]                      = "وقت الإغلاق";
    m_ar["Opening Cash"]                   = "الرصيد الافتتاحي";
    m_ar["Closing Cash"]                   = "الرصيد الختامي";
    m_ar["Current Session:"]               = "الوردية الحالية:";
    m_ar["No active session"]              = "لا توجد وردية نشطة";
    
    // Expenses page
    m_ar["📝 Expenses & Categories"]       = "📝 المصروفات والفئات";
    m_ar["＋ Add Expense"]                 = "＋ إضافة مصروف";
    m_ar["＋ Add Category"]                = "＋ إضافة فئة";
    m_ar["💸 Expenses"]                    = "💸 المصروفات";
    m_ar["📂 Categories"]                  = "📂 الفئات";
    m_ar["Category:"]                      = "الفئة:";
    m_ar["Amount:"]                        = "المبلغ:";
    m_ar["Receipt"]                        = "الإيصال";
    m_ar["Created By"]                     = "أُنشئ بواسطة";
    m_ar["Add Expense"]                    = "إضافة مصروف";
    m_ar["Add Category"]                   = "إضافة فئة";
    m_ar["Name:"]                          = "الاسم:";
    m_ar["Failed to save expense."]        = "فشل حفظ المصروف.";
    m_ar["Failed to save category."]       = "فشل حفظ الفئة.";
    m_ar["All Categories"]                 = "كل الفئات";
    
    // Inventory page
    m_ar["📦 Inventory & Stocktake"]       = "📦 الجرد والمخزون";
    
    // POS Window
    m_ar["Point of Sale"]                  = "نقطة البيع";
    m_ar["Cart"]                           = "السلة";
    m_ar["Clear Cart"]                     = "مسح السلة";
    m_ar["Checkout"]                       = "إتمام البيع";
    m_ar["Cash"]                           = "نقدي";
    m_ar["Card"]                           = "بطاقة";
    m_ar["Other"]                          = "أخرى";
    m_ar["Payment"]                        = "الدفع";
    m_ar["Complete Payment"]               = "إتمام الدفع";
    m_ar["Total Amount:"]                  = "المبلغ الإجمالي:";
    m_ar["Payment Method:"]                = "طريقة الدفع:";
    m_ar["Cash Received:"]                 = "المبلغ المستلم:";
    m_ar["Change:"]                        = "الباقي:";
    m_ar["ج.م"]                           = "ج.م";
    m_ar["Customer (Optional)"]            = "العميل (اختياري)";
    m_ar["Walk-in Customer"]               = "عميل عابر";
    m_ar["Select Customer"]                = "اختر عميل";
    m_ar["💳 CONFIRM PAYMENT"]             = "💳 تأكيد الدفع";
    m_ar["No Customers"]                   = "لا يوجد عملاء";
    m_ar["No customers found. The order will be marked as Walk-in customer."]
                                           = "لا يوجد عملاء. سيتم تسجيل الطلب كعميل عابر.";
    m_ar["Search by name or phone..."]     = "ابحث بالاسم أو الهاتف...";
    m_ar["Insufficient Cash"]              = "نقدية غير كافية";
    m_ar["Cash received is less than the total amount."]
                                           = "المبلغ المستلم أقل من الإجمالي المطلوب.";
    m_ar["No Active Session"]              = "لا توجد وردية نشطة";
    m_ar["Please open a register session before processing sales."]
                                           = "الرجاء فتح وردية صندوق قبل معالجة المبيعات.";
    m_ar["Success"]                        = "نجح";
    m_ar["Order completed successfully!"]  = "تم إتمام الطلب بنجاح!";
    m_ar["Order #"]                        = "رقم الطلب";
    m_ar["Failed to save order."]          = "فشل حفظ الطلب.";
    m_ar["Print Receipt"]                  = "طباعة الإيصال";
    m_ar["Would you like to print the receipt?"]
                                           = "هل تريد طباعة الإيصال؟";
    m_ar["Saved"]                          = "تم الحفظ";
    m_ar["Receipt saved to file"]          = "تم حفظ الإيصال في ملف";
    m_ar["Thank you for your business!"]   = "شكراً لتعاملكم معنا!";
    m_ar["Date"]                           = "التاريخ";
    m_ar["Search products or scan barcode..."]
                                           = "ابحث عن منتج أو امسح الباركود...";
    m_ar["Product Not Found"]              = "المنتج غير موجود";
    m_ar["No product found with barcode"]  = "لا يوجد منتج بالباركود";
    
    // User Management - Phase 1 Permissions
    m_ar["Phase 1 - Additional Modules"]   = "المرحلة 1 - وحدات إضافية";
    m_ar["Can Manage Purchases"]           = "إدارة المشتريات";
    m_ar["Can Access POS"]                 = "الدخول لنقطة البيع";
    m_ar["Can Manage Register"]            = "إدارة الصندوق";
    m_ar["Can Manage Expenses"]            = "إدارة المصروفات";
    m_ar["Can Manage Inventory"]           = "إدارة الجرد";
}

// ── Missing column/table translations ────────────────────────────────────────
void LangManager::loadArabicColumns() {
    // customer table headers
    m_ar["Address"]                        = "العنوان";
    m_ar["Distance"]                       = "المسافة";
    m_ar["Region"]                         = "المنطقة";
    m_ar["Debt"]                           = "الديون";

    // returns page
    m_ar["Type"]                           = "النوع";
    m_ar["Refund"]                         = "المبلغ المسترد";
    m_ar["Reason"]                         = "السبب";
    m_ar["Full"]                           = "كامل";
    m_ar["Partial"]                        = "جزئي";

    // users page
    m_ar["Username"]                       = "اسم المستخدم";
    m_ar["Role Name"]                      = "اسم الدور";

    // coupons page
    m_ar["Code"]                           = "الكود";
    m_ar["Description"]                    = "الوصف";
    m_ar["Value"]                          = "القيمة";
    m_ar["Expiry"]                         = "تاريخ الانتهاء";
    m_ar["Uses"]                           = "الاستخدام";

    // audit log
    m_ar["Timestamp"]                      = "الوقت";
    m_ar["Entity"]                         = "الكيان";
    m_ar["Entity ID"]                      = "معرّف الكيان";
    m_ar["Details"]                        = "التفاصيل";

    // reports
    m_ar["Preferred Payment"]              = "طريقة الدفع المفضلة";
    m_ar["Total Spent"]                    = "إجمالي الإنفاق";
    m_ar["Last Order"]                     = "آخر طلب";
    m_ar["Days Inactive"]                  = "أيام التوقف";
    m_ar["Total Delivery Fees"]            = "إجمالي رسوم التوصيل";
    m_ar["Qty Sold"]                       = "الكمية المباعة";
    m_ar["Revenue"]                        = "الإيراد";

    // scheduled orders
    m_ar["Schedule"]                       = "الجدول";
    m_ar["Time"]                           = "الوقت";
    m_ar["Action"]                         = "إجراء";

    // regions dialog
    m_ar["Delivery Fee"]                   = "رسوم التوصيل";
    m_ar["＋ Add Customer"]               = "＋ إضافة عميل";
    m_ar["Close"]                          = "إغلاق";

    // order statuses — display labels
    m_ar["Pending"]                        = "قيد الانتظار";
    m_ar["Out for Delivery"]               = "جاري التوصيل";
    m_ar["Delivered"]                      = "تم التوصيل";
    m_ar["Cancelled"]                      = "ملغي";
    m_ar["Out_for_Delivery"]               = "جاري التوصيل";

    // orders history
    m_ar["Delivery"]                       = "التوصيل";
    m_ar["Invoice #"]                      = "رقم الفاتورة";
    m_ar["Order #"]                        = "رقم الطلب";
}

// ── Missing button/label translations ────────────────────────────────────────
void LangManager::loadArabicButtons() {
    // customers
    m_ar["🗺 Regions"]                         = "🗺 المناطق";
    m_ar["📥 Import Data"]                     = "📥 استيراد بيانات";
    m_ar["🔍  Search by name, phone, or address..."]
                                               = "🔍  ابحث بالاسم أو الهاتف أو العنوان...";

    // products
    m_ar["♻️ Restore to Active"]              = "♻️ استعادة إلى نشط";
    m_ar["🔄 Refresh"]                         = "🔄 تحديث";
    m_ar["📥 Import"]                          = "📥 استيراد";
    m_ar["Mark selected product as Active again"]
                                               = "تعيين المنتج كنشط مجدداً";
    m_ar["Reload expired products list"]       = "إعادة تحميل القائمة";
    m_ar["Product Name"]                       = "اسم المنتج";
    m_ar["Quantity"]                           = "الكمية";
    m_ar["Safety Margin"]                      = "هامش الأمان";
    m_ar["Manufacture Date"]                   = "تاريخ الإنتاج";
    m_ar["Expiry Date"]                        = "تاريخ الانتهاء";

    // orders
    m_ar["＋ New Order"]                       = "＋ طلب جديد";
    m_ar["🧾 Invoice"]                         = "🧾 فاتورة";
    m_ar["🔍  Search by customer name or invoice # (e.g. INV-0042)..."]
                                               = "🔍  ابحث بالاسم أو رقم الفاتورة...";
    m_ar["Filter by date:"]                    = "تصفية بالتاريخ:";
    m_ar["Invoice"]                            = "الفاتورة";
    m_ar["Total"]                              = "الإجمالي";

    // scheduled
    m_ar["＋ New Schedule"]                    = "＋ جدولة جديدة";

    // reports
    m_ar["🔄  Generate"]                       = "🔄  توليد";
    m_ar["📄  PDF"]                            = "📄  PDF";
    m_ar["📊  Excel"]                          = "📊  Excel";
    m_ar["Report:"]                            = "التقرير:";
    m_ar["From:"]                              = "من:";
    m_ar["To:"]                                = "إلى:";

    // returns
    m_ar["＋ New Return"]                      = "＋ مرتجع جديد";
    m_ar["🖨 Credit Note"]                     = "🖨 إشعار دائن";

    // coupons
    m_ar["＋ Add Coupon"]                      = "＋ إضافة كوبون";

    // users
    m_ar["＋ Add User"]                        = "＋ إضافة مستخدم";
    m_ar["＋ Add Role"]                        = "＋ إضافة دور";
    m_ar["＋ Add Driver"]                      = "＋ إضافة مندوب";
    m_ar["✏️ Edit"]                            = "✏️ تعديل";
    m_ar["🗑 Delete"]                          = "🗑 حذف";

    // audit log
    m_ar["🔄  Refresh"]                        = "🔄  تحديث";
    m_ar["↩️  Undo Selected"]                 = "↩️  تراجع عن المحدد";
    m_ar["Apply"]                              = "تطبيق";
    m_ar["Clear"]                              = "مسح";
    m_ar["User:"]                              = "المستخدم:";
    m_ar["Action:"]                            = "الإجراء:";
    m_ar["Entity:"]                            = "الكيان:";
    m_ar["From:"]                              = "من:";

    // notes
    m_ar["📝  Notes  /  ملاحظات"]             = "📝  ملاحظات";
    m_ar["🔍  Search notes…"]                  = "🔍  ابحث في الملاحظات…";
    m_ar["＋  New Note"]                       = "＋  ملاحظة جديدة";
}

// ── Customer dialog + Scheduled order dialog translations ─────────────────────
void LangManager::loadArabicDialogs() {
    // customer_dialog
    m_ar["Customer"]                          = "عميل";
    m_ar["Name *"]                            = "الاسم *";
    m_ar["Full name..."]                      = "الاسم الكامل...";
    m_ar["— No Region —"]                     = "— بدون منطقة —";
    m_ar["Distance"]                          = "المسافة";
    m_ar["Debt"]                              = "الديون";
    m_ar["No debt"]                           = "لا يوجد ديون";
    m_ar["EGP "]                              = "ج.م ";
    m_ar["Optional notes..."]                 = "ملاحظات اختيارية...";
    m_ar["Preferred Payment"]                 = "طريقة الدفع المفضلة";
    m_ar["— Not specified —"]                 = "— غير محدد —";
    m_ar["🔖 Other (specify)"]               = "🔖 أخرى (حدد)";
    m_ar["Please specify:"]                   = "حدد الطريقة:";
    m_ar["Phone number..."]                   = "رقم الهاتف...";
    m_ar["Address..."]                        = "العنوان...";
    m_ar["Add"]                               = "إضافة";
    m_ar["Remove"]                            = "حذف";
    m_ar["🔍  Search by name or barcode..."] = "🔍  ابحث بالاسم أو الباركود...";
    m_ar["Hesitant"]                          = "متردد";
    m_ar["Suspended"]                         = "موقوف";

    // scheduled_order_dialog
    m_ar["Scheduled / Recurring Order"]       = "طلب مجدول / متكرر";
    m_ar["Order Details"]                     = "تفاصيل الطلب";
    m_ar["Customer *"]                        = "العميل *";
    m_ar["🔍  Type customer name..."]         = "🔍  اكتب اسم العميل...";
    m_ar["Delivery Time *"]                   = "وقت التوصيل *";
    m_ar["Schedule Type"]                     = "نوع الجدول";
    m_ar["🔁  Repeat weekly (recurring)"]    = "🔁  تكرار أسبوعي (متكرر)";
    m_ar["Date *"]                            = "التاريخ *";
    m_ar["Order will be created once on this date."]
                                              = "سيُنشأ الطلب مرة واحدة في هذا التاريخ.";
    m_ar["Reminder & Auto-Create Settings"]   = "إعدادات التذكير والإنشاء التلقائي";
    m_ar["⏰ Remind me:"]                     = "⏰ ذكّرني:";
    m_ar["  min before delivery"]             = "  دقيقة قبل التوصيل";
    m_ar["🔁 Repeat every:"]                 = "🔁 تكرار كل:";
    m_ar["Once only (no repeat)"]             = "مرة واحدة فقط";
    m_ar["  min  (repeat interval)"]          = "  دقيقة (فترة التكرار)";
    m_ar["🤖 Auto-create:"]                   = "🤖 إنشاء تلقائي:";
    m_ar["Automatically create a pending order at reminder time\n(order will appear in Orders tab — fill remaining details there)"]
                                              = "إنشاء طلب معلق تلقائياً عند التذكير\n(سيظهر في تاب الطلبات — أكمل البيانات هناك)";
    m_ar["Fixed Products for this Order"]     = "المنتجات الثابتة لهذا الطلب";
    m_ar["＋ Add Product"]                    = "＋ إضافة منتج";
    m_ar["Product (name or barcode)"]         = "المنتج (اسم أو باركود)";
    m_ar["Unit Price"]                        = "سعر الوحدة";
    m_ar["Type name or barcode..."]           = "اكتب الاسم أو الباركود...";

    // missing products tab
    m_ar["＋ Report Missing Product"]         = "＋ إبلاغ عن منتج ناقص";
    m_ar["✓ Toggle Resolved"]                 = "✓ تبديل الحالة";
    m_ar["🔍  Search by name or barcode..."]  = "🔍  ابحث بالاسم أو الباركود...";
    
    // Promotions
    m_ar["promotion_title"]                   = "العنوان";
    m_ar["promotion_type"]                    = "النوع";
    m_ar["promotion_discount"]                = "الخصم";
    m_ar["promotion_start_date"]              = "تاريخ البدء";
    m_ar["promotion_end_date"]                = "تاريخ الانتهاء";
    m_ar["promotion_status"]                  = "الحالة";
    m_ar["promotion_product"]                 = "المنتج";
    m_ar["promotion_category"]                = "الفئة";
    m_ar["promotion_type_magazine"]           = "مجلة";
    m_ar["promotion_type_percentage"]         = "نسبة مئوية";
    m_ar["promotion_type_fixed"]              = "مبلغ ثابت";
    m_ar["promotion_type_bogo"]               = "اشتر X واحصل على Y";
    m_ar["promotion_type_freeship"]           = "شحن مجاني";
    m_ar["promotion_status_active"]           = "نشط";
    m_ar["promotion_status_inactive"]         = "غير نشط";
    m_ar["promotion_status_scheduled"]        = "مجدول";
    m_ar["promotion_status_expired"]          = "منتهي";
    m_ar["add_promotion"]                     = "إضافة مجلة";
    m_ar["edit_promotion"]                    = "تعديل مجلة";
    m_ar["delete_promotion"]                  = "حذف مجلة";
    m_ar["delete_promotion_confirm"]          = "هل أنت متأكد من حذف المجلة '%1'؟";
    m_ar["please_select_promotion"]           = "الرجاء تحديد مجلة";
    m_ar["promotion_not_found"]               = "المجلة غير موجودة";
    m_ar["promotion_created_successfully"]    = "تم إنشاء المجلة بنجاح";
    m_ar["promotion_updated_successfully"]    = "تم تحديث المجلة بنجاح";
    m_ar["promotion_deleted_successfully"]    = "تم حذف المجلة بنجاح";
    m_ar["promotion_deleted"]                 = "تم حذف المجلة بنجاح";
    m_ar["promotion_delete_failed"]           = "فشل حذف المجلة";
    m_ar["failed_to_save_promotion"]          = "فشل حفظ المجلة";
    m_ar["failed_to_delete_promotion"]        = "فشل حذف المجلة";
    m_ar["search_promotions"]                 = "ابحث في المجلات...";
    m_ar["Promotions Magazine"]               = "مجلة العروض";
    m_ar["Add Products to Magazine"]          = "إضافة منتجات للمجلة";
    m_ar["Current Price"]                     = "السعر الحالي";
    m_ar["Magazine Price"]                    = "سعر المجلة";
    
    // Magazine Dialog
    m_ar["magazine_title"]                    = "عنوان المجلة";
    m_ar["magazine_description"]              = "الوصف";
    m_ar["magazine_start_date"]               = "تاريخ البدء";
    m_ar["magazine_end_date"]                 = "تاريخ الانتهاء";
    m_ar["magazine_status"]                   = "الحالة";
    m_ar["magazine_add_products"]             = "إضافة منتجات للمجلة";
    m_ar["magazine_search_products"]          = "ابحث عن منتجات...";
    m_ar["magazine_add_product_btn"]          = "إضافة";
    m_ar["magazine_current_price"]            = "السعر الحالي";
    m_ar["magazine_price"]                    = "سعر المجلة";
    m_ar["magazine_remove_product"]           = "حذف";
    m_ar["magazine_new"]                      = "إنشاء مجلة جديدة";
    m_ar["magazine_edit"]                     = "تعديل";
    m_ar["magazine_delete"]                   = "حذف";
    
    // Custom Theme Editor
    m_ar["custom_theme_editor"]               = "محرر الثيم المخصص";
    m_ar["pick_color"]                        = "اختر اللون";
    m_ar["load_from_light"]                   = "تحميل من Light";
    m_ar["load_from_dark"]                    = "تحميل من Dark";
    m_ar["surfaces"]                          = "الأسطح";
    m_ar["borders"]                           = "الحدود";
    m_ar["text_colors"]                       = "ألوان النصوص";
    m_ar["primary_colors"]                    = "الألوان الأساسية";
    m_ar["semantic_colors"]                   = "الألوان الدلالية";
    m_ar["table_colors"]                      = "ألوان الجداول";
    m_ar["navigation_colors"]                 = "ألوان التنقل";
    m_ar["scrollbar_colors"]                  = "ألوان شريط التمرير";
    m_ar["preview"]                           = "معاينة";
    m_ar["select_color"]                      = "اختر اللون";
    m_ar["sample_card"]                       = "كارت عينة";
    m_ar["this_is_preview"]                   = "هذه معاينة للثيم المخصص";
    m_ar["sample_input"]                      = "حقل إدخال عينة";
    m_ar["sample_button"]                     = "زر عينة";
    m_ar["navigation"]                        = "التنقل";
    m_ar["selected_item"]                     = "عنصر محدد";
    m_ar["custom_theme_saved"]                = "تم حفظ الثيم المخصص بنجاح!";
    m_ar["customize_theme"]                   = "تخصيص الثيم";
    m_ar["svg_icon_mode"]                     = "وضع أيقونات SVG";
    m_ar["svg_icon_mode_hint"]                = "اختر لون الأيقونات: Light (أسود) أو Dark (أبيض) أو Custom (لون مخصص). يؤثر على جميع الأيقونات في البرنامج.";
    
    // Password Reset Dialog
    m_ar["forgot_password_title"]             = "نسيت كلمة المرور";
    m_ar["enter_your_email"]                  = "أدخل بريدك الإلكتروني";
    m_ar["email_recovery_desc"]               = "سنرسل لك رمز تحقق على بريدك الإلكتروني.";
    m_ar["email_address"]                     = "البريد الإلكتروني";
    m_ar["send_verification_code"]            = "إرسال رمز التحقق";
    m_ar["sending"]                           = "جاري الإرسال...";
    m_ar["invalid_email_format"]              = "البريد الإلكتروني غير صحيح";
    m_ar["please_wait_seconds"]               = "الرجاء الانتظار %1 ثانية قبل المحاولة مرة أخرى";
    m_ar["network_error"]                     = "خطأ في الاتصال بالإنترنت";
    m_ar["otp_sent_generic_message"]          = "تم إرسال رمز التحقق! تحقق من بريدك الإلكتروني.";
    m_ar["enter_verification_code"]           = "أدخل رمز التحقق";
    m_ar["otp_sent_desc"]                     = "لقد أرسلنا رمزاً مكوناً من 6 أرقام إلى بريدك الإلكتروني.";
    m_ar["verification_code"]                 = "رمز التحقق";
    m_ar["verify_code"]                       = "تحقق من الرمز";
    m_ar["verifying"]                         = "جاري التحقق...";
    m_ar["resend_code"]                       = "إعادة إرسال الرمز";
    m_ar["otp_must_be_6_digits"]              = "الرمز يجب أن يكون 6 أرقام";
    m_ar["incorrect_otp_remaining"]           = "الرمز غير صحيح. محاولات متبقية: %1";
    m_ar["otp_attempts_exhausted"]            = "تم استنفاد كل المحاولات. الرجاء إعادة إرسال رمز جديد.";
    m_ar["remaining_attempts"]                = "المحاولات المتبقية: %1";
    m_ar["time_remaining"]                    = "الوقت المتبقي: %1:%2";
    m_ar["otp_expired"]                       = "انتهت صلاحية الرمز";
    m_ar["please_wait"]                       = "الرجاء الانتظار";
    m_ar["set_new_password"]                  = "تعيين كلمة مرور جديدة";
    m_ar["new_password_desc"]                 = "أدخل كلمة المرور الجديدة (لا تقل عن 8 أحرف).";
    m_ar["new_password"]                      = "كلمة المرور الجديدة";
    m_ar["min_8_chars"]                       = "8 أحرف على الأقل";
    m_ar["confirm_password"]                  = "تأكيد كلمة المرور";
    m_ar["retype_password"]                   = "أعد كتابة كلمة المرور";
    m_ar["reset_password"]                    = "إعادة تعيين كلمة المرور";
    m_ar["password_required"]                 = "كلمة المرور مطلوبة";
    m_ar["password_min_8_chars"]              = "كلمة المرور يجب أن تكون 8 أحرف على الأقل";
    m_ar["passwords_dont_match"]              = "كلمة المرور غير متطابقة";
    m_ar["password_reset_failed"]             = "فشل إعادة تعيين كلمة المرور";
    m_ar["password_reset_success"]            = "تم إعادة تعيين كلمة المرور بنجاح! يمكنك الآن تسجيل الدخول.";
}