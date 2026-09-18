#ifndef LANG_MANAGER_H
#define LANG_MANAGER_H

#include <QMap>
#include <QString>
#include <QObject>

enum class AppLang { English, Arabic };

// LangManager is a QObject so it can emit signals
class LangManager : public QObject {
    Q_OBJECT
public:
    static LangManager& instance();

    void setLanguage(AppLang lang);
    void setLanguage(const QString& code);  // "en" or "ar"
    AppLang  current() const;
    QString  currentCode() const;

    QString t(const QString& key) const;

signals:
    void languageChanged();

private:
    LangManager();
    void loadArabic();
    void loadArabicExtra();
    void loadArabicComprehensive();
    void loadArabicFinal();
    void loadArabicColumns();
    void loadArabicButtons();
    void loadArabicDialogs();

    AppLang              m_lang = AppLang::English;
    QMap<QString,QString> m_ar;
};

#endif // LANG_MANAGER_H
