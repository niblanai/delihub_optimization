#ifndef IREPOSITORIES_H
#define IREPOSITORIES_H

#include "core/customer.h"
#include "core/product.h"
#include "core/product_price_history.h"
#include "core/product_sub_unit.h"
#include "core/order.h"
#include "core/scheduled_order.h"
#include "core/missing_product.h"
#include "core/user.h"
#include "core/role.h"
#include "core/region.h"
#include "core/category.h"
#include "core/audit_log_entry.h"
#include "core/branch.h"
#include "core/delivery_driver.h"
#include "core/coupon.h"
#include "core/order_return.h"
#include "core/note.h"
// Phase 1: New models
#include "core/supplier.h"
#include "core/stock_movement.h"
#include "core/register_session.h"
#include "core/cash_movement.h"
#include "core/expense_category.h"
#include "core/expense.h"
#include "core/purchase_invoice.h"
#include "core/purchase_invoice_item.h"
#include "core/stock_count.h"
#include "core/stock_count_item.h"
#include "core/product_cost_history.h"
#include "core/attendance.h"
#include "core/promotion.h"

#include <QList>
#include <QString>
#include <QDateTime>

// IBranchRepository
class IBranchRepository {
public:
    virtual ~IBranchRepository() = default;
    virtual QList<Branch> getAll() = 0;
    virtual Branch getById(int id) = 0;
    virtual bool save(Branch& branch) = 0;
    virtual bool remove(int id) = 0;
};

// ICategoryRepository
class ICategoryRepository {
public:
    virtual ~ICategoryRepository() = default;
    virtual QList<Category> getAll() = 0;
    virtual Category getById(int id) = 0;
    virtual bool save(Category& category) = 0;
    virtual bool remove(int id) = 0;
};

// IRegionRepository
class IRegionRepository {
public:
    virtual ~IRegionRepository() = default;
    virtual QList<Region> getAll() = 0;
    virtual Region getById(int id) = 0;
    virtual bool save(Region& region) = 0;
    virtual bool remove(int id) = 0;
};

// IRoleRepository
class IRoleRepository {
public:
    virtual ~IRoleRepository() = default;
    virtual QList<Role> getAll() = 0;
    virtual Role getById(int id) = 0;
    virtual bool save(Role& role) = 0;
    virtual bool remove(int id) = 0;
};

// ICustomerRepository
class ICustomerRepository {
public:
    virtual ~ICustomerRepository() = default;
    virtual QList<Customer> getAll() = 0;
    virtual Customer getById(int id) = 0;
    virtual bool save(Customer& customer) = 0;
    virtual bool remove(int id) = 0;
    virtual QList<Customer> search(const QString& query) = 0;
    
    // Pagination support for lazy loading
    virtual QList<Customer> getPage(int offset, int limit) = 0;
    virtual int getTotalCount() = 0;
};

// IProductRepository
class IProductRepository {
public:
    virtual ~IProductRepository() = default;
    virtual QList<Product> getAll() = 0;
    virtual Product getById(int id) = 0;
    virtual bool save(Product& product) = 0;
    virtual bool remove(int id) = 0;
    virtual QList<Product> search(const QString& query) = 0;
    
    // Pagination support for lazy loading
    virtual QList<Product> getPage(int offset, int limit) = 0;
    virtual int getTotalCount() = 0;
    
    // Price history operations
    virtual QList<ProductPriceHistory> getPriceHistory(int productId) = 0;
    virtual bool addPriceHistoryEntry(int productId, double price, const QDateTime& effectiveDate) = 0;
};

// IOrderRepository
class IOrderRepository {
public:
    virtual ~IOrderRepository() = default;
    virtual QList<Order> getAll() = 0;
    virtual Order getById(int id) = 0;
    virtual bool save(Order& order) = 0;
    virtual bool remove(int id) = 0;
    virtual QList<Order> getOrdersByCustomerId(int customerId) = 0;
    virtual QList<Order> getOrdersDateRange(const QDateTime& start, const QDateTime& end) = 0;
    
    // Pagination support for lazy loading
    virtual QList<Order> getPage(int offset, int limit) = 0;
    virtual int getTotalCount() = 0;
};

// IScheduledOrderRepository
class IScheduledOrderRepository {
public:
    virtual ~IScheduledOrderRepository() = default;
    virtual QList<ScheduledOrder> getAll() = 0;
    virtual ScheduledOrder getById(int id) = 0;
    virtual bool save(ScheduledOrder& order) = 0;
    virtual bool remove(int id) = 0;
    virtual QList<ScheduledOrder> getByCustomerId(int customerId) = 0;
};

// IMissingProductRepository
class IMissingProductRepository {
public:
    virtual ~IMissingProductRepository() = default;
    virtual QList<MissingProduct> getAll() = 0;
    virtual MissingProduct getById(int id) = 0;
    virtual bool save(MissingProduct& entry) = 0;
    virtual bool remove(int id) = 0;
    // Returns the first active (not purchased) auto_low_stock entry for a product, or default (id==0)
    virtual MissingProduct getActiveAutoFlagByProductId(int productId) = 0;
};

// IUserRepository
class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual QList<User> getAll() = 0;
    virtual User getById(int id) = 0;
    virtual User getByUsername(const QString& username) = 0;
    virtual User getByEmail(const QString& email) = 0;  // NEW: for password recovery
    virtual bool save(User& user) = 0;
    virtual bool remove(int id) = 0;
};

// IAuditLogRepository
class IAuditLogRepository {
public:
    virtual ~IAuditLogRepository() = default;
    virtual QList<AuditLogEntry> getAll() = 0;
    virtual bool addEntry(const AuditLogEntry& entry) = 0;
    virtual QList<AuditLogEntry> getEntriesDateRange(const QDateTime& start, const QDateTime& end) = 0;
};

// IDeliveryDriverRepository
class IDeliveryDriverRepository {
public:
    virtual ~IDeliveryDriverRepository() = default;
    virtual QList<DeliveryDriver> getAll() = 0;
    virtual QList<DeliveryDriver> getActive() = 0;
    virtual DeliveryDriver getById(int id) = 0;
    virtual bool save(DeliveryDriver& driver) = 0;
    virtual bool remove(int id) = 0;
};

// ICouponRepository
class ICouponRepository {
public:
    virtual ~ICouponRepository() = default;
    virtual QList<Coupon> getAll() = 0;
    virtual Coupon getByCode(const QString& code) = 0;
    virtual Coupon getById(int id) = 0;
    virtual bool save(Coupon& coupon) = 0;
    virtual bool remove(int id) = 0;
    virtual bool incrementUsage(int id) = 0;
};

// IOrderReturnRepository
class IOrderReturnRepository {
public:
    virtual ~IOrderReturnRepository() = default;
    virtual QList<OrderReturn> getAll() = 0;
    virtual QList<OrderReturn> getByOrderId(int orderId) = 0;
    virtual OrderReturn getById(int id) = 0;
    virtual bool save(OrderReturn& ret) = 0;
    virtual bool remove(int id) = 0;
};

// INoteRepository
class INoteRepository {
public:
    virtual ~INoteRepository() = default;
    virtual QList<Note> getAll() = 0;
    virtual Note getById(int id) = 0;
    virtual bool save(Note& note) = 0;    // insert if id==0, update otherwise
    virtual bool remove(int id) = 0;
};

// ══════════════════════════════════════════════════════════════════════════════
// Phase 1: New Repository Interfaces
// ══════════════════════════════════════════════════════════════════════════════

// ISupplierRepository
class ISupplierRepository {
public:
    virtual ~ISupplierRepository() = default;
    virtual QList<Supplier> getAll() = 0;
    virtual QList<Supplier> getActive() = 0;
    virtual Supplier getById(int id) = 0;
    virtual bool save(Supplier& supplier) = 0;
    virtual bool remove(int id) = 0;
};

// IStockMovementRepository
class IStockMovementRepository {
public:
    virtual ~IStockMovementRepository() = default;
    virtual QList<StockMovement> getAll() = 0;
    virtual QList<StockMovement> getByProductId(int productId) = 0;
    virtual QList<StockMovement> getByDateRange(const QDateTime& start, const QDateTime& end) = 0;
    virtual StockMovement getById(int id) = 0;
    virtual bool save(StockMovement& movement) = 0;
    virtual bool remove(int id) = 0;
};

// IRegisterSessionRepository
class IRegisterSessionRepository {
public:
    virtual ~IRegisterSessionRepository() = default;
    virtual QList<RegisterSession> getAll() = 0;
    virtual QList<RegisterSession> getByUserId(int userId) = 0;
    virtual RegisterSession getOpenSessionByUserId(int userId) = 0;
    virtual RegisterSession getById(int id) = 0;
    virtual bool save(RegisterSession& session) = 0;
    virtual bool remove(int id) = 0;
};

// ICashMovementRepository
class ICashMovementRepository {
public:
    virtual ~ICashMovementRepository() = default;
    virtual QList<CashMovement> getAll() = 0;
    virtual QList<CashMovement> getByRegisterSessionId(int sessionId) = 0;
    virtual CashMovement getById(int id) = 0;
    virtual bool save(CashMovement& movement) = 0;
    virtual bool remove(int id) = 0;
};

// IExpenseCategoryRepository
class IExpenseCategoryRepository {
public:
    virtual ~IExpenseCategoryRepository() = default;
    virtual QList<ExpenseCategory> getAll() = 0;
    virtual ExpenseCategory getById(int id) = 0;
    virtual bool save(ExpenseCategory& category) = 0;
    virtual bool remove(int id) = 0;
};

// IExpenseRepository
class IExpenseRepository {
public:
    virtual ~IExpenseRepository() = default;
    virtual QList<Expense> getAll() = 0;
    virtual QList<Expense> getByDateRange(const QDate& start, const QDate& end) = 0;
    virtual QList<Expense> getByRegisterSessionId(int sessionId) = 0;
    virtual Expense getById(int id) = 0;
    virtual bool save(Expense& expense) = 0;
    virtual bool remove(int id) = 0;
};

// IPurchaseInvoiceRepository
class IPurchaseInvoiceRepository {
public:
    virtual ~IPurchaseInvoiceRepository() = default;
    virtual QList<PurchaseInvoice> getAll() = 0;
    virtual QList<PurchaseInvoice> getBySupplierId(int supplierId) = 0;
    virtual QList<PurchaseInvoice> getByDateRange(const QDate& start, const QDate& end) = 0;
    virtual PurchaseInvoice getById(int id) = 0;
    virtual bool save(PurchaseInvoice& invoice) = 0;
    virtual bool remove(int id) = 0;
    
    // Item management
    virtual QList<PurchaseInvoiceItem> getItems(int invoiceId) = 0;
    virtual bool saveItems(int invoiceId, const QList<PurchaseInvoiceItem>& items) = 0;
};

// IStockCountRepository
class IStockCountRepository {
public:
    virtual ~IStockCountRepository() = default;
    virtual QList<StockCount> getAll() = 0;
    virtual QList<StockCount> getByDateRange(const QDate& start, const QDate& end) = 0;
    virtual StockCount getById(int id) = 0;
    virtual bool save(StockCount& count) = 0;
    virtual bool remove(int id) = 0;
    
    // Item management
    virtual QList<StockCountItem> getItems(int countId) = 0;
    virtual bool saveItems(int countId, const QList<StockCountItem>& items) = 0;
};

// IProductCostHistoryRepository
class IProductCostHistoryRepository {
public:
    virtual ~IProductCostHistoryRepository() = default;
    virtual QList<ProductCostHistory> getByProductId(int productId) = 0;
    virtual ProductCostHistory getById(int id) = 0;
    virtual bool save(ProductCostHistory& history) = 0;
    virtual bool remove(int id) = 0;
};

// IProductSubUnitRepository
class IProductSubUnitRepository {
public:
    virtual ~IProductSubUnitRepository() = default;
    virtual QList<ProductSubUnit> getByProductId(int productId) = 0;
    virtual ProductSubUnit getById(int id) = 0;
    virtual bool save(ProductSubUnit& subUnit) = 0;
    virtual bool remove(int id) = 0;
};

// IAttendanceRepository
class IAttendanceRepository {
public:
    virtual ~IAttendanceRepository() = default;
    virtual QList<Attendance> getAll() = 0;
    virtual QList<Attendance> getByUserId(int userId) = 0;
    virtual QList<Attendance> getByDateRange(const QDateTime& start, const QDateTime& end) = 0;
    virtual QList<Attendance> getByUserAndDateRange(int userId, const QDateTime& start, const QDateTime& end) = 0;
    virtual Attendance getLastByUserId(int userId) = 0;
    virtual bool save(Attendance& attendance) = 0;
    virtual bool remove(int id) = 0;
};

// IPromotionRepository
class IPromotionRepository {
public:
    virtual ~IPromotionRepository() = default;
    virtual QList<Promotion> getAll() = 0;
    virtual QList<Promotion> getActive() = 0;
    virtual QList<Promotion> getByDateRange(const QDate& start, const QDate& end) = 0;
    virtual Promotion getById(int id) = 0;
    virtual bool save(Promotion& promo) = 0;
    virtual bool remove(int id) = 0;
    
    // Promotion-Product associations (PromotionProducts table)
    virtual bool addProductToPromotion(int promotionId, int productId) = 0;
    virtual bool removeProductFromPromotion(int promotionId, int productId) = 0;
    virtual bool removeAllProductsFromPromotion(int promotionId) = 0;
    virtual QList<int> getProductsForPromotion(int promotionId) = 0;
    virtual bool setProductMagazinePrice(int promotionId, int productId, double magazinePrice) = 0;
    virtual double getProductMagazinePrice(int promotionId, int productId) = 0;
};

#endif // IREPOSITORIES_H
