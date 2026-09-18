#include "config_manager.h"
#include "logger.h"
#include <QSettings>
#include <QFile>
#include <QDir>

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

// Private helper: override map takes precedence over QSettings
QString ConfigManager::value(const QString& key, const QString& defaultVal) const {
    if (m_overrides.contains(key))
        return m_overrides.value(key);
    if (m_settings)
        return m_settings->value(key, defaultVal).toString();
    return defaultVal;
}

void ConfigManager::init(const QString& filePath) {
    m_filePath = filePath;
    m_settings = std::make_unique<QSettings>(filePath, QSettings::IniFormat);
    Logger::instance().info("Configuration loaded from " + filePath);
}

QString ConfigManager::databaseType() const {
    return value("Database/Type", "SQLite");
}

QString ConfigManager::odbcConnectionString() const {
    return value("Database/ConnectionString", "");
}

QString ConfigManager::sqlitePath() const {
    return value("Database/SQLitePath", "delivery_system.db");
}

int ConfigManager::branchId() const {
    bool ok = false;
    int v = value("Branch/ID", "1").toInt(&ok);
    return ok ? v : 1;
}

QString ConfigManager::branchName() const {
    return value("Branch/Name", "Main Branch");
}

QString ConfigManager::currencySymbol() const {
    QString v = value("System/Currency", "£");
    // If empty or still the old $ default, return £
    return (v.isEmpty() || v == "$") ? "£" : v;
}

// Setters: write to override map (immediate effect) AND persist to QSettings if loaded
void ConfigManager::setDatabaseType(const QString& type) {
    m_overrides["Database/Type"] = type;
    if (m_settings) m_settings->setValue("Database/Type", type);
    Logger::instance().info("Config: Database Type = " + type);
}

void ConfigManager::setOdbcConnectionString(const QString& connStr) {
    m_overrides["Database/ConnectionString"] = connStr;
    if (m_settings) m_settings->setValue("Database/ConnectionString", connStr);
}

void ConfigManager::setSqlitePath(const QString& path) {
    m_overrides["Database/SQLitePath"] = path;
    if (m_settings) m_settings->setValue("Database/SQLitePath", path);
    Logger::instance().info("Config: SQLite Path = " + path);
}

void ConfigManager::setBranchId(int id) {
    m_overrides["Branch/ID"] = QString::number(id);
    if (m_settings) m_settings->setValue("Branch/ID", id);
}

void ConfigManager::setBranchName(const QString& name) {
    m_overrides["Branch/Name"] = name;
    if (m_settings) m_settings->setValue("Branch/Name", name);
}

void ConfigManager::setCurrencySymbol(const QString& symbol) {
    m_overrides["System/Currency"] = symbol;
    if (m_settings) m_settings->setValue("System/Currency", symbol);
}

QString ConfigManager::theme() const {
    return value("System/Theme", "dark");
}

QString ConfigManager::language() const {
    return value("System/Language", "en");
}

void ConfigManager::setTheme(const QString& theme) {
    m_overrides["System/Theme"] = theme;
    if (m_settings) m_settings->setValue("System/Theme", theme);
}

void ConfigManager::setLanguage(const QString& lang) {
    m_overrides["System/Language"] = lang;
    if (m_settings) m_settings->setValue("System/Language", lang);
}

// ── Company / Invoice settings ────────────────────────────────────────────────
QString ConfigManager::companyName()      const { return value("Company/Name",      "DeliHub"); }
QString ConfigManager::companyAddress()   const { return value("Company/Address",   ""); }
QString ConfigManager::companyPhone()     const { return value("Company/Phone",     ""); }
QString ConfigManager::companyTaxNumber() const { return value("Company/TaxNumber", ""); }
QString ConfigManager::invoiceLogoPath()  const { return value("Company/LogoPath",  ""); }
QString ConfigManager::receiptLogoPath()  const { return value("Company/ReceiptLogoPath",  ""); }
int     ConfigManager::nextInvoiceNumber() const {
    bool ok; int n = value("Invoice/NextNumber", "1000").toInt(&ok);
    return ok ? n : 1000;
}
void ConfigManager::incrementInvoiceNumber() {
    int n = nextInvoiceNumber() + 1;
    m_overrides["Invoice/NextNumber"] = QString::number(n);
    if (m_settings) m_settings->setValue("Invoice/NextNumber", n);
}

// Tax settings
double ConfigManager::taxRate() const {
    QString val = value("Tax/Rate", "0");
    bool ok = false;
    double rate = val.toDouble(&ok);
    return ok ? rate : 0.0;
}
void ConfigManager::setTaxRate(double rate) {
    m_overrides["Tax/Rate"] = QString::number(rate, 'f', 2);
    if (m_settings) m_settings->setValue("Tax/Rate", rate);
}

void ConfigManager::setCompanyName(const QString& v)      { m_overrides["Company/Name"]      = v; if (m_settings) m_settings->setValue("Company/Name",      v); }
void ConfigManager::setCompanyAddress(const QString& v)   { m_overrides["Company/Address"]   = v; if (m_settings) m_settings->setValue("Company/Address",   v); }
void ConfigManager::setCompanyPhone(const QString& v)     { m_overrides["Company/Phone"]     = v; if (m_settings) m_settings->setValue("Company/Phone",     v); }
void ConfigManager::setCompanyTaxNumber(const QString& v) { m_overrides["Company/TaxNumber"] = v; if (m_settings) m_settings->setValue("Company/TaxNumber", v); }
void ConfigManager::setInvoiceLogoPath(const QString& v)  { m_overrides["Company/LogoPath"]  = v; if (m_settings) m_settings->setValue("Company/LogoPath",  v); }
void ConfigManager::setReceiptLogoPath(const QString& v)  { m_overrides["Company/ReceiptLogoPath"]  = v; if (m_settings) m_settings->setValue("Company/ReceiptLogoPath",  v); }

// Auto-backup settings
QString ConfigManager::autoBackupTime()     const { return value("Backup/DailyTime",       "02:00"); }
QString ConfigManager::autoBackupDir()      const { return value("Backup/AutoDir",          ""); }
QString ConfigManager::lastAutoBackupDate() const { return value("Backup/LastAutoBackup",   ""); }
void ConfigManager::setLastAutoBackupDate(const QString& d) {
    m_overrides["Backup/LastAutoBackup"] = d;
    if (m_settings) m_settings->setValue("Backup/LastAutoBackup", d);
}
void ConfigManager::setAutoBackupTime(const QString& t) {
    m_overrides["Backup/DailyTime"] = t;
    if (m_settings) m_settings->setValue("Backup/DailyTime", t);
}
void ConfigManager::setAutoBackupDir(const QString& d) {
    m_overrides["Backup/AutoDir"] = d;
    if (m_settings) m_settings->setValue("Backup/AutoDir", d);
}

// ── Notes board background ────────────────────────────────────────────────────
QString ConfigManager::notesBgMode()      const { return value("Notes/BgMode",      "solid"); }
QString ConfigManager::notesBgImagePath() const { return value("Notes/BgImagePath", ""); }
void ConfigManager::setNotesBgMode(const QString& mode) {
    m_overrides["Notes/BgMode"] = mode;
    if (m_settings) m_settings->setValue("Notes/BgMode", mode);
}
void ConfigManager::setNotesBgImagePath(const QString& path) {
    m_overrides["Notes/BgImagePath"] = path;
    if (m_settings) m_settings->setValue("Notes/BgImagePath", path);
}

QString ConfigManager::receiptTemplateJson(int paperWidthMm) const {
    const QString key = QString("Receipt/Template_%1mm").arg(paperWidthMm <= 58 ? 58 : 80);
    return value(key, "");
}
void ConfigManager::setReceiptTemplateJson(int paperWidthMm, const QString& json) {
    const QString key = QString("Receipt/Template_%1mm").arg(paperWidthMm <= 58 ? 58 : 80);
    m_overrides[key] = json;
    if (m_settings) { m_settings->setValue(key, json); m_settings->sync(); }
}
int ConfigManager::receiptPaperWidthMm() const {
    bool ok = false;
    int mm = value("Receipt/PaperWidthMm", "80").toInt(&ok);
    return (ok && mm == 58) ? 58 : 80;
}
void ConfigManager::setReceiptPaperWidthMm(int mm) {
    const int clamped = (mm == 58) ? 58 : 80;
    m_overrides["Receipt/PaperWidthMm"] = QString::number(clamped);
    if (m_settings) m_settings->setValue("Receipt/PaperWidthMm", clamped);
}
QString ConfigManager::receiptPrinterName() const {
    return value("Receipt/PrinterName", "");
}
void ConfigManager::setReceiptPrinterName(const QString& name) {
    m_overrides["Receipt/PrinterName"] = name;
    if (m_settings) m_settings->setValue("Receipt/PrinterName", name);
}

bool ConfigManager::requireAuthForPOSDeletion() const {
    if (m_overrides.contains("requireAuthForPOSDeletion"))
        return m_overrides.value("requireAuthForPOSDeletion") == "true";
    if (m_settings)
        return m_settings->value("POS/requireAuthForDeletion", false).toBool();
    return false;
}

void ConfigManager::setRequireAuthForPOSDeletion(bool enabled) {
    m_overrides["requireAuthForPOSDeletion"] = enabled ? "true" : "false";
    if (m_settings) {
        m_settings->setValue("POS/requireAuthForDeletion", enabled);
        m_settings->sync();
    }
}

// ── Brevo Email Service ───────────────────────────────────────────────────
QString ConfigManager::brevoApiKey() const {
    // Embedded API key for production use
    // This is intentional for this controlled-distribution desktop application
    return "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";
}

QString ConfigManager::brevoSenderEmail() const {
    return "hosamwork2003@gmail.com";
}

QString ConfigManager::brevoSenderName() const {
    return "DeliHub";
}

void ConfigManager::setBrevoApiKey(const QString& key) {
    // Method kept for API compatibility but does nothing
    // API key is embedded
    Q_UNUSED(key);
}

void ConfigManager::setBrevoSenderEmail(const QString& email) {
    // Method kept for API compatibility but does nothing
    // Sender email is embedded
    Q_UNUSED(email);
}

void ConfigManager::setBrevoSenderName(const QString& name) {
    m_overrides["Brevo/senderName"] = name;
    if (m_settings) {
        m_settings->setValue("Brevo/senderName", name);
        m_settings->sync();
    }
}

// ── Receipt Template Designer ─────────────────────────────────────────────

// ══════════════════════════════════════════════════════════════════════════════
// Custom Theme Settings
// ══════════════════════════════════════════════════════════════════════════════

QString ConfigManager::customTheme_windowBg() const        { return value("CustomTheme/windowBg", "#F8FAFC"); }
QString ConfigManager::customTheme_sidebarBg() const       { return value("CustomTheme/sidebarBg", "#FFFFFF"); }
QString ConfigManager::customTheme_cardBg() const          { return value("CustomTheme/cardBg", "#FFFFFF"); }
QString ConfigManager::customTheme_cardHover() const       { return value("CustomTheme/cardHover", "#F1F5F9"); }
QString ConfigManager::customTheme_inputBg() const         { return value("CustomTheme/inputBg", "#FFFFFF"); }
QString ConfigManager::customTheme_dialogBg() const        { return value("CustomTheme/dialogBg", "#FFFFFF"); }
QString ConfigManager::customTheme_border() const          { return value("CustomTheme/border", "#E5E7EB"); }
QString ConfigManager::customTheme_borderFocus() const     { return value("CustomTheme/borderFocus", "#3B82F6"); }
QString ConfigManager::customTheme_textPrimary() const     { return value("CustomTheme/textPrimary", "#0F172A"); }
QString ConfigManager::customTheme_textSecondary() const   { return value("CustomTheme/textSecondary", "#64748B"); }
QString ConfigManager::customTheme_textDisabled() const    { return value("CustomTheme/textDisabled", "#CBD5E1"); }
QString ConfigManager::customTheme_textOnPrimary() const   { return value("CustomTheme/textOnPrimary", "#FFFFFF"); }
QString ConfigManager::customTheme_primary() const         { return value("CustomTheme/primary", "#3B82F6"); }
QString ConfigManager::customTheme_primaryHover() const    { return value("CustomTheme/primaryHover", "#2563EB"); }
QString ConfigManager::customTheme_primaryPressed() const  { return value("CustomTheme/primaryPressed", "#1D4ED8"); }
QString ConfigManager::customTheme_success() const         { return value("CustomTheme/success", "#22C55E"); }
QString ConfigManager::customTheme_successBg() const       { return value("CustomTheme/successBg", "#DCFCE7"); }
QString ConfigManager::customTheme_warning() const         { return value("CustomTheme/warning", "#F59E0B"); }
QString ConfigManager::customTheme_warningBg() const       { return value("CustomTheme/warningBg", "#FEF3C7"); }
QString ConfigManager::customTheme_danger() const          { return value("CustomTheme/danger", "#EF4444"); }
QString ConfigManager::customTheme_dangerBg() const        { return value("CustomTheme/dangerBg", "#FEE2E2"); }
QString ConfigManager::customTheme_tableHeaderBg() const   { return value("CustomTheme/tableHeaderBg", "#F1F5F9"); }
QString ConfigManager::customTheme_tableHeaderText() const { return value("CustomTheme/tableHeaderText", "#475569"); }
QString ConfigManager::customTheme_tableRowAlt() const     { return value("CustomTheme/tableRowAlt", "#F8FAFC"); }
QString ConfigManager::customTheme_tableRowHover() const   { return value("CustomTheme/tableRowHover", "#EFF6FF"); }
QString ConfigManager::customTheme_tableSelected() const   { return value("CustomTheme/tableSelected", "#DBEAFE"); }
QString ConfigManager::customTheme_tableBorder() const     { return value("CustomTheme/tableBorder", "#E5E7EB"); }
QString ConfigManager::customTheme_navBg() const           { return value("CustomTheme/navBg", "#FFFFFF"); }
QString ConfigManager::customTheme_navHover() const        { return value("CustomTheme/navHover", "#F1F5F9"); }
QString ConfigManager::customTheme_navSelected() const     { return value("CustomTheme/navSelected", "#3B82F6"); }
QString ConfigManager::customTheme_navSelectedText() const { return value("CustomTheme/navSelectedText", "#FFFFFF"); }
QString ConfigManager::customTheme_navText() const         { return value("CustomTheme/navText", "#64748B"); }
QString ConfigManager::customTheme_scrollHandle() const    { return value("CustomTheme/scrollHandle", "#CBD5E1"); }
QString ConfigManager::customTheme_scrollTrack() const     { return value("CustomTheme/scrollTrack", "#F1F5F9"); }

void ConfigManager::setCustomTheme_windowBg(const QString& v)        { m_overrides["CustomTheme/windowBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/windowBg", v); }
void ConfigManager::setCustomTheme_sidebarBg(const QString& v)       { m_overrides["CustomTheme/sidebarBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/sidebarBg", v); }
void ConfigManager::setCustomTheme_cardBg(const QString& v)          { m_overrides["CustomTheme/cardBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/cardBg", v); }
void ConfigManager::setCustomTheme_cardHover(const QString& v)       { m_overrides["CustomTheme/cardHover"] = v; if (m_settings) m_settings->setValue("CustomTheme/cardHover", v); }
void ConfigManager::setCustomTheme_inputBg(const QString& v)         { m_overrides["CustomTheme/inputBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/inputBg", v); }
void ConfigManager::setCustomTheme_dialogBg(const QString& v)        { m_overrides["CustomTheme/dialogBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/dialogBg", v); }
void ConfigManager::setCustomTheme_border(const QString& v)          { m_overrides["CustomTheme/border"] = v; if (m_settings) m_settings->setValue("CustomTheme/border", v); }
void ConfigManager::setCustomTheme_borderFocus(const QString& v)     { m_overrides["CustomTheme/borderFocus"] = v; if (m_settings) m_settings->setValue("CustomTheme/borderFocus", v); }
void ConfigManager::setCustomTheme_textPrimary(const QString& v)     { m_overrides["CustomTheme/textPrimary"] = v; if (m_settings) m_settings->setValue("CustomTheme/textPrimary", v); }
void ConfigManager::setCustomTheme_textSecondary(const QString& v)   { m_overrides["CustomTheme/textSecondary"] = v; if (m_settings) m_settings->setValue("CustomTheme/textSecondary", v); }
void ConfigManager::setCustomTheme_textDisabled(const QString& v)    { m_overrides["CustomTheme/textDisabled"] = v; if (m_settings) m_settings->setValue("CustomTheme/textDisabled", v); }
void ConfigManager::setCustomTheme_textOnPrimary(const QString& v)   { m_overrides["CustomTheme/textOnPrimary"] = v; if (m_settings) m_settings->setValue("CustomTheme/textOnPrimary", v); }
void ConfigManager::setCustomTheme_primary(const QString& v)         { m_overrides["CustomTheme/primary"] = v; if (m_settings) m_settings->setValue("CustomTheme/primary", v); }
void ConfigManager::setCustomTheme_primaryHover(const QString& v)    { m_overrides["CustomTheme/primaryHover"] = v; if (m_settings) m_settings->setValue("CustomTheme/primaryHover", v); }
void ConfigManager::setCustomTheme_primaryPressed(const QString& v)  { m_overrides["CustomTheme/primaryPressed"] = v; if (m_settings) m_settings->setValue("CustomTheme/primaryPressed", v); }
void ConfigManager::setCustomTheme_success(const QString& v)         { m_overrides["CustomTheme/success"] = v; if (m_settings) m_settings->setValue("CustomTheme/success", v); }
void ConfigManager::setCustomTheme_successBg(const QString& v)       { m_overrides["CustomTheme/successBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/successBg", v); }
void ConfigManager::setCustomTheme_warning(const QString& v)         { m_overrides["CustomTheme/warning"] = v; if (m_settings) m_settings->setValue("CustomTheme/warning", v); }
void ConfigManager::setCustomTheme_warningBg(const QString& v)       { m_overrides["CustomTheme/warningBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/warningBg", v); }
void ConfigManager::setCustomTheme_danger(const QString& v)          { m_overrides["CustomTheme/danger"] = v; if (m_settings) m_settings->setValue("CustomTheme/danger", v); }
void ConfigManager::setCustomTheme_dangerBg(const QString& v)        { m_overrides["CustomTheme/dangerBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/dangerBg", v); }
void ConfigManager::setCustomTheme_tableHeaderBg(const QString& v)   { m_overrides["CustomTheme/tableHeaderBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableHeaderBg", v); }
void ConfigManager::setCustomTheme_tableHeaderText(const QString& v) { m_overrides["CustomTheme/tableHeaderText"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableHeaderText", v); }
void ConfigManager::setCustomTheme_tableRowAlt(const QString& v)     { m_overrides["CustomTheme/tableRowAlt"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableRowAlt", v); }
void ConfigManager::setCustomTheme_tableRowHover(const QString& v)   { m_overrides["CustomTheme/tableRowHover"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableRowHover", v); }
void ConfigManager::setCustomTheme_tableSelected(const QString& v)   { m_overrides["CustomTheme/tableSelected"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableSelected", v); }
void ConfigManager::setCustomTheme_tableBorder(const QString& v)     { m_overrides["CustomTheme/tableBorder"] = v; if (m_settings) m_settings->setValue("CustomTheme/tableBorder", v); }
void ConfigManager::setCustomTheme_navBg(const QString& v)           { m_overrides["CustomTheme/navBg"] = v; if (m_settings) m_settings->setValue("CustomTheme/navBg", v); }
void ConfigManager::setCustomTheme_navHover(const QString& v)        { m_overrides["CustomTheme/navHover"] = v; if (m_settings) m_settings->setValue("CustomTheme/navHover", v); }
void ConfigManager::setCustomTheme_navSelected(const QString& v)     { m_overrides["CustomTheme/navSelected"] = v; if (m_settings) m_settings->setValue("CustomTheme/navSelected", v); }
void ConfigManager::setCustomTheme_navSelectedText(const QString& v) { m_overrides["CustomTheme/navSelectedText"] = v; if (m_settings) m_settings->setValue("CustomTheme/navSelectedText", v); }
void ConfigManager::setCustomTheme_navText(const QString& v)         { m_overrides["CustomTheme/navText"] = v; if (m_settings) m_settings->setValue("CustomTheme/navText", v); }
void ConfigManager::setCustomTheme_scrollHandle(const QString& v)    { m_overrides["CustomTheme/scrollHandle"] = v; if (m_settings) m_settings->setValue("CustomTheme/scrollHandle", v); }
void ConfigManager::setCustomTheme_scrollTrack(const QString& v)     { m_overrides["CustomTheme/scrollTrack"] = v; if (m_settings) m_settings->setValue("CustomTheme/scrollTrack", v); }

// ══════════════════════════════════════════════════════════════════════════════
// SVG Icon Mode
// ══════════════════════════════════════════════════════════════════════════════
QString ConfigManager::svgIconMode() const {
    return value("CustomTheme/svgIconMode", "dark");  // default to dark (white icons)
}

void ConfigManager::setSvgIconMode(const QString& mode) {
    m_overrides["CustomTheme/svgIconMode"] = mode;
    if (m_settings) {
        m_settings->setValue("CustomTheme/svgIconMode", mode);
        m_settings->sync();
    }
}
