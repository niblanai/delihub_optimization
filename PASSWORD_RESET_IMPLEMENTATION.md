# نظام إعادة تعيين كلمة المرور - دليل الاستخدام

## البنية المعمارية الجديدة

تم تحويل نظام إعادة تعيين كلمة المرور من الاتصال المباشر بـ Brevo API إلى استخدام Cloudflare Worker كـ Proxy آمن:

```
C++ DeliHub → Cloudflare Worker → Brevo API
```

### المزايا الأمنية:
- ✅ مفتاح Brevo API مخفي تماماً (موجود فقط في Worker Secret)
- ✅ لا يمكن استخراج المفتاح من البرنامج أو من GitHub
- ✅ يمكن تدوير (rotate) المفتاح من Cloudflare دون إعادة بناء البرنامج
- ✅ Rate limiting مدمج في Worker
- ✅ تخزين آمن لـ OTP في Cloudflare KV مع hash + expiry

---

## الملفات المُعدلة

### 1. **src/ui/login/forgot_password_dialog.h**
- إضافة `QNetworkAccessManager*` للاتصال بالـ Worker
- إضافة `m_remainingAttempts` لتتبع المحاولات المتبقية
- حذف `onEmailSent()` slot (لم يعد مطلوباً)

### 2. **src/ui/login/forgot_password_dialog.cpp**
- استبدال `BrevoEmailService` و `OtpManager` بـ **QNetworkRequest**
- تحديث `onRequestOtp()`:
  - إرسال POST request إلى `/request-reset`
  - معالجة الرد من Worker (success/error)
- تحديث `onVerifyOtp()`:
  - إرسال POST request إلى `/verify-otp`
  - معالجة remainingAttempts من Worker
- حذف الاعتماد على `OtpManager::instance()` و `BrevoEmailService`

### 3. **src/services/lang_manager.cpp**
- إضافة 30 ترجمة عربية جديدة لواجهة إعادة تعيين كلمة المرور:
  - `forgot_password_title`, `enter_your_email`, `send_verification_code`
  - `verify_code`, `resend_code`, `set_new_password`
  - `otp_expired`, `otp_attempts_exhausted`, `password_reset_success`
  - وغيرها...

---

## Cloudflare Worker Endpoints

### 1. **POST /request-reset**
إرسال OTP للبريد الإلكتروني

**Request:**
```json
{
  "email": "user@example.com"
}
```

**Response (Success):**
```json
{
  "success": true,
  "message": "OTP sent successfully"
}
```

**Response (Error):**
```json
{
  "success": false,
  "error": "Rate limit exceeded"
}
```

---

### 2. **POST /verify-otp**
التحقق من صحة OTP

**Request:**
```json
{
  "email": "user@example.com",
  "otp": "123456"
}
```

**Response (Success):**
```json
{
  "success": true,
  "message": "OTP verified successfully"
}
```

**Response (Failed):**
```json
{
  "success": false,
  "error": "Invalid OTP",
  "remainingAttempts": 4
}
```

---

### 3. **GET /test-email**
اختبار اتصال Brevo API (للتطوير فقط)

**Request:**
```
GET /test-email?email=user@example.com
```

**Response:**
```json
{
  "success": true,
  "message": "Test email sent successfully"
}
```

---

## الإعدادات المطلوبة في Cloudflare

### 1. Worker Environment Variables:
```
SENDER_EMAIL = hosamwork2003@gmail.com
OTP_LENGTH = 6
OTP_EXPIRY_MINUTES = 10
MAX_ATTEMPTS = 5
```

### 2. Worker Secrets:
```
BREVO_API_KEY = xkeysib-XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```

### 3. KV Namespace Binding:
```
PASSWORD_RESET_KV (binding name)
```

---

## سير العمل (Workflow)

### المرحلة 1: طلب OTP
1. المستخدم يدخل بريده الإلكتروني في `forgot_password_dialog`
2. C++ يرسل POST request إلى Worker `/request-reset`
3. Worker:
   - يتحقق من Rate limiting
   - يولد OTP من 6 أرقام
   - يخزن hash(OTP) في KV مع expiry = 10 دقائق
   - يرسل OTP عبر Brevo API إلى البريد الإلكتروني
4. المستخدم يتلقى الإيميل مع OTP

### المرحلة 2: التحقق من OTP
1. المستخدم يدخل OTP في الواجهة
2. C++ يرسل POST request إلى Worker `/verify-otp`
3. Worker:
   - يستخرج hash(OTP) من KV
   - يقارن hash(entered_OTP) مع hash المحفوظ
   - يقلل عداد المحاولات
   - يرد بـ success/error + remainingAttempts
4. إذا كان OTP صحيح → الانتقال للمرحلة 3

### المرحلة 3: إعادة تعيين كلمة المرور
1. المستخدم يدخل كلمة المرور الجديدة
2. C++ يُخزن كلمة المرور بعد hash + salt في قاعدة البيانات المحلية (SQLite/Access)
3. يتم عرض رسالة نجاح وإغلاق الـ dialog

---

## الأمان والحماية

### 1. **Rate Limiting**
- حد أقصى لعدد الطلبات لكل IP/Email في فترة زمنية معينة
- يمنع Brute Force attacks

### 2. **OTP Security**
- يتم تخزين hash(OTP) فقط وليس OTP نفسه في KV
- Expiry تلقائي بعد 10 دقائق
- حد أقصى 5 محاولات خاطئة

### 3. **API Key Protection**
- المفتاح موجود فقط في Cloudflare Worker Secret
- لا يمكن الوصول إليه من C++ code أو GitHub
- يمكن تدويره بسهولة دون إعادة بناء البرنامج

### 4. **HTTPS Enforcement**
- جميع الاتصالات بين C++ و Worker عبر HTTPS
- SSL/TLS certificates من Cloudflare

---

## التجربة والاختبار

### اختبار الـ Worker مباشرةً:
```powershell
# Test email endpoint
Invoke-WebRequest -Uri "https://erp-password-recovery.hosamwork2003.workers.dev/test-email?email=your-email@example.com" -Method GET

# Test request-reset
$body = @{ email = "your-email@example.com" } | ConvertTo-Json
Invoke-WebRequest -Uri "https://erp-password-recovery.hosamwork2003.workers.dev/request-reset" -Method POST -Body $body -ContentType "application/json"

# Test verify-otp
$body = @{ email = "your-email@example.com"; otp = "123456" } | ConvertTo-Json
Invoke-WebRequest -Uri "https://erp-password-recovery.hosamwork2003.workers.dev/verify-otp" -Method POST -Body $body -ContentType "application/json"
```

### اختبار من داخل DeliHub:
1. افتح البرنامج
2. في دايلوج تسجيل الدخول، اضغط "نسيت كلمة المرور"
3. أدخل بريد إلكتروني موجود في قاعدة البيانات
4. اضغط "إرسال رمز التحقق"
5. افتح بريدك وانسخ OTP
6. الصق OTP في الواجهة
7. أدخل كلمة المرور الجديدة

---

## الملفات المحذوفة (لم تعد مطلوبة)

بعد هذا التحديث، الملفات التالية **لم تعد مستخدمة** ويمكن حذفها (اختياري):

- ~~`src/services/brevo_email_service.h`~~
- ~~`src/services/brevo_email_service.cpp`~~
- ~~`src/services/otp_manager.h`~~
- ~~`src/services/otp_manager.cpp`~~

**ملاحظة:** لم نحذف هذه الملفات تلقائياً للحفاظ على التوافق مع الإصدارات السابقة. إذا كنت متأكداً من عدم الحاجة إليها، يمكنك حذفها يدوياً.

---

## الخطوات التالية

✅ **تم:**
- تحديث forgot_password_dialog للاتصال بـ Worker
- إضافة ترجمات عربية كاملة
- بناء البرنامج بنجاح (77.99 MB)
- Installer جاهز: `E:\tifany\installer_output\DeliHub_v2.3.0_Setup.exe`

🔜 **قريباً:**
- تحسين الأداء (Performance Optimization) باستخدام 3 skills من GitHub
- المهام المتبقية من القائمة (7: Dashboard, 8: Encryption/Trial, 9: Settings, 10: Suppliers)

---

## دعم فني

إذا واجهت أي مشاكل:
1. تحقق من Cloudflare Worker logs
2. افحص crash_report.txt في مجلد البرنامج
3. تأكد من أن البريد الإلكتروني موثق في Brevo
4. تحقق من إعدادات Firewall/Antivirus (قد تحظر الاتصال بـ Worker)
