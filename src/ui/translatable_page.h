#ifndef TRANSLATABLE_PAGE_H
#define TRANSLATABLE_PAGE_H

// Mixin interface: any page that implements retranslateUi() can be
// auto-retranslated by MainWindow when the language changes.
class TranslatablePage {
public:
    virtual ~TranslatablePage() = default;
    virtual void retranslateUi() = 0;
};

#endif // TRANSLATABLE_PAGE_H
