#ifndef PROMOTIONS_PAGE_H
#define PROMOTIONS_PAGE_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include "data/irepositories.h"
#include "promotion_table_model.h"
#include "ui/translatable_page.h"

class PromotionsPage : public QWidget, public TranslatablePage {
    Q_OBJECT

public:
    explicit PromotionsPage(QWidget* parent = nullptr);
    void retranslateUi() override;

private slots:
    void onAddPromotion();
    void onEditPromotion();
    void onDeletePromotion();
    void onRefresh();
    void onFilterChanged();

private:
    void setupUi();
    void loadPromotions();

    IPromotionRepository* m_promotionRepo = nullptr;
    IProductRepository* m_productRepo = nullptr;
    ICategoryRepository* m_categoryRepo = nullptr;

    QTableView* m_tableView;
    PromotionTableModel* m_model;
    
    QPushButton* m_btnAdd;
    QPushButton* m_btnEdit;
    QPushButton* m_btnDelete;
    QPushButton* m_btnRefresh;
    
    QLineEdit* m_searchEdit;
    QComboBox* m_filterCombo;
};

#endif // PROMOTIONS_PAGE_H
