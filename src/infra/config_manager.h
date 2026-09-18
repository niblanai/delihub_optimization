#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QString>
#include <QMap>
#include <memory>

// Forward declaration — QSettings only needed in .cpp
class QSettings;

class ConfigManager {
public:
    static ConfigManager& instance();

    void init(const QString& filePath);

    // Getters
    QString databaseType() const;
    QString odbcConnectionString() const;
    QString sqlitePath() const;
    int     branchId() const;
    QString branchName() const;
    QString currencySymbol() const;
    QString theme() const;
    QString language() const;

    // Company / Invoice settings
    QString companyName() const;
    QString companyAddress() const;
    QString companyPhone() const;
    QString companyTaxNumber() const;
    QString invoiceLogoPath() const;
    QString receiptLogoPath() const;  // Separate logo for thermal receipts
    int     nextInvoiceNumber() const;
    void    incrementInvoiceNumber();
    
    // Tax settings
    double  taxRate() const;
    void    setTaxRate(double rate);

    void setCompanyName(const QString& v);
    void setCompanyAddress(const QString& v);
    void setCompanyPhone(const QString& v);
    void setCompanyTaxNumber(const QString& v);
    void setInvoiceLogoPath(const QString& v);
    void setReceiptLogoPath(const QString& v);  // Separate logo for thermal receipts

    // Auto-backup settings
    QString autoBackupTime()     const;
    QString autoBackupDir()      const;
    QString lastAutoBackupDate() const;
    void setLastAutoBackupDate(const QString& d);
    void setAutoBackupTime(const QString& t);
    void setAutoBackupDir(const QString& d);

    // Notes board background
    // bgMode: "solid" (default — follows theme) or "image"
    QString notesBgMode()       const;
    QString notesBgImagePath()  const;
    void setNotesBgMode(const QString& mode);
    void setNotesBgImagePath(const QString& path);

    // POS deletion protection
    bool requireAuthForPOSDeletion() const;
    void setRequireAuthForPOSDeletion(bool enabled);

    // Brevo email service credentials
    QString brevoApiKey() const;
    QString brevoSenderEmail() const;
    QString brevoSenderName() const;
    void setBrevoApiKey(const QString& key);
    void setBrevoSenderEmail(const QString& email);
    void setBrevoSenderName(const QString& name);

    // Receipt template designer
    // Templates are stored per paper width so switching 80mm <-> 58mm doesn't
    // clobber the other size's layout.
    QString receiptTemplateJson(int paperWidthMm) const;
    void    setReceiptTemplateJson(int paperWidthMm, const QString& json);
    int     receiptPaperWidthMm() const;          // last-selected paper size
    void    setReceiptPaperWidthMm(int mm);
    QString receiptPrinterName() const;           // empty = show print dialog
    void    setReceiptPrinterName(const QString& name);

    // Generic key-value read (for reminder state tracking)
    QString value(const QString& key, const QString& defaultVal) const;
    // Allow external code to set transient in-memory overrides (e.g. reminder tracking)
    void setOverride(const QString& key, const QString& val) {
        m_overrides[key] = val;
    }

    // Setters (work even without init(), using in-memory overrides)
    void setDatabaseType(const QString& type);
    void setOdbcConnectionString(const QString& connStr);
    void setSqlitePath(const QString& path);
    void setBranchId(int id);
    void setBranchName(const QString& name);
    void setCurrencySymbol(const QString& symbol);
    void setTheme(const QString& theme);
    void setLanguage(const QString& lang);

    // ── Custom Theme Settings ─────────────────────────────────────────────────
    // All custom theme colors (stored in config.ini when user creates custom theme)
    QString customTheme_windowBg() const;
    QString customTheme_sidebarBg() const;
    QString customTheme_cardBg() const;
    QString customTheme_cardHover() const;
    QString customTheme_inputBg() const;
    QString customTheme_dialogBg() const;
    QString customTheme_border() const;
    QString customTheme_borderFocus() const;
    QString customTheme_textPrimary() const;
    QString customTheme_textSecondary() const;
    QString customTheme_textDisabled() const;
    QString customTheme_textOnPrimary() const;
    QString customTheme_primary() const;
    QString customTheme_primaryHover() const;
    QString customTheme_primaryPressed() const;
    QString customTheme_success() const;
    QString customTheme_successBg() const;
    QString customTheme_warning() const;
    QString customTheme_warningBg() const;
    QString customTheme_danger() const;
    QString customTheme_dangerBg() const;
    QString customTheme_tableHeaderBg() const;
    QString customTheme_tableHeaderText() const;
    QString customTheme_tableRowAlt() const;
    QString customTheme_tableRowHover() const;
    QString customTheme_tableSelected() const;
    QString customTheme_tableBorder() const;
    QString customTheme_navBg() const;
    QString customTheme_navHover() const;
    QString customTheme_navSelected() const;
    QString customTheme_navSelectedText() const;
    QString customTheme_navText() const;
    QString customTheme_scrollHandle() const;
    QString customTheme_scrollTrack() const;
    
    // SVG Icon Mode: "light", "dark", or "custom" hex color
    QString svgIconMode() const;
    void setSvgIconMode(const QString& mode);

    void setCustomTheme_windowBg(const QString& v);
    void setCustomTheme_sidebarBg(const QString& v);
    void setCustomTheme_cardBg(const QString& v);
    void setCustomTheme_cardHover(const QString& v);
    void setCustomTheme_inputBg(const QString& v);
    void setCustomTheme_dialogBg(const QString& v);
    void setCustomTheme_border(const QString& v);
    void setCustomTheme_borderFocus(const QString& v);
    void setCustomTheme_textPrimary(const QString& v);
    void setCustomTheme_textSecondary(const QString& v);
    void setCustomTheme_textDisabled(const QString& v);
    void setCustomTheme_textOnPrimary(const QString& v);
    void setCustomTheme_primary(const QString& v);
    void setCustomTheme_primaryHover(const QString& v);
    void setCustomTheme_primaryPressed(const QString& v);
    void setCustomTheme_success(const QString& v);
    void setCustomTheme_successBg(const QString& v);
    void setCustomTheme_warning(const QString& v);
    void setCustomTheme_warningBg(const QString& v);
    void setCustomTheme_danger(const QString& v);
    void setCustomTheme_dangerBg(const QString& v);
    void setCustomTheme_tableHeaderBg(const QString& v);
    void setCustomTheme_tableHeaderText(const QString& v);
    void setCustomTheme_tableRowAlt(const QString& v);
    void setCustomTheme_tableRowHover(const QString& v);
    void setCustomTheme_tableSelected(const QString& v);
    void setCustomTheme_tableBorder(const QString& v);
    void setCustomTheme_navBg(const QString& v);
    void setCustomTheme_navHover(const QString& v);
    void setCustomTheme_navSelected(const QString& v);
    void setCustomTheme_navSelectedText(const QString& v);
    void setCustomTheme_navText(const QString& v);
    void setCustomTheme_scrollHandle(const QString& v);
    void setCustomTheme_scrollTrack(const QString& v);

    QString configFilePath() const { return m_filePath; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString m_filePath;
    std::unique_ptr<QSettings> m_settings;

    // In-memory overrides (used even without a settings file)
    QMap<QString, QString> m_overrides;
};

#endif // CONFIG_MANAGER_H
