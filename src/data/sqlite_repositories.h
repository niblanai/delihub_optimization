#ifndef SQLITE_REPOSITORIES_H
#define SQLITE_REPOSITORIES_H

#include "irepositories.h"
#include "product_supplier_repository.h"
#include <QSqlDatabase>

class BaseSQLiteRepository {
protected:
    QSqlDatabase db() const;
    const QString m_connectionName = "default_delivery_connection";
};

class SQLiteBranchRepository : public IBranchRepository, private BaseSQLiteRepository {
public:
    QList<Branch> getAll() override;
    Branch getById(int id) override;
    bool save(Branch& branch) override;
    bool remove(int id) override;
};

class SQLiteCategoryRepository : public ICategoryRepository, private BaseSQLiteRepository {
public:
    QList<Category> getAll() override;
    Category getById(int id) override;
    bool save(Category& category) override;
    bool remove(int id) override;
};

class SQLiteRegionRepository : public IRegionRepository, private BaseSQLiteRepository {
public:
    QList<Region> getAll() override;
    Region getById(int id) override;
    bool save(Region& region) override;
    bool remove(int id) override;
};

class SQLiteRoleRepository : public IRoleRepository, private BaseSQLiteRepository {
public:
    QList<Role> getAll() override;
    Role getById(int id) override;
    bool save(Role& role) override;
    bool remove(int id) override;
};

class SQLiteCustomerRepository : public ICustomerRepository, private BaseSQLiteRepository {
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

class SQLiteProductRepository : public IProductRepository, private BaseSQLiteRepository {
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

    // Called after every save — creates/removes auto_low_stock flags as needed.
    // Requires a missingRepo to create/remove MissingProduct entries.
    void checkAndUpdateLowStockFlag(const Product& p, IMissingProductRepository* missingRepo);
};

class SQLiteOrderRepository : public IOrderRepository, private BaseSQLiteRepository {
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

class SQLiteScheduledOrderRepository : public IScheduledOrderRepository, private BaseSQLiteRepository {
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

class SQLiteMissingProductRepository : public IMissingProductRepository, private BaseSQLiteRepository {
public:
    QList<MissingProduct> getAll() override;
    MissingProduct getById(int id) override;
    bool save(MissingProduct& entry) override;
    bool remove(int id) override;
    MissingProduct getActiveAutoFlagByProductId(int productId) override;
};

class SQLiteUserRepository : public IUserRepository, private BaseSQLiteRepository {
public:
    QList<User> getAll() override;
    User getById(int id) override;
    User getByUsername(const QString& username) override;
    User getByEmail(const QString& email);
    bool save(User& user) override;
    bool remove(int id) override;
};

class SQLiteAuditLogRepository : public IAuditLogRepository, private BaseSQLiteRepository {
public:
    QList<AuditLogEntry> getAll() override;
    bool addEntry(const AuditLogEntry& entry) override;
    QList<AuditLogEntry> getEntriesDateRange(const QDateTime& start, const QDateTime& end) override;
};

class SQLiteDeliveryDriverRepository : public IDeliveryDriverRepository, private BaseSQLiteRepository {
public:
    QList<DeliveryDriver> getAll() override;
    QList<DeliveryDriver> getActive() override;
    DeliveryDriver getById(int id) override;
    bool save(DeliveryDriver& driver) override;
    bool remove(int id) override;
};

class SQLiteCouponRepository : public ICouponRepository, private BaseSQLiteRepository {
public:
    QList<Coupon> getAll() override;
    Coupon getByCode(const QString& code) override;
    Coupon getById(int id) override;
    bool save(Coupon& coupon) override;
    bool remove(int id) override;
    bool incrementUsage(int id) override;
};

class SQLiteOrderReturnRepository : public IOrderReturnRepository, private BaseSQLiteRepository {
public:
    QList<OrderReturn> getAll() override;
    QList<OrderReturn> getByOrderId(int orderId) override;
    OrderReturn getById(int id) override;
    bool save(OrderReturn& ret) override;
    bool remove(int id) override;

private:
    void loadItems(OrderReturn& ret);
    bool saveItems(const OrderReturn& ret);
};

class SQLiteNoteRepository : public INoteRepository, private BaseSQLiteRepository {
public:
    QList<Note> getAll() override;
    Note getById(int id) override;
    bool save(Note& note) override;
    bool remove(int id) override;
};

// ══════════════════════════════════════════════════════════════════════════════
// Phase 1: New SQLite Repository Implementations
// ══════════════════════════════════════════════════════════════════════════════

class SQLiteSupplierRepository : public ISupplierRepository, private BaseSQLiteRepository {
public:
    QList<Supplier> getAll() override;
    QList<Supplier> getActive() override;
    Supplier getById(int id) override;
    bool save(Supplier& supplier) override;
    bool remove(int id) override;
};

class SQLiteStockMovementRepository : public IStockMovementRepository, private BaseSQLiteRepository {
public:
    QList<StockMovement> getAll() override;
    QList<StockMovement> getByProductId(int productId) override;
    QList<StockMovement> getByDateRange(const QDateTime& start, const QDateTime& end) override;
    StockMovement getById(int id) override;
    bool save(StockMovement& movement) override;
    bool remove(int id) override;
};

class SQLiteRegisterSessionRepository : public IRegisterSessionRepository, private BaseSQLiteRepository {
public:
    QList<RegisterSession> getAll() override;
    QList<RegisterSession> getByUserId(int userId) override;
    RegisterSession getOpenSessionByUserId(int userId) override;
    RegisterSession getById(int id) override;
    bool save(RegisterSession& session) override;
    bool remove(int id) override;
};

class SQLiteCashMovementRepository : public ICashMovementRepository, private BaseSQLiteRepository {
public:
    QList<CashMovement> getAll() override;
    QList<CashMovement> getByRegisterSessionId(int sessionId) override;
    CashMovement getById(int id) override;
    bool save(CashMovement& movement) override;
    bool remove(int id) override;
};

class SQLiteExpenseCategoryRepository : public IExpenseCategoryRepository, private BaseSQLiteRepository {
public:
    QList<ExpenseCategory> getAll() override;
    ExpenseCategory getById(int id) override;
    bool save(ExpenseCategory& category) override;
    bool remove(int id) override;
};

class SQLiteExpenseRepository : public IExpenseRepository, private BaseSQLiteRepository {
public:
    QList<Expense> getAll() override;
    QList<Expense> getByDateRange(const QDate& start, const QDate& end) override;
    QList<Expense> getByRegisterSessionId(int sessionId) override;
    Expense getById(int id) override;
    bool save(Expense& expense) override;
    bool remove(int id) override;
};

class SQLitePurchaseInvoiceRepository : public IPurchaseInvoiceRepository, private BaseSQLiteRepository {
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

class SQLiteStockCountRepository : public IStockCountRepository, private BaseSQLiteRepository {
public:
    QList<StockCount> getAll() override;
    QList<StockCount> getByDateRange(const QDate& start, const QDate& end) override;
    StockCount getById(int id) override;
    bool save(StockCount& count) override;
    bool remove(int id) override;
    
    QList<StockCountItem> getItems(int countId) override;
    bool saveItems(int countId, const QList<StockCountItem>& items) override;
};

class SQLiteProductCostHistoryRepository : public IProductCostHistoryRepository, private BaseSQLiteRepository {
public:
    QList<ProductCostHistory> getByProductId(int productId) override;
    ProductCostHistory getById(int id) override;
    bool save(ProductCostHistory& history) override;
    bool remove(int id) override;
};

class SQLiteProductSubUnitRepository : public IProductSubUnitRepository, private BaseSQLiteRepository {
public:
    QList<ProductSubUnit> getByProductId(int productId) override;
    ProductSubUnit getById(int id) override;
    bool save(ProductSubUnit& subUnit) override;
    bool remove(int id) override;
};

// SQLiteAttendanceRepository
class SQLiteAttendanceRepository : public IAttendanceRepository, private BaseSQLiteRepository {
public:
    QList<Attendance> getAll() override;
    QList<Attendance> getByUserId(int userId) override;
    QList<Attendance> getByDateRange(const QDateTime& start, const QDateTime& end) override;
    QList<Attendance> getByUserAndDateRange(int userId, const QDateTime& start, const QDateTime& end) override;
    Attendance getLastByUserId(int userId) override;
    bool save(Attendance& attendance) override;
    bool remove(int id) override;
};

class SQLiteProductSupplierRepository : public IProductSupplierRepository, private BaseSQLiteRepository {
public:
    bool save(const ProductSupplier& ps) override;
    bool remove(int productId, int supplierId) override;
    std::vector<ProductSupplier> getByProductId(int productId) override;
    std::vector<ProductSupplier> getBySupplierId(int supplierId) override;
    ProductSupplier getPreferredSupplier(int productId) override;
    bool setPreferred(int productId, int supplierId) override;
};

class SQLitePromotionRepository : public IPromotionRepository, private BaseSQLiteRepository {
public:
    QList<Promotion> getAll() override;
    QList<Promotion> getActive() override;
    QList<Promotion> getByDateRange(const QDate& start, const QDate& end) override;
    Promotion getById(int id) override;
    bool save(Promotion& promo) override;
    bool remove(int id) override;
    
    bool addProductToPromotion(int promotionId, int productId) override;
    bool removeProductFromPromotion(int promotionId, int productId) override;
    bool removeAllProductsFromPromotion(int promotionId) override;
    QList<int> getProductsForPromotion(int promotionId) override;
    bool setProductMagazinePrice(int promotionId, int productId, double magazinePrice) override;
    double getProductMagazinePrice(int promotionId, int productId) override;
};

#endif // SQLITE_REPOSITORIES_H
