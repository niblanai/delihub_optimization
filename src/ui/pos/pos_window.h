#ifndef POS_WINDOW_H
#define POS_WINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include "core/product.h"
#include "core/customer.h"
#include "core/register_session.h"
#include "core/order.h"
#include "data/irepositories.h"

class NumericKeypad;

struct CartItem {
    int productId = 0;
    QString name;
    double unitPrice = 0.0;  // Base price (before tax)
    int quantity = 0;
    bool hasTax = false;     // Whether this product has tax enabled
    double total() const { return unitPrice * quantity; }
};

// Single source of truth for what the customer actually owes: subtotal + tax on
// taxable items + delivery fee (if this is a delivery order). Both the cart screen
// and the payment/checkout screen must use this same struct, never recompute
// their own version - that divergence is exactly what caused checkout to charge
// only the subtotal while the cart correctly displayed subtotal + tax + delivery.
struct CartTotals {
    double subtotal = 0.0;
    double tax = 0.0;
    double deliveryFee = 0.0;
    double total() const { return subtotal + tax + deliveryFee; }
};

class PosWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit PosWindow(QWidget* parent = nullptr);
    ~PosWindow() override;
    
    void setRegisterSession(const RegisterSession& session);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onProductClicked(const Product& product);
    void onQuantityChanged();
    void onCustomerSelect();
    void onCheckout();
    void onCashPaymentClicked();
    void onCardPaymentClicked();
    void onValidateAndPrint();
    void onPrintReceipt();
    void onNewOrder();
    void onClearCart();
    void onLogout();
    void onSessionChanged(int index);
    void refreshProducts();
    void updatePaymentDisplay();

private:
    void setupUi();
    void loadProducts();
    void loadSessionsToCombo();
    void updateCartDisplay();
    void updateTotals();
    void showPaymentView();
    void showSuccessView();
    void showCartView();
    bool saveOrder(const QString& paymentMethod);
    bool verifyAdminPassword();
    bool requestDeleteAuthorization();  // Request barcode authorization for deletion
    // Returns the active magazine/promotion price for this product, or 0.0 if none applies.
    // Shared by refreshProducts() (for display) and onProductClicked() (for the actual sale price)
    // so the two can never disagree again.
    // Shared by updateTotals() (cart screen) and onCheckout() (payment screen) -
    // see CartTotals in pos_window.h for why these must never be computed separately.
    CartTotals calculateCartTotals() const;
    double getActivePromotionPrice(const Product& product, const QList<Promotion>& activePromotions) const;
    
    // Repositories
    IProductRepository* m_productRepo = nullptr;
    ICustomerRepository* m_customerRepo = nullptr;
    IOrderRepository* m_orderRepo = nullptr;
    IRegionRepository* m_regionRepo = nullptr;
    IRegisterSessionRepository* m_sessionRepo = nullptr;
    IStockMovementRepository* m_stockMovementRepo = nullptr;
    IPromotionRepository* m_promotionRepo = nullptr;
    
    // Data
    QList<Product> m_products;
    QList<CartItem> m_cart;
    Customer m_selectedCustomer;
    RegisterSession m_session;
    Order m_lastOrder;
    
    // Quantity input buffer
    QString m_quantityBuffer;
    int m_selectedCartIndex = -1;
    
    // UI Components
    QWidget* m_centralWidget = nullptr;
    QGridLayout* m_productGrid = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    
    // Right panel with stacked views
    QStackedWidget* m_rightStack = nullptr;
    
    // View 0: Cart
    QWidget* m_cartView = nullptr;
    QListWidget* m_cartList = nullptr;
    QLabel* m_customerLbl = nullptr;
    QLabel* m_cartSubtotalLbl = nullptr;
    QLabel* m_cartTaxLbl = nullptr;
    QLabel* m_cartTotalLbl = nullptr;
    QPushButton* m_selectCustomerBtn = nullptr;
    QPushButton* m_checkoutBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_deliveryToggleBtn = nullptr;
    QLabel* m_deliveryFeeLabel = nullptr;
    QDoubleSpinBox* m_deliveryFeeSpin = nullptr;
    
    // View 1: Payment
    QWidget* m_paymentView = nullptr;
    QLabel* m_paymentTotalLbl = nullptr;
    NumericKeypad* m_numericKeypad = nullptr;
    QLabel* m_cashAmountLbl = nullptr;
    QLabel* m_cardAmountLbl = nullptr;
    QLabel* m_remainingLbl = nullptr;
    QPushButton* m_cashBtn = nullptr;
    QPushButton* m_cardBtn = nullptr;
    QPushButton* m_validateBtn = nullptr;
    QPushButton* m_backToCartBtn = nullptr;
    
    // Payment state
    double m_totalAmount = 0.0;
    double m_cashAmount = 0.0;
    double m_cardAmount = 0.0;
    
    // View 2: Success
    QWidget* m_successView = nullptr;
    QLabel* m_successMessageLbl = nullptr;
    QPushButton* m_printReceiptBtn = nullptr;
    QPushButton* m_newOrderBtn = nullptr;
    
    QLabel* m_sessionLbl = nullptr;
    QPushButton* m_logoutBtn = nullptr;
    QComboBox* m_sessionCombo = nullptr;
};

#endif // POS_WINDOW_H
