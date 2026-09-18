#ifndef COUPONS_PAGE_H
#define COUPONS_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "data/irepositories.h"
#include "ui/translatable_page.h"

class CouponsPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit CouponsPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onSelectionChanged();

private:
    void setupUi();
    void loadCoupons();

    ICouponRepository* m_repo = nullptr;
    QList<Coupon> m_coupons;

    QTableWidget* m_table     = nullptr;
    QPushButton*  m_addBtn    = nullptr;
    QPushButton*  m_editBtn   = nullptr;
    QPushButton*  m_deleteBtn = nullptr;
    QLabel*       m_statusLbl = nullptr;
};

#endif // COUPONS_PAGE_H
