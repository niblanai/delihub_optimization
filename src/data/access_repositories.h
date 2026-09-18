#ifndef ACCESS_REPOSITORIES_H
#define ACCESS_REPOSITORIES_H

#include "irepositories.h"
#include "product_supplier_repository.h"
#include <QSqlDatabase>

class BaseAccessRepository {
protected:
    QSqlDatabase db() const;
    const QString m_connectionName = "default_delivery_connection";
};

class AccessBranchRepository : public IBranchRepository, private BaseAccessRepository {
public:
    QList<Branch> getAll() override;
    Branch getById(int id) override;
    bool save(Branch& branch) override;
    bool remove(int id) override;
};

class AccessCategoryRepository : public ICategoryRepository, private BaseAccessRepository {
public:
    QList<Category> getAll() override;
    Category getById(int id) override;
    bool save(Category& category) override;
    bool remove(int id) override;
};

class AccessRegionRepository : public IRegionRepository, private BaseAccessRepository {
public:
    QList<Region> getAll() override;
    Region getById(int id) override;
    bool save(Region& region) override;
    bool remove(int id) override;
};

class AccessRoleRepository : public IRoleRepository, private BaseAccessRepository {
public:
    QList<Role> getAll() override;
    Role getById(int id) override;
    bool save(Role& role) override;
    bool remove(int id) override;
};

class AccessCustomerRepository : public ICustomerRepository, private BaseAccessRepository {
public:
    QList<Customer> getAll() override;
    Customer getById(int id) override;
    bool save(Customer& customer) override;
    bool remove(int id) override;
    QList<Customer> search(const QString& query) override;
    
    // Pagination support
    QList<Customer> getPage(int offset, int limit) override;
    int getTotalCount() override;

private:
    void loadPhonesAndAddresses(Customer& customer);
    void loadFavorites(Customer& customer);
    bool savePhonesAndAddresses(const Customer& customer);
    bool saveFavorites(const Customer& customer);
};

class AccessProductRepository : public IProductRepository, private BaseAccessRepository {
public:
    QList<Product> getAll() override;
    Product getById(int id) override;
    bool save(Product& product) override;
    bool remove(int id) override;
    QList<Product> search(const QString& query) override;

    // Pagination support
    QList<Product> getPage(int offset, int limit) override;
    int getTotalCount() override;

    QList<ProductPriceHistory> getPriceHistory(int productId) override;
    bool addPriceHistoryEntry(int productId, double price, const QDateTime& effectiveDate) override;
};

class AccessOrderRepository : public IOrderRepository, private BaseAccessRepository {
public:
    QList<Order> getAll() override;
    Order getById(int id) override;
    bool save(Order& order) override;
    bool remove(int id) override;
    QList<Order> getOrdersByCustomerId(int customerId) override;
    QList<Order> getOrdersDateRange(const QDateTime& start, const QDateTime& end) override;
    
    // Pagination support
    QList<Order> getPage(int offset, int limit) override;
    int getTotalCount() override;

private:
    void loadItems(Order& order);
    bool saveItems(const Order& order);
};

class AccessScheduledOrderRepository : public IScheduledOrderRepository, private BaseAccessRepository {
public:
    QList<ScheduledOrder> getAll() override;
    ScheduledOrder getById(int id) override;
    bool save(ScheduledOrder& order) override;
    bool remove(int id) override;
    QList<ScheduledOrder> getByCustomerId(int customerId) override;

private:
    void loadItems(ScheduledOrder& order);
    bool saveItems(const ScheduledOrder& order);
};

class AccessMissingProductRepository : public IMissingProductRepository, private BaseAccessRepository {
public:
    QList<MissingProduct> getAll() override;
    MissingProduct getById(int id) override;
    bool save(MissingProduct& entry) override;
    bool remove(int id) override;
    MissingProduct getActiveAutoFlagByProductId(int /*productId*/) override { return {}; }
};

class AccessUserRepository : public IUserRepository, private BaseAccessRepository {
public:
    QList<User> getAll() override;
    User getById(int id) override;
    User getByUsername(const QString& username) override;
    User getByEmail(const QString& email) override;
    bool save(User& user) override;
    bool remove(int id) override;
};

class AccessAuditLogRepository : public IAuditLogRepository, private BaseAccessRepository {
public:
    QList<AuditLogEntry> getAll() override;
    bool addEntry(const AuditLogEntry& entry) override;
    QList<AuditLogEntry> getEntriesDateRange(const QDateTime& start, const QDateTime& end) override;
};

class AccessDeliveryDriverRepository : public IDeliveryDriverRepository, private BaseAccessRepository {
public:
    QList<DeliveryDriver> getAll() override;
    QList<DeliveryDriver> getActive() override;
    DeliveryDriver getById(int id) override;
    bool save(DeliveryDriver& driver) override;
    bool remove(int id) override;
};

// Coupon and Return repositories — delegate to SQLite impl
class AccessCouponRepository : public ICouponRepository, private BaseAccessRepository {
public:
    QList<Coupon> getAll() override;
    Coupon getByCode(const QString& code) override;
    Coupon getById(int id) override;
    bool save(Coupon& c) override;
    bool remove(int id) override;
    bool incrementUsage(int id) override;
};

class AccessOrderReturnRepository : public IOrderReturnRepository, private BaseAccessRepository {
public:
    QList<OrderReturn> getAll() override;
    QList<OrderReturn> getByOrderId(int orderId) override;
    OrderReturn getById(int id) override;
    bool save(OrderReturn& ret) override;
    bool remove(int id) override;
};

// AccessNoteRepository — uses the active connection (works with QPSQL + QSQLITE)
class AccessNoteRepository : public INoteRepository, private BaseAccessRepository {
public:
    QList<Note> getAll() override;
    Note        getById(int id) override;
    bool        save(Note& note) override;
    bool        remove(int id) override;
};

// ══════════════════════════════════════════════════════════════════════════════
// Phase 1: New Access Repository Implementations
// ══════════════════════════════════════════════════════════════════════════════

class AccessSupplierRepository : public ISupplierRepository, private BaseAccessRepository {
public:
    QList<Supplier> getAll() override;
    QList<Supplier> getActive() override;
    Supplier getById(int id) override;
    bool save(Supplier& supplier) override;
    bool remove(int id) override;
};

class AccessStockMovementRepository : public IStockMovementRepository, private BaseAccessRepository {
public:
    QList<StockMovement> getAll() override;
    QList<StockMovement> getByProductId(int productId) override;
    QList<StockMovement> getByDateRange(const QDateTime& start, const QDateTime& end) override;
    StockMovement getById(int id) override;
    bool save(StockMovement& movement) override;
    bool remove(int id) override;
};

class AccessRegisterSessionRepository : public IRegisterSessionRepository, private BaseAccessRepository {
public:
    QList<RegisterSession> getAll() override;
    QList<RegisterSession> getByUserId(int userId) override;
    RegisterSession getOpenSessionByUserId(int userId) override;
    RegisterSession getById(int id) override;
    bool save(RegisterSession& session) override;
    bool remove(int id) override;
};

class AccessCashMovementRepository : public ICashMovementRepository, private BaseAccessRepository {
public:
    QList<CashMovement> getAll() override;
    QList<CashMovement> getByRegisterSessionId(int sessionId) override;
    CashMovement getById(int id) override;
    bool save(CashMovement& movement) override;
    bool remove(int id) override;
};

class AccessExpenseCategoryRepository : public IExpenseCategoryRepository, private BaseAccessRepository {
public:
    QList<ExpenseCategory> getAll() override;
    ExpenseCategory getById(int id) override;
    bool save(ExpenseCategory& category) override;
    bool remove(int id) override;
};

class AccessExpenseRepository : public IExpenseRepository, private BaseAccessRepository {
public:
    QList<Expense> getAll() override;
    QList<Expense> getByDateRange(const QDate& start, const QDate& end) override;
    QList<Expense> getByRegisterSessionId(int sessionId) override;
    Expense getById(int id) override;
    bool save(Expense& expense) override;
    bool remove(int id) override;
};

class AccessPurchaseInvoiceRepository : public IPurchaseInvoiceRepository, private BaseAccessRepository {
public:
    QList<PurchaseInvoice> getAll() override;
    QList<PurchaseInvoice> getBySupplierId(int supplierId) override;
    QList<PurchaseInvoice> getByDateRange(const QDate& start, const QDate& end) override;
    PurchaseInvoice getById(int id) override;
    bool save(PurchaseInvoice& invoice) override;
    bool remove(int id) override;
    
    QList<PurchaseInvoiceItem> getItems(int invoiceId) override;
    bool saveItems(int invoiceId, const QList<PurchaseInvoiceItem>& items) override;
};

class AccessStockCountRepository : public IStockCountRepository, private BaseAccessRepository {
public:
    QList<StockCount> getAll() override;
    QList<StockCount> getByDateRange(const QDate& start, const QDate& end) override;
    StockCount getById(int id) override;
    bool save(StockCount& count) override;
    bool remove(int id) override;
    
    QList<StockCountItem> getItems(int countId) override;
    bool saveItems(int countId, const QList<StockCountItem>& items) override;
};

class AccessProductCostHistoryRepository : public IProductCostHistoryRepository, private BaseAccessRepository {
public:
    QList<ProductCostHistory> getByProductId(int productId) override;
    ProductCostHistory getById(int id) override;
    bool save(ProductCostHistory& history) override;
    bool remove(int id) override;
};

class AccessProductSubUnitRepository : public IProductSubUnitRepository, private BaseAccessRepository {
public:
    QList<ProductSubUnit> getByProductId(int productId) override;
    ProductSubUnit getById(int id) override;
    bool save(ProductSubUnit& subUnit) override;
    bool remove(int id) override;
};

class AccessProductSupplierRepository : public IProductSupplierRepository, private BaseAccessRepository {
public:
    bool save(const ProductSupplier& ps) override;
    bool remove(int productId, int supplierId) override;
    std::vector<ProductSupplier> getByProductId(int productId) override;
    std::vector<ProductSupplier> getBySupplierId(int supplierId) override;
    ProductSupplier getPreferredSupplier(int productId) override;
    bool setPreferred(int productId, int supplierId) override;
};

class AccessPromotionRepository : public IPromotionRepository, private BaseAccessRepository {
public:
    QList<Promotion> getAll() override;
    QList<Promotion> getActive() override;
    QList<Promotion> getByDateRange(const QDate& start, const QDate& end) override;
    Promotion getById(int id) override;
    bool save(Promotion& promotion) override;
    bool remove(int id) override;
    
    bool addProductToPromotion(int promotionId, int productId) override;
    bool removeProductFromPromotion(int promotionId, int productId) override;
    bool removeAllProductsFromPromotion(int promotionId) override;
    QList<int> getProductsForPromotion(int promotionId) override;
    bool setProductMagazinePrice(int promotionId, int productId, double magazinePrice) override;
    double getProductMagazinePrice(int promotionId, int productId) override;
    
    // Additional non-interface methods
    QList<Promotion> getByProductId(int productId);
    QList<Promotion> getByCategoryId(int categoryId);
};

#endif // ACCESS_REPOSITORIES_H
