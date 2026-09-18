#ifndef TR_HELPER_H
#define TR_HELPER_H

// ── Helpers for live retranslation ────────────────────────────────────────────
// Usage:
//   TR_LABEL(myLabel, "Customers")        // stores key + translates now
//   TR_BTN(myBtn, "Add Customer")         // same for buttons
//   TR_TITLE(myLabel, "Customers")        // pageTitle variant
//
// MainWindow calls retranslateWidget(rootWidget) on languageChanged signal
// which walks all children looking for the "trKey" property.

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTabBar>
#include "services/lang_manager.h"

// Store translation key as a Qt property
#define TR_SET(widget, key) \
    (widget)->setText(LangManager::instance().t(key)); \
    (widget)->setProperty("trKey", QString(key))

#define TR_LABEL(lbl, key)  TR_SET(lbl,  key)
#define TR_BTN(btn,   key)  TR_SET(btn,  key)
#define TR_GROUP(grp, key)  \
    (grp)->setTitle(LangManager::instance().t(key)); \
    (grp)->setProperty("trKey", QString(key))

// Walk widget tree and retranslate anything with a "trKey" property
inline void retranslateWidget(QWidget* root) {
    if (!root) return;
    auto& L = LangManager::instance();

    // Check this widget
    QVariant key = root->property("trKey");
    if (key.isValid()) {
        QString text = L.t(key.toString());
        if (auto* lbl = qobject_cast<QLabel*>(root))       lbl->setText(text);
        else if (auto* btn = qobject_cast<QPushButton*>(root)) btn->setText(text);
        else if (auto* grp = qobject_cast<QGroupBox*>(root))   grp->setTitle(text);
    }

    // Recurse
    for (QObject* child : root->children()) {
        if (auto* w = qobject_cast<QWidget*>(child))
            retranslateWidget(w);
    }
}

#endif // TR_HELPER_H
