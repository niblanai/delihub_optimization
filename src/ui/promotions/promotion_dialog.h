#ifndef PROMOTION_DIALOG_H
#define PROMOTION_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QLabel>
#include "core/promotion.h"
#include "data/irepositories.h"

class PromotionDialog : public QDialog {
    Q_OBJECT

public:
    explicit PromotionDialog(IPromotionRepository* promotionRepo,
                            IProductRepository* productRepo,
                            ICategoryRepository* categoryRepo,
                            QWidget* parent = nullptr);

    void setPromotion(const Promotion& promotion);
    Promotion getPromotion() const;

private slots:
    void onSave();
    void onCancel();
    void onTypeChanged(int index);
    void onBrowseImage();

private:
    void setupUi();
    void retranslateUi();

    IPromotionRepository* m_promotionRepo;
    IProductRepository* m_productRepo;
    ICategoryRepository* m_categoryRepo;

    Promotion m_promotion;

    QLineEdit* m_titleEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_typeCombo;
    QDoubleSpinBox* m_discountSpin;
    QDateTimeEdit* m_startDateEdit;
    QDateTimeEdit* m_endDateEdit;
    QComboBox* m_statusCombo;
    QLineEdit* m_imagePathEdit;
    QPushButton* m_btnBrowseImage;
    QComboBox* m_productCombo;
    QComboBox* m_categoryCombo;
    QDoubleSpinBox* m_minPurchaseSpin;

    QPushButton* m_btnSave;
    QPushButton* m_btnCancel;
};

#endif // PROMOTION_DIALOG_H
