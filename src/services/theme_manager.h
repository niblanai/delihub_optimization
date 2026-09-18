#pragma once
#include <QString>
#include <QObject>
#include "services/design_tokens.h"

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    // Apply theme by enum or string ("dark"/"light"/"teal")
    void applyTheme(ThemeType type);
    void applyTheme(const QString& name);

    ThemeType      currentThemeType() const { return m_current; }
    DesignTokens   tokens()           const { return m_tokens; }
    QString        currentThemeName() const;

    // Modular stylesheet builders
    QString colorsStyle()  const;
    QString buttonsStyle() const;
    QString inputsStyle()  const;
    QString tableStyle()   const;
    QString sidebarStyle() const;
    QString cardsStyle()   const;
    QString dialogsStyle() const;
    QString scrollStyle()  const;
    QString notesStyle()   const;   // Notes page — board, chips, pagination
    QString fullStyle()    const;

    // Legacy compat — keeps old callers compiling
    QString theme() const { return currentThemeName(); }
    static QString darkStylesheet()  { return instance().fullStyle(); }
    static QString lightStylesheet() { return instance().fullStyle(); }
    static QString tealStylesheet()  { return instance().fullStyle(); }

signals:
    void themeChanged(ThemeType newTheme);

private:
    explicit ThemeManager(QObject* parent = nullptr) : QObject(parent) {}
    ThemeType    m_current = ThemeType::Dark;
    DesignTokens m_tokens  = DesignTokens::dark();
};
