#ifndef PRODUCTS_PAGE_H
#define PRODUCTS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QLabel>
#include <QTabWidget>
#include "ui/products/product_table_model.h"
#include "ui/translatable_page.h"
#include "data/irepositories.h"

class IProductSupplierRepository;

class ProductsPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit ProductsPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    // Products Tab
    void onSearchProducts();
    void onCategoryFilterChanged(int index);
    void onAddProduct();
    void onEditProduct();
    void onDeleteProduct();
    void onProductSelectionChanged();
    void onPrintBarcode();  // NEW: Print barcode for selected product
    void showProductContextMenu(const QPoint& pos);  // NEW: Context menu
    void onManageSuppliers();  // NEW: Manage suppliers for selected product

    // Category Management
    void onManageCategories();

    // Missing Products Tab
    void onAddMissingProduct();
    void onToggleMissingResolved();
    void onDeleteMissingProduct();
    void onMissingProductDoubleClicked(int row);
    void onImportMissingProducts();

    // Promotions Tab
    void onAddPromotion();
    void onEditPromotion();
    void onDeletePromotion();
    void onRefreshPromotions();
    void onPromotionSearchChanged();

    // Products Directory Import
    void onImportProductsDirectory();

private:
    void setupUi();
    void loadCategories();
    void loadProducts();
    void loadMissingProducts();
    void loadExpiredProducts();
    void loadPromotions();

    // Repositories
    IProductRepository*        m_productRepo   = nullptr;
    ICategoryRepository*       m_categoryRepo  = nullptr;
    IMissingProductRepository* m_missingRepo   = nullptr;
    ICustomerRepository*       m_customerRepo  = nullptr;
    IProductSubUnitRepository* m_subUnitRepo   = nullptr;
    IProductSupplierRepository* m_productSupplierRepo = nullptr;
    ISupplierRepository*       m_supplierRepo  = nullptr;
    IPromotionRepository*      m_promotionRepo = nullptr;

    // UI — Products Tab
    QLineEdit*           m_searchEdit       = nullptr;
    QComboBox*           m_catFilterCombo   = nullptr;
    QPushButton*         m_addProdBtn       = nullptr;
    QPushButton*         m_editProdBtn      = nullptr;
    QPushButton*         m_deleteProdBtn    = nullptr;
    QPushButton*         m_manageCatBtn     = nullptr;
    QPushButton*         m_importProdBtn    = nullptr;   // NEW: import products directory
    QTableView*          m_prodTableView    = nullptr;
    QLabel*              m_prodStatusLabel  = nullptr;
    ProductTableModel*   m_prodModel        = nullptr;

    // UI — Missing Products Tab
    QTableWidget*        m_missingTable     = nullptr;
    QLineEdit*           m_missingSearchEdit= nullptr;   // #10: search bar
    QPushButton*         m_addMissingBtn    = nullptr;
    QPushButton*         m_toggleResolveBtn = nullptr;
    QPushButton*         m_deleteMissingBtn = nullptr;   // Issue 5
    QPushButton*         m_importMissingBtn = nullptr;

    // UI — Expired Products Tab
    QTableWidget*        m_expiredTable        = nullptr;
    QPushButton*         m_restoreExpiredBtn   = nullptr;
    QPushButton*         m_deleteExpiredBtn    = nullptr;
    QPushButton*         m_refreshExpiredBtn   = nullptr;

    // UI — Promotions Tab
    QTableWidget*        m_promotionsTable     = nullptr;
    QPushButton*         m_addPromotionBtn     = nullptr;
    QPushButton*         m_editPromotionBtn    = nullptr;
    QPushButton*         m_deletePromotionBtn  = nullptr;
    QPushButton*         m_refreshPromotionsBtn= nullptr;
    QLineEdit*           m_promotionSearchEdit = nullptr;

    // Cached data
    QList<Category>      m_categories;
    QMap<int, QString>   m_categoryMap;
    QList<Customer>      m_customers;
    QList<MissingProduct> m_missingProducts;
    QList<Promotion>     m_promotions;
};

#endif // PRODUCTS_PAGE_H
