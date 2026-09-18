#ifndef MAGAZINE_DIALOG_H
#define MAGAZINE_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include "core/promotion.h"
#include "data/irepositories.h"

class MagazineDialog : public QDialog {
    Q_OBJECT

public:
    explicit MagazineDialog(IPromotionRepository* promotionRepo,
                           IProductRepository* productRepo,
                           QWidget* parent = nullptr);

    void setPromotion(const Promotion& promotion, const QList<int>& productIds);
    Promotion getPromotion() const { return m_promotion; }
    QList<int> getProductIds() const;

private slots:
    void onSave();
    void onCancel();
    void onSearchProduct();
    void onAddProduct();
    void onRemoveProduct();

private:
    void setupUi();
    void loadPromotionData();
    void retranslateUi();

    IPromotionRepository* m_promotionRepo;
    IProductRepository* m_productRepo;

    Promotion m_promotion;
    QList<Product> m_allProducts;

    // Basic Info
    QLineEdit* m_titleEdit;
    QTextEdit* m_descriptionEdit;
    QDateTimeEdit* m_startDateEdit;
    QDateTimeEdit* m_endDateEdit;
    QComboBox* m_statusCombo;

    // Product Search & Add
    QLineEdit* m_productSearchEdit;
    QPushButton* m_btnAddProduct;
    QTableWidget* m_productsTable;
    QPushButton* m_btnRemoveProduct;

    // Save/Cancel
    QPushButton* m_btnSave;
    QPushButton* m_btnCancel;
};

#endif // MAGAZINE_DIALOG_H
