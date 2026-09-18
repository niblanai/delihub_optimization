#include "access_repositories.h"
#include "sqlite_repositories.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QHash>
#include "infra/logger.h"
#include "infra/database_connection_manager.h"
#include "core/promotion.h"

QSqlDatabase BaseAccessRepository::db() const {
    return QSqlDatabase::database(m_connectionName);
}

// Helper: Quote table/column names for PostgreSQL (case-sensitive), leave unquoted for others
static QString maybeQuote(const QString& identifier, const QSqlDatabase& db) {
    if (db.driverName() == "QPSQL") {
        // PostgreSQL: quote to preserve PascalCase
        return QString("\"%1\"").arg(identifier);
    }
    // SQLite, MS Access: unquoted (case-insensitive)
    return identifier;
}

// Helper function to get auto-increment ID in MS Access
static int getAccessInsertId(QSqlQuery& query, QSqlDatabase db) {
    QVariant lastId = query.lastInsertId();
    if (lastId.isValid() && lastId.toInt() > 0) {
        return lastId.toInt();
    }
    // Fallback for MS Access ODBC
    QSqlQuery idQuery("SELECT @@IDENTITY", db);
    if (idQuery.next()) {
        return idQuery.value(0).toInt();
    }
    return 0;
}

// ==========================================
// AccessBranchRepository
// ==========================================

QList<Branch> AccessBranchRepository::getAll() {
    QList<Branch> list;
    QSqlQuery query("SELECT Id, Name, Address FROM Branches", db());
    while (query.next()) {
        Branch b;
        b.id = query.value(0).toInt();
        b.name = query.value(1).toString();
        b.address = query.value(2).toString();
        list.append(b);
    }
    return list;
}

Branch AccessBranchRepository::getById(int id) {
    Branch b;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name, Address FROM Branches WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        b.id = query.value(0).toInt();
        b.name = query.value(1).toString();
        b.address = query.value(2).toString();
    }
    return b;
}

bool AccessBranchRepository::save(Branch& branch) {
    QSqlQuery query(db());
    if (branch.id > 0) {
        query.prepare("UPDATE Branches SET Name = ?, Address = ? WHERE Id = ?");
        query.addBindValue(branch.name);
        query.addBindValue(branch.address);
        query.addBindValue(branch.id);
        return query.exec();
    } else {
        query.prepare("INSERT INTO Branches (Name, Address) VALUES (?, ?)");
        query.addBindValue(branch.name);
        query.addBindValue(branch.address);
        if (query.exec()) {
            branch.id = getAccessInsertId(query, db());
            return true;
        }
    }
    return false;
}

bool AccessBranchRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Branches WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessCategoryRepository
// ==========================================

QList<Category> AccessCategoryRepository::getAll() {
    QList<Category> list;
    QSqlQuery query("SELECT Id, Name FROM Categories", db());
    while (query.next()) {
        Category c;
        c.id = query.value(0).toInt();
        c.name = query.value(1).toString();
        list.append(c);
    }
    return list;
}

Category AccessCategoryRepository::getById(int id) {
    Category c;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name FROM Categories WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        c.id = query.value(0).toInt();
        c.name = query.value(1).toString();
    }
    return c;
}

bool AccessCategoryRepository::save(Category& category) {
    QSqlQuery query(db());
    if (category.id > 0) {
        query.prepare("UPDATE Categories SET Name = ? WHERE Id = ?");
        query.addBindValue(category.name);
        query.addBindValue(category.id);
        return query.exec();
    } else {
        query.prepare("INSERT INTO Categories (Name) VALUES (?)");
        query.addBindValue(category.name);
        if (query.exec()) {
            category.id = getAccessInsertId(query, db());
            return true;
        }
    }
    return false;
}

bool AccessCategoryRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Categories WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessRegionRepository
// ==========================================

QList<Region> AccessRegionRepository::getAll() {
    QList<Region> list;
    QSqlQuery query("SELECT Id, Name FROM Regions", db());
    while (query.next()) {
        Region r;
        r.id = query.value(0).toInt();
        r.name = query.value(1).toString();
        list.append(r);
    }
    return list;
}

Region AccessRegionRepository::getById(int id) {
    Region r;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name FROM Regions WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        r.id = query.value(0).toInt();
        r.name = query.value(1).toString();
    }
    return r;
}

bool AccessRegionRepository::save(Region& region) {
    QSqlQuery query(db());
    if (region.id > 0) {
        query.prepare("UPDATE Regions SET Name = ? WHERE Id = ?");
        query.addBindValue(region.name);
        query.addBindValue(region.id);
        return query.exec();
    } else {
        query.prepare("INSERT INTO Regions (Name) VALUES (?)");
        query.addBindValue(region.name);
        if (query.exec()) {
            region.id = getAccessInsertId(query, db());
            return true;
        }
    }
    return false;
}

bool AccessRegionRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Regions WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessRoleRepository
// ==========================================

QList<Role> AccessRoleRepository::getAll() {
    QList<Role> list;
    // Try full query first (with all new columns)
    QSqlQuery query("SELECT Id, Name, CanManageUsers, CanViewReports, "
                    "CanAddCustomer, CanEditCustomer, CanDeleteCustomer, "
                    "CanAddProduct, CanEditProduct, CanDeleteProduct, "
                    "CanAddOrder, CanEditOrder, CanCancelOrder, CanDeleteOrders, "
                    "CanManageRegions, CanAccessSettings, CanEditOrderDate, CanAccessDashboard, "
                    "CanManagePurchases, CanAccessPOS, CanManageRegister, CanManageExpenses, CanManageInventory, canDeleteFromPOS FROM Roles", db());
    
    // If query fails (missing columns), try legacy query
    bool useLegacy = !query.isActive() || query.lastError().isValid();
    if (useLegacy) {
        Logger::instance().warn("AccessRoleRepository: Some columns missing, using legacy query");
        query = QSqlQuery("SELECT Id, Name, CanManageUsers, CanViewReports, "
                         "CanAddCustomer, CanEditCustomer, CanDeleteCustomer, "
                         "CanAddProduct, CanEditProduct, CanDeleteProduct, "
                         "CanAddOrder, CanEditOrder, CanCancelOrder, CanDeleteOrders, "
                         "CanManageRegions, CanAccessSettings FROM Roles", db());
    }
    
    while (query.next()) {
        Role r;
        r.id               = query.value(0).toInt();
        r.name             = query.value(1).toString();
        r.canManageUsers   = query.value(2).toBool();
        r.canViewReports   = query.value(3).toBool();
        r.canAddCustomer   = query.value(4).toBool();
        r.canEditCustomer  = query.value(5).toBool();
        r.canDeleteCustomer= query.value(6).toBool();
        r.canAddProduct    = query.value(7).toBool();
        r.canEditProduct   = query.value(8).toBool();
        r.canDeleteProduct = query.value(9).toBool();
        r.canAddOrder      = query.value(10).toBool();
        r.canEditOrder     = query.value(11).toBool();
        r.canCancelOrder   = query.value(12).toBool();
        r.canDeleteOrders  = query.value(13).toBool();
        r.canManageRegions = query.value(14).toBool();
        r.canAccessSettings= query.value(15).toBool();
        
        if (!useLegacy) {
            r.canEditOrderDate = query.value(16).toBool();
            r.canAccessDashboard= query.value(17).isNull() ? true : query.value(17).toBool();
            // Phase 1 permissions
            r.canManagePurchases= query.value(18).toBool();
            r.canAccessPOS      = query.value(19).toBool();
            r.canManageRegister = query.value(20).toBool();
            r.canManageExpenses = query.value(21).toBool();
            r.canManageInventory= query.value(22).toBool();
            r.canDeleteFromPOS  = query.value(23).toBool();
        } else {
            // Legacy database - use defaults
            r.canAccessDashboard = true;
        }
        list.append(r);
    }
    return list;
}

Role AccessRoleRepository::getById(int id) {
    Role r;
    QSqlQuery query(db());
    // Try full query first
    query.prepare("SELECT Id, Name, CanManageUsers, CanViewReports, "
                  "CanAddCustomer, CanEditCustomer, CanDeleteCustomer, "
                  "CanAddProduct, CanEditProduct, CanDeleteProduct, "
                  "CanAddOrder, CanEditOrder, CanCancelOrder, CanDeleteOrders, "
                  "CanManageRegions, CanAccessSettings, CanEditOrderDate, CanAccessDashboard, "
                  "CanManagePurchases, CanAccessPOS, CanManageRegister, CanManageExpenses, CanManageInventory, canDeleteFromPOS FROM Roles WHERE Id=?");
    query.addBindValue(id);
    bool success = query.exec() && query.next();
    
    // If failed, try legacy query
    bool useLegacy = false;
    if (!success || query.lastError().isValid()) {
        Logger::instance().warn("AccessRoleRepository::getById: Some columns missing, using legacy query");
        query = QSqlQuery(db());
        query.prepare("SELECT Id, Name, CanManageUsers, CanViewReports, "
                     "CanAddCustomer, CanEditCustomer, CanDeleteCustomer, "
                     "CanAddProduct, CanEditProduct, CanDeleteProduct, "
                     "CanAddOrder, CanEditOrder, CanCancelOrder, CanDeleteOrders, "
                     "CanManageRegions, CanAccessSettings FROM Roles WHERE Id=?");
        query.addBindValue(id);
        useLegacy = true;
        success = query.exec() && query.next();
    }
    
    if (success) {
        r.id               = query.value(0).toInt();
        r.name             = query.value(1).toString();
        r.canManageUsers   = query.value(2).toBool();
        r.canViewReports   = query.value(3).toBool();
        r.canAddCustomer   = query.value(4).toBool();
        r.canEditCustomer  = query.value(5).toBool();
        r.canDeleteCustomer= query.value(6).toBool();
        r.canAddProduct    = query.value(7).toBool();
        r.canEditProduct   = query.value(8).toBool();
        r.canDeleteProduct = query.value(9).toBool();
        r.canAddOrder      = query.value(10).toBool();
        r.canEditOrder     = query.value(11).toBool();
        r.canCancelOrder   = query.value(12).toBool();
        r.canDeleteOrders  = query.value(13).toBool();
        r.canManageRegions = query.value(14).toBool();
        r.canAccessSettings= query.value(15).toBool();
        
        if (!useLegacy) {
            r.canEditOrderDate = query.value(16).toBool();
            r.canAccessDashboard= query.value(17).isNull() ? true : query.value(17).toBool();
            // Phase 1 permissions
            r.canManagePurchases= query.value(18).toBool();
            r.canAccessPOS      = query.value(19).toBool();
            r.canManageRegister = query.value(20).toBool();
            r.canManageExpenses = query.value(21).toBool();
            r.canManageInventory= query.value(22).toBool();
            r.canDeleteFromPOS  = query.value(23).toBool();
        } else {
            // Legacy database - use defaults
            r.canAccessDashboard = true;
        }
    }
    return r;
}

bool AccessRoleRepository::save(Role& role) {
    QSqlQuery query(db());
    auto b = [](bool v){ return v ? 1 : 0; };
    if (role.id > 0) {
        query.prepare("UPDATE Roles SET Name=?, CanManageUsers=?, CanViewReports=?, "
                      "CanAddCustomer=?, CanEditCustomer=?, CanDeleteCustomer=?, "
                      "CanAddProduct=?, CanEditProduct=?, CanDeleteProduct=?, "
                      "CanAddOrder=?, CanEditOrder=?, CanCancelOrder=?, CanDeleteOrders=?, "
                      "CanManageRegions=?, CanAccessSettings=?, "
                      "CanManagePurchases=?, CanAccessPOS=?, CanManageRegister=?, CanManageExpenses=?, CanManageInventory=? WHERE Id=?");
        query.addBindValue(role.name);
        query.addBindValue(b(role.canManageUsers));   query.addBindValue(b(role.canViewReports));
        query.addBindValue(b(role.canAddCustomer));   query.addBindValue(b(role.canEditCustomer));
        query.addBindValue(b(role.canDeleteCustomer));
        query.addBindValue(b(role.canAddProduct));    query.addBindValue(b(role.canEditProduct));
        query.addBindValue(b(role.canDeleteProduct));
        query.addBindValue(b(role.canAddOrder));      query.addBindValue(b(role.canEditOrder));
        query.addBindValue(b(role.canCancelOrder));   query.addBindValue(b(role.canDeleteOrders));
        query.addBindValue(b(role.canManageRegions)); query.addBindValue(b(role.canAccessSettings));
        // Phase 1 permissions
        query.addBindValue(b(role.canManagePurchases)); query.addBindValue(b(role.canAccessPOS));
        query.addBindValue(b(role.canManageRegister));  query.addBindValue(b(role.canManageExpenses));
        query.addBindValue(b(role.canManageInventory));
        query.addBindValue(role.id);
        return query.exec();
    } else {
        query.prepare("INSERT INTO Roles (Name, CanManageUsers, CanViewReports, "
                      "CanAddCustomer, CanEditCustomer, CanDeleteCustomer, "
                      "CanAddProduct, CanEditProduct, CanDeleteProduct, "
                      "CanAddOrder, CanEditOrder, CanCancelOrder, CanDeleteOrders, "
                      "CanManageRegions, CanAccessSettings, "
                      "CanManagePurchases, CanAccessPOS, CanManageRegister, CanManageExpenses, CanManageInventory) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        query.addBindValue(role.name);
        query.addBindValue(b(role.canManageUsers));   query.addBindValue(b(role.canViewReports));
        query.addBindValue(b(role.canAddCustomer));   query.addBindValue(b(role.canEditCustomer));
        query.addBindValue(b(role.canDeleteCustomer));
        query.addBindValue(b(role.canAddProduct));    query.addBindValue(b(role.canEditProduct));
        query.addBindValue(b(role.canDeleteProduct));
        query.addBindValue(b(role.canAddOrder));      query.addBindValue(b(role.canEditOrder));
        query.addBindValue(b(role.canCancelOrder));   query.addBindValue(b(role.canDeleteOrders));
        query.addBindValue(b(role.canManageRegions)); query.addBindValue(b(role.canAccessSettings));
        // Phase 1 permissions
        query.addBindValue(b(role.canManagePurchases)); query.addBindValue(b(role.canAccessPOS));
        query.addBindValue(b(role.canManageRegister));  query.addBindValue(b(role.canManageExpenses));
        query.addBindValue(b(role.canManageInventory));
        if (query.exec()) {
            role.id = getAccessInsertId(query, db());
            return true;
        }
    }
    return false;
}

bool AccessRoleRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Roles WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessCustomerRepository
// ==========================================

void AccessCustomerRepository::loadPhonesAndAddresses(Customer& customer) {
    QSqlQuery query(db());
    query.prepare("SELECT PhoneNumber FROM CustomerPhones WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (query.exec()) {
        QList<Phone> phones;
        while (query.next()) {
            phones.append(Phone{query.value(0).toString()});
        }
        customer.setPhones(phones);
    }

    query.prepare("SELECT AddressText FROM CustomerAddresses WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (query.exec()) {
        QList<Address> addresses;
        while (query.next()) {
            addresses.append(Address{query.value(0).toString()});
        }
        customer.setAddresses(addresses);
    }
}

void AccessCustomerRepository::loadFavorites(Customer& customer) {
    QSqlQuery query(db());
    query.prepare("SELECT ProductId FROM CustomerFavorites WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (query.exec()) {
        QList<int> favorites;
        while (query.next()) {
            favorites.append(query.value(0).toInt());
        }
        customer.setFavoriteProductIds(favorites);
    }
}

bool AccessCustomerRepository::savePhonesAndAddresses(const Customer& customer) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM CustomerPhones WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (!query.exec()) return false;

    query.prepare("DELETE FROM CustomerAddresses WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (!query.exec()) return false;

    for (const auto& phone : customer.phones()) {
        query.prepare("INSERT INTO CustomerPhones (CustomerId, PhoneNumber) VALUES (?, ?)");
        query.addBindValue(customer.id());
        query.addBindValue(phone.number);
        if (!query.exec()) return false;
    }

    for (const auto& address : customer.addresses()) {
        query.prepare("INSERT INTO CustomerAddresses (CustomerId, AddressText) VALUES (?, ?)");
        query.addBindValue(customer.id());
        query.addBindValue(address.text);
        if (!query.exec()) return false;
    }
    return true;
}

bool AccessCustomerRepository::saveFavorites(const Customer& customer) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM CustomerFavorites WHERE CustomerId = ?");
    query.addBindValue(customer.id());
    if (!query.exec()) return false;

    for (int prodId : customer.favoriteProductIds()) {
        query.prepare("INSERT INTO CustomerFavorites (CustomerId, ProductId) VALUES (?, ?)");
        query.addBindValue(customer.id());
        query.addBindValue(prodId);
        if (!query.exec()) return false;
    }
    return true;
}

QList<Customer> AccessCustomerRepository::getAll() {
    QList<Customer> list;

    // Batch-load every customer's phones/addresses/favorites in 3 queries
    // TOTAL instead of 3 queries PER CUSTOMER. On local SQLite the old
    // per-row loadPhonesAndAddresses()/loadFavorites() calls cost almost
    // nothing (no network hop). Over Supabase, each one is a real network
    // round trip — with thousands of customers that's thousands of extra
    // round trips, which is what turns "open the app" into a 30-minute wait.
    QHash<int, QList<Phone>>   phonesByCustomer;
    QHash<int, QList<Address>> addressesByCustomer;
    QHash<int, QList<int>>     favoritesByCustomer;
    {
        QSqlQuery q("SELECT CustomerId, PhoneNumber FROM CustomerPhones", db());
        while (q.next())
            phonesByCustomer[q.value(0).toInt()].append(Phone{q.value(1).toString()});
    }
    {
        QSqlQuery q("SELECT CustomerId, AddressText FROM CustomerAddresses", db());
        while (q.next())
            addressesByCustomer[q.value(0).toInt()].append(Address{q.value(1).toString()});
    }
    {
        QSqlQuery q("SELECT CustomerId, ProductId FROM CustomerFavorites", db());
        while (q.next())
            favoritesByCustomer[q.value(0).toInt()].append(q.value(1).toInt());
    }

    QSqlQuery query("SELECT Id, Name, DistanceKm, RegionId, Notes, FirstContactDate, LastOrderDate, Status, PreferredPaymentMethod, PreferredPaymentOther, Debt FROM Customers", db());
    while (query.next()) {
        Customer c(query.value(0).toInt(), query.value(1).toString());
        c.setDistanceKm(query.value(2).toDouble());
        c.setRegionId(query.value(3).toInt());
        c.setNotes(query.value(4).toString());
        c.setFirstContactDate(QDateTime::fromString(query.value(5).toString(), Qt::ISODate));
        c.setLastOrderDate(QDateTime::fromString(query.value(6).toString(), Qt::ISODate));
        c.setStatus(Customer::stringToStatus(query.value(7).toString()));
        c.setPreferredPaymentMethod(query.value(8).toString());
        c.setPreferredPaymentOther(query.value(9).toString());
        c.setDebt(query.value(10).toDouble());
        c.setPhones(phonesByCustomer.value(c.id()));
        c.setAddresses(addressesByCustomer.value(c.id()));
        c.setFavoriteProductIds(favoritesByCustomer.value(c.id()));
        list.append(c);
    }
    return list;
}

Customer AccessCustomerRepository::getById(int id) {
    Customer c;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name, DistanceKm, RegionId, Notes, FirstContactDate, LastOrderDate, Status, PreferredPaymentMethod, PreferredPaymentOther, Debt FROM Customers WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        c.setId(query.value(0).toInt());
        c.setName(query.value(1).toString());
        c.setDistanceKm(query.value(2).toDouble());
        c.setRegionId(query.value(3).toInt());
        c.setNotes(query.value(4).toString());
        c.setFirstContactDate(QDateTime::fromString(query.value(5).toString(), Qt::ISODate));
        c.setLastOrderDate(QDateTime::fromString(query.value(6).toString(), Qt::ISODate));
        c.setStatus(Customer::stringToStatus(query.value(7).toString()));
        c.setPreferredPaymentMethod(query.value(8).toString());
        c.setPreferredPaymentOther(query.value(9).toString());
        c.setDebt(query.value(10).toDouble());
        loadPhonesAndAddresses(c);
        loadFavorites(c);
    }
    return c;
}

QList<Customer> AccessCustomerRepository::getPage(int offset, int limit) {
    QList<Customer> list;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name, DistanceKm, RegionId, Notes, FirstContactDate, LastOrderDate, Status, PreferredPaymentMethod, PreferredPaymentOther, Debt FROM Customers LIMIT ? OFFSET ?");
    query.addBindValue(limit);
    query.addBindValue(offset);
    if (!query.exec()) return list;

    while (query.next()) {
        Customer c(query.value(0).toInt(), query.value(1).toString());
        c.setDistanceKm(query.value(2).toDouble());
        c.setRegionId(query.value(3).toInt());
        c.setNotes(query.value(4).toString());
        c.setFirstContactDate(QDateTime::fromString(query.value(5).toString(), Qt::ISODate));
        c.setLastOrderDate(QDateTime::fromString(query.value(6).toString(), Qt::ISODate));
        c.setStatus(Customer::stringToStatus(query.value(7).toString()));
        c.setPreferredPaymentMethod(query.value(8).toString());
        c.setPreferredPaymentOther(query.value(9).toString());
        c.setDebt(query.value(10).toDouble());
        list.append(c);
    }
    if (list.isEmpty()) return list;

    // Same N+1 fix as getAll(): this used to call loadPhonesAndAddresses()
    // and loadFavorites() per row here — for a 200-row page that's 600
    // round trips to Supabase before the page could even render. Batch
    // it into 3 queries scoped to just this page's ids.
    QStringList idStrs;
    idStrs.reserve(list.size());
    for (const Customer& c : list) idStrs << QString::number(c.id());
    const QString idList = idStrs.join(",");

    QHash<int, QList<Phone>>   phonesByCustomer;
    QHash<int, QList<Address>> addressesByCustomer;
    QHash<int, QList<int>>     favoritesByCustomer;
    {
        QSqlQuery q(QString("SELECT CustomerId, PhoneNumber FROM CustomerPhones WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            phonesByCustomer[q.value(0).toInt()].append(Phone{q.value(1).toString()});
    }
    {
        QSqlQuery q(QString("SELECT CustomerId, AddressText FROM CustomerAddresses WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            addressesByCustomer[q.value(0).toInt()].append(Address{q.value(1).toString()});
    }
    {
        QSqlQuery q(QString("SELECT CustomerId, ProductId FROM CustomerFavorites WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            favoritesByCustomer[q.value(0).toInt()].append(q.value(1).toInt());
    }

    for (Customer& c : list) {
        c.setPhones(phonesByCustomer.value(c.id()));
        c.setAddresses(addressesByCustomer.value(c.id()));
        c.setFavoriteProductIds(favoritesByCustomer.value(c.id()));
    }
    return list;
}

int AccessCustomerRepository::getTotalCount() {
    QSqlQuery query("SELECT COUNT(*) FROM Customers", db());
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool AccessCustomerRepository::save(Customer& customer) {
    QSqlQuery query(db());
    bool success = false;
    
    db().transaction();

    if (customer.id() > 0) {
        query.prepare("UPDATE Customers SET Name = ?, DistanceKm = ?, RegionId = ?, Notes = ?, "
                      "FirstContactDate = ?, LastOrderDate = ?, Status = ?, PreferredPaymentMethod = ?, "
                      "PreferredPaymentOther = ?, Debt = ? WHERE Id = ?");
        query.addBindValue(customer.name());
        query.addBindValue(customer.distanceKm());
        query.addBindValue(customer.regionId());
        query.addBindValue(customer.notes());
        query.addBindValue(customer.firstContactDate().toString(Qt::ISODate));
        query.addBindValue(customer.lastOrderDate().toString(Qt::ISODate));
        query.addBindValue(Customer::statusToString(customer.status()));
        query.addBindValue(customer.preferredPaymentMethod());
        query.addBindValue(customer.preferredPaymentOther());
        query.addBindValue(customer.debt());
        query.addBindValue(customer.id());
        success = query.exec();
    } else {
        query.prepare("INSERT INTO Customers (Name, DistanceKm, RegionId, Notes, FirstContactDate, LastOrderDate, Status, "
                      "PreferredPaymentMethod, PreferredPaymentOther, Debt) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(customer.name());
        query.addBindValue(customer.distanceKm());
        query.addBindValue(customer.regionId());
        query.addBindValue(customer.notes());
        query.addBindValue(customer.firstContactDate().toString(Qt::ISODate));
        query.addBindValue(customer.lastOrderDate().toString(Qt::ISODate));
        query.addBindValue(Customer::statusToString(customer.status()));
        query.addBindValue(customer.preferredPaymentMethod());
        query.addBindValue(customer.preferredPaymentOther());
        query.addBindValue(customer.debt());
        
        if (query.exec()) {
            customer.setId(getAccessInsertId(query, db()));
            success = true;
        }
    }

    if (success) {
        success = savePhonesAndAddresses(customer) && saveFavorites(customer);
    }

    if (success) {
        db().commit();
    } else {
        db().rollback();
        Logger::instance().error("Failed to save Access Customer: " + query.lastError().text());
    }
    return success;
}

bool AccessCustomerRepository::remove(int id) {
    QSqlQuery query(db());
    db().transaction();
    
    query.prepare("DELETE FROM CustomerPhones WHERE CustomerId = ?");
    query.addBindValue(id);
    query.exec();

    query.prepare("DELETE FROM CustomerAddresses WHERE CustomerId = ?");
    query.addBindValue(id);
    query.exec();

    query.prepare("DELETE FROM CustomerFavorites WHERE CustomerId = ?");
    query.addBindValue(id);
    query.exec();

    query.prepare("DELETE FROM Customers WHERE Id = ?");
    query.addBindValue(id);
    bool success = query.exec();

    if (success) {
        db().commit();
    } else {
        db().rollback();
    }
    return success;
}

QList<Customer> AccessCustomerRepository::search(const QString& keyword) {
    QList<Customer> list;
    QSqlQuery query(db());
    QString sql = "SELECT DISTINCT c.Id, c.Name, c.DistanceKm, c.RegionId, c.Notes, c.FirstContactDate, c.LastOrderDate, c.Status "
                  "FROM Customers c "
                  "LEFT JOIN CustomerPhones cp ON c.Id = cp.CustomerId "
                  "LEFT JOIN CustomerAddresses ca ON c.Id = ca.CustomerId "
                  "LEFT JOIN Regions r ON c.RegionId = r.Id "
                  "WHERE c.Name LIKE ? OR cp.PhoneNumber LIKE ? OR ca.AddressText LIKE ? OR r.Name LIKE ?";
    query.prepare(sql);
    QString boundVal = "%" + keyword + "%";
    query.addBindValue(boundVal);
    query.addBindValue(boundVal);
    query.addBindValue(boundVal);
    query.addBindValue(boundVal);

    if (!query.exec()) return list;

    while (query.next()) {
        Customer c(query.value(0).toInt(), query.value(1).toString());
        c.setDistanceKm(query.value(2).toDouble());
        c.setRegionId(query.value(3).toInt());
        c.setNotes(query.value(4).toString());
        c.setFirstContactDate(QDateTime::fromString(query.value(5).toString(), Qt::ISODate));
        c.setLastOrderDate(QDateTime::fromString(query.value(6).toString(), Qt::ISODate));
        c.setStatus(Customer::stringToStatus(query.value(7).toString()));
        list.append(c);
    }
    if (list.isEmpty()) return list;

    QStringList idStrs;
    idStrs.reserve(list.size());
    for (const Customer& c : list) idStrs << QString::number(c.id());
    const QString idList = idStrs.join(",");

    QHash<int, QList<Phone>>   phonesByCustomer;
    QHash<int, QList<Address>> addressesByCustomer;
    QHash<int, QList<int>>     favoritesByCustomer;
    {
        QSqlQuery q(QString("SELECT CustomerId, PhoneNumber FROM CustomerPhones WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            phonesByCustomer[q.value(0).toInt()].append(Phone{q.value(1).toString()});
    }
    {
        QSqlQuery q(QString("SELECT CustomerId, AddressText FROM CustomerAddresses WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            addressesByCustomer[q.value(0).toInt()].append(Address{q.value(1).toString()});
    }
    {
        QSqlQuery q(QString("SELECT CustomerId, ProductId FROM CustomerFavorites WHERE CustomerId IN (%1)").arg(idList), db());
        while (q.next())
            favoritesByCustomer[q.value(0).toInt()].append(q.value(1).toInt());
    }

    for (Customer& c : list) {
        c.setPhones(phonesByCustomer.value(c.id()));
        c.setAddresses(addressesByCustomer.value(c.id()));
        c.setFavoriteProductIds(favoritesByCustomer.value(c.id()));
    }
    return list;
}

// ==========================================
// AccessProductRepository
// ==========================================
QList<Product> AccessProductRepository::getAll() {
    QList<Product> list;
    QSqlQuery query("SELECT \"Id\", \"Name\", \"Price\", \"CategoryId\", \"Status\", \"Barcode\", \"StockQty\", \"LowStockThreshold\", "
                    "\"ManufactureDate\", \"ExpiryDate\", \"CostPrice\", \"UnitLabel\", \"ImagePath\", \"HasTax\" "
                    "FROM \"Products\"", db());
    while (query.next()) {
        Product p(
            query.value(0).toInt(),
            query.value(1).toString(),
            query.value(2).toDouble(),
            query.value(3).toInt(),
            Product::stringToStatus(query.value(4).toString())
        );
        p.setBarcode(query.value(5).toString());
        p.setStockQty(query.value(6).toInt());
        p.setLowStockThreshold(query.value(7).toInt());
        
        QString mfgDateStr = query.value(8).toString();
        if (!mfgDateStr.isEmpty()) {
            p.setManufactureDate(QDate::fromString(mfgDateStr, Qt::ISODate));
        }
        
        QString expDateStr = query.value(9).toString();
        if (!expDateStr.isEmpty()) {
            p.setExpiryDate(QDate::fromString(expDateStr, Qt::ISODate));
        }
        
        p.setCostPrice(query.value(10).toDouble());
        p.setUnitLabel(query.value(11).toString());
        p.setImagePath(query.value(12).toString());
        p.setHasTax(query.value(13).toInt() != 0);
        
        list.append(p);
    }
    return list;
}

Product AccessProductRepository::getById(int id) {
    Product p;
    QSqlQuery query(db());
    query.prepare("SELECT \"Id\", \"Name\", \"Price\", \"CategoryId\", \"Status\", \"Barcode\", \"StockQty\", \"LowStockThreshold\", "
                  "\"ManufactureDate\", \"ExpiryDate\", \"CostPrice\", \"UnitLabel\", \"ImagePath\", \"HasTax\" "
                  "FROM \"Products\" WHERE \"Id\" = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        p.setId(query.value(0).toInt());
        p.setName(query.value(1).toString());
        p.setPrice(query.value(2).toDouble());
        p.setCategoryId(query.value(3).toInt());
        p.setStatus(Product::stringToStatus(query.value(4).toString()));
        p.setBarcode(query.value(5).toString());
        p.setStockQty(query.value(6).toInt());
        p.setLowStockThreshold(query.value(7).toInt());
        
        QString mfgDateStr = query.value(8).toString();
        if (!mfgDateStr.isEmpty()) {
            p.setManufactureDate(QDate::fromString(mfgDateStr, Qt::ISODate));
        }
        
        QString expDateStr = query.value(9).toString();
        if (!expDateStr.isEmpty()) {
            p.setExpiryDate(QDate::fromString(expDateStr, Qt::ISODate));
        }
        
        p.setCostPrice(query.value(10).toDouble());
        p.setUnitLabel(query.value(11).toString());
        p.setImagePath(query.value(12).toString());
        p.setHasTax(query.value(13).toInt() != 0);
    }
    return p;
}

QList<Product> AccessProductRepository::getPage(int offset, int limit) {
    QList<Product> list;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name, Price, CategoryId, Status, Barcode, StockQty, LowStockThreshold, ManufactureDate, ExpiryDate FROM Products LIMIT ? OFFSET ?");
    query.addBindValue(limit);
    query.addBindValue(offset);
    if (query.exec()) {
        while (query.next()) {
            Product p(
                query.value(0).toInt(),
                query.value(1).toString(),
                query.value(2).toDouble(),
                query.value(3).toInt(),
                Product::stringToStatus(query.value(4).toString())
            );
            p.setBarcode(query.value(5).toString());
            p.setStockQty(query.value(6).toInt());
            p.setLowStockThreshold(query.value(7).toInt());
            
            QString mfgDateStr = query.value(8).toString();
            if (!mfgDateStr.isEmpty()) {
                p.setManufactureDate(QDate::fromString(mfgDateStr, Qt::ISODate));
            }
            
            QString expDateStr = query.value(9).toString();
            if (!expDateStr.isEmpty()) {
                p.setExpiryDate(QDate::fromString(expDateStr, Qt::ISODate));
            }
            
            list.append(p);
        }
    }
    return list;
}

int AccessProductRepository::getTotalCount() {
    QSqlQuery query("SELECT COUNT(*) FROM Products", db());
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool AccessProductRepository::save(Product& product) {
    QSqlQuery query(db());
    bool isNew = product.id() == 0;
    double oldPrice = 0.0;
    
    if (!isNew) {
        Product oldProd = getById(product.id());
        oldPrice = oldProd.price();
    }

    db().transaction();

    bool success = false;
    if (product.id() > 0) {
        query.prepare("UPDATE \"Products\" SET \"Name\" = ?, \"Price\" = ?, \"CategoryId\" = ?, \"Status\" = ?, "
                      "\"Barcode\" = ?, \"StockQty\" = ?, \"LowStockThreshold\" = ?, \"ManufactureDate\" = ?, \"ExpiryDate\" = ?, "
                      "\"CostPrice\" = ?, \"UnitLabel\" = ?, \"ImagePath\" = ?, \"HasTax\" = ? "
                      "WHERE \"Id\" = ?");
        query.addBindValue(product.name());
        query.addBindValue(product.price());
        query.addBindValue(product.categoryId());
        query.addBindValue(Product::statusToString(product.status()));
        query.addBindValue(product.barcode());
        query.addBindValue(product.stockQty());
        query.addBindValue(product.lowStockThreshold());
        query.addBindValue(product.manufactureDate().isValid() 
                          ? product.manufactureDate().toString(Qt::ISODate) 
                          : QString());
        query.addBindValue(product.expiryDate().isValid() 
                          ? product.expiryDate().toString(Qt::ISODate) 
                          : QString());
        query.addBindValue(product.costPrice());
        query.addBindValue(product.unitLabel());
        query.addBindValue(product.imagePath());
        query.addBindValue(product.hasTax() ? 1 : 0);
        query.addBindValue(product.id());
        success = query.exec();
    } else {
        query.prepare("INSERT INTO \"Products\" (\"Name\", \"Price\", \"CategoryId\", \"Status\", "
                      "\"Barcode\", \"StockQty\", \"LowStockThreshold\", \"ManufactureDate\", \"ExpiryDate\", "
                      "\"CostPrice\", \"UnitLabel\", \"ImagePath\", \"HasTax\") "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(product.name());
        query.addBindValue(product.price());
        query.addBindValue(product.categoryId());
        query.addBindValue(Product::statusToString(product.status()));
        query.addBindValue(product.barcode());
        query.addBindValue(product.stockQty());
        query.addBindValue(product.lowStockThreshold());
        query.addBindValue(product.manufactureDate().isValid() 
                          ? product.manufactureDate().toString(Qt::ISODate) 
                          : QString());
        query.addBindValue(product.expiryDate().isValid() 
                          ? product.expiryDate().toString(Qt::ISODate) 
                          : QString());
        query.addBindValue(product.costPrice());
        query.addBindValue(product.unitLabel());
        query.addBindValue(product.imagePath());
        query.addBindValue(product.hasTax() ? 1 : 0);
        if (query.exec()) {
            product.setId(getAccessInsertId(query, db()));
            success = true;
        }
    }

    if (success) {
        if (isNew || qAbs(product.price() - oldPrice) > 0.001) {
            success = addPriceHistoryEntry(product.id(), product.price(), QDateTime::currentDateTime());
        }
    }

    if (success) {
        db().commit();
    } else {
        db().rollback();
        Logger::instance().error("Failed to save Access Product: " + query.lastError().text());
    }
    return success;
}

bool AccessProductRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Products WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

QList<Product> AccessProductRepository::search(const QString& keyword) {
    QList<Product> list;
    QSqlQuery query(db());
    query.prepare("SELECT p.Id, p.Name, p.Price, p.CategoryId, p.Status, p.Barcode, p.StockQty, p.LowStockThreshold, p.ManufactureDate, p.ExpiryDate FROM Products p "
                  "LEFT JOIN Categories c ON p.CategoryId = c.Id "
                  "WHERE p.Name LIKE ? OR c.Name LIKE ?");
    QString val = "%" + keyword + "%";
    query.addBindValue(val);
    query.addBindValue(val);
    if (query.exec()) {
        while (query.next()) {
            Product p(
                query.value(0).toInt(),
                query.value(1).toString(),
                query.value(2).toDouble(),
                query.value(3).toInt(),
                Product::stringToStatus(query.value(4).toString())
            );
            p.setBarcode(query.value(5).toString());
            p.setStockQty(query.value(6).toInt());
            p.setLowStockThreshold(query.value(7).toInt());
            
            QString mfgDateStr = query.value(8).toString();
            if (!mfgDateStr.isEmpty()) {
                p.setManufactureDate(QDate::fromString(mfgDateStr, Qt::ISODate));
            }
            
            QString expDateStr = query.value(9).toString();
            if (!expDateStr.isEmpty()) {
                p.setExpiryDate(QDate::fromString(expDateStr, Qt::ISODate));
            }
            
            list.append(p);
        }
    }
    return list;
}

QList<ProductPriceHistory> AccessProductRepository::getPriceHistory(int productId) {
    QList<ProductPriceHistory> list;
    QSqlQuery query(db());
    query.prepare("SELECT ProductId, Price, EffectiveDate FROM ProductPriceHistory WHERE ProductId = ? ORDER BY EffectiveDate DESC");
    query.addBindValue(productId);
    if (query.exec()) {
        while (query.next()) {
            ProductPriceHistory entry;
            entry.productId = query.value(0).toInt();
            entry.price = query.value(1).toDouble();
            entry.effectiveDate = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
            list.append(entry);
        }
    }
    return list;
}

bool AccessProductRepository::addPriceHistoryEntry(int productId, double price, const QDateTime& effectiveDate) {
    QSqlQuery query(db());
    query.prepare("INSERT INTO ProductPriceHistory (ProductId, Price, EffectiveDate) VALUES (?, ?, ?)");
    query.addBindValue(productId);
    query.addBindValue(price);
    query.addBindValue(effectiveDate.toString(Qt::ISODate));
    return query.exec();
}

// ==========================================
// AccessOrderRepository
// ==========================================

void AccessOrderRepository::loadItems(Order& order) {
    QSqlQuery query(db());
    query.prepare("SELECT oi.ProductId, p.Name, oi.Quantity, oi.UnitPrice "
                  "FROM OrderItems oi "
                  "LEFT JOIN Products p ON oi.ProductId = p.Id "
                  "WHERE oi.OrderId = ?");
    query.addBindValue(order.id());
    if (query.exec()) {
        QList<OrderItem> items;
        while (query.next()) {
            OrderItem item;
            item.productId = query.value(0).toInt();
            item.productName = query.value(1).toString();
            item.quantity = query.value(2).toInt();
            item.unitPrice = query.value(3).toDouble();
            items.append(item);
        }
        order.setItems(items);
    }
}

bool AccessOrderRepository::saveItems(const Order& order) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM OrderItems WHERE OrderId = ?");
    query.addBindValue(order.id());
    if (!query.exec()) return false;

    for (const auto& item : order.items()) {
        query.prepare("INSERT INTO OrderItems (OrderId, ProductId, Quantity, UnitPrice) VALUES (?, ?, ?, ?)");
        query.addBindValue(order.id());
        query.addBindValue(item.productId);
        query.addBindValue(item.quantity);
        query.addBindValue(item.unitPrice);
        if (!query.exec()) return false;
    }
    return true;
}

QList<Order> AccessOrderRepository::getAll() {
    QList<Order> list;

    // Same N+1 fix as customers: one query for ALL order items instead of
    // one query per order. Orders is usually the biggest table in the app,
    // so this loop was likely the single largest contributor to the load time.
    QHash<int, QList<OrderItem>> itemsByOrder;
    {
        QSqlQuery q("SELECT oi.OrderId, oi.ProductId, p.Name, oi.Quantity, oi.UnitPrice "
                    "FROM OrderItems oi LEFT JOIN Products p ON oi.ProductId = p.Id", db());
        while (q.next()) {
            OrderItem item;
            item.productId   = q.value(1).toInt();
            item.productName = q.value(2).toString();
            item.quantity    = q.value(3).toInt();
            item.unitPrice   = q.value(4).toDouble();
            itemsByOrder[q.value(0).toInt()].append(item);
        }
    }

    QSqlQuery query("SELECT o.\"Id\", o.\"CustomerId\", c.\"Name\", o.\"RegisterSessionId\", o.\"DateTime\", o.\"DeliveryFee\", o.\"Subtotal\", o.\"GrandTotal\", "
                    "o.\"Status\", o.\"CancelReason\", o.\"DriverId\", o.\"DriverName\", "
                    "o.\"DiscountAmount\", o.\"DiscountReason\", o.\"PaymentMethod\", o.\"PaymentOtherDetail\", o.\"InvoiceNumber\", o.\"InvoiceBarcode\" "
                    "FROM \"Orders\" o "
                    "LEFT JOIN \"Customers\" c ON o.\"CustomerId\" = c.\"Id\"", db());
    while (query.next()) {
        Order o;
        o.setId(query.value(0).toInt());
        o.setCustomerId(query.value(1).toInt());
        o.setCustomerName(query.value(2).toString());
        o.setRegisterSessionId(query.value(3).toInt());
        o.setDateTime(QDateTime::fromString(query.value(4).toString(), Qt::ISODate));
        o.setDeliveryFee(query.value(5).toDouble());
        o.setItems(itemsByOrder.value(o.id()));
        // Now load the missing fields:
        o.setStatus(query.value(8).toString());
        o.setCancelReason(query.value(9).toString());
        o.setDriverId(query.value(10).toInt());
        o.setDriverName(query.value(11).toString());
        o.setDiscountAmount(query.value(12).toDouble());
        o.setDiscountReason(query.value(13).toString());
        o.setPaymentMethod(query.value(14).toString());
        o.setPaymentOtherDetail(query.value(15).toString());
        o.setInvoiceNumber(query.value(16).toInt());
        o.setInvoiceBarcode(query.value(17).toString());
        list.append(o);
    }
    return list;
}

Order AccessOrderRepository::getById(int id) {
    Order o;
    QSqlQuery query(db());
    query.prepare("SELECT o.\"Id\", o.\"CustomerId\", c.\"Name\", o.\"RegisterSessionId\", o.\"DateTime\", o.\"DeliveryFee\", o.\"Subtotal\", o.\"GrandTotal\", "
                  "o.\"Status\", o.\"CancelReason\", o.\"DriverId\", o.\"DriverName\", "
                  "o.\"DiscountAmount\", o.\"DiscountReason\", o.\"PaymentMethod\", o.\"PaymentOtherDetail\", o.\"InvoiceNumber\", o.\"InvoiceBarcode\" "
                  "FROM \"Orders\" o "
                  "LEFT JOIN \"Customers\" c ON o.\"CustomerId\" = c.\"Id\" "
                  "WHERE o.\"Id\" = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        o.setId(query.value(0).toInt());
        o.setCustomerId(query.value(1).toInt());
        o.setCustomerName(query.value(2).toString());
        o.setRegisterSessionId(query.value(3).toInt());
        o.setDateTime(QDateTime::fromString(query.value(4).toString(), Qt::ISODate));
        o.setDeliveryFee(query.value(5).toDouble());
        o.setStatus(query.value(8).toString());
        o.setCancelReason(query.value(9).toString());
        o.setDriverId(query.value(10).toInt());
        o.setDriverName(query.value(11).toString());
        o.setDiscountAmount(query.value(12).toDouble());
        o.setDiscountReason(query.value(13).toString());
        o.setPaymentMethod(query.value(14).toString());
        o.setPaymentOtherDetail(query.value(15).toString());
        o.setInvoiceNumber(query.value(16).toInt());
        o.setInvoiceBarcode(query.value(17).toString());
        loadItems(o);
    }
    return o;
}

QList<Order> AccessOrderRepository::getPage(int offset, int limit) {
    QList<Order> list;
    QSqlQuery query(db());
    query.prepare("SELECT o.Id, o.CustomerId, c.Name, o.DateTime, o.DeliveryFee, o.Subtotal, o.GrandTotal, "
                  "o.Status, o.CancelReason, o.DriverId, o.DriverName, "
                  "o.DiscountAmount, o.DiscountReason, o.PaymentMethod, o.PaymentOtherDetail, o.InvoiceNumber, o.InvoiceBarcode "
                  "FROM Orders o "
                  "LEFT JOIN Customers c ON o.CustomerId = c.Id "
                  "LIMIT ? OFFSET ?");
    query.addBindValue(limit);
    query.addBindValue(offset);
    if (!query.exec()) return list;

    while (query.next()) {
        Order o;
        o.setId(query.value(0).toInt());
        o.setCustomerId(query.value(1).toInt());
        o.setCustomerName(query.value(2).toString());
        o.setDateTime(QDateTime::fromString(query.value(3).toString(), Qt::ISODate));
        o.setDeliveryFee(query.value(4).toDouble());
        o.setStatus(query.value(7).toString());
        o.setCancelReason(query.value(8).toString());
        o.setDriverId(query.value(9).toInt());
        o.setDriverName(query.value(10).toString());
        o.setDiscountAmount(query.value(11).toDouble());
        o.setDiscountReason(query.value(12).toString());
        o.setPaymentMethod(query.value(13).toString());
        o.setPaymentOtherDetail(query.value(14).toString());
        o.setInvoiceNumber(query.value(15).toInt());
        o.setInvoiceBarcode(query.value(16).toString());
        list.append(o);
    }
    if (list.isEmpty()) return list;

    // Same N+1 fix: batch-load this page's items in one query instead of
    // one query per order.
    QStringList idStrs;
    idStrs.reserve(list.size());
    for (const Order& o : list) idStrs << QString::number(o.id());
    const QString idList = idStrs.join(",");

    QHash<int, QList<OrderItem>> itemsByOrder;
    {
        QSqlQuery q(QString("SELECT oi.OrderId, oi.ProductId, p.Name, oi.Quantity, oi.UnitPrice "
                    "FROM OrderItems oi LEFT JOIN Products p ON oi.ProductId = p.Id "
                    "WHERE oi.OrderId IN (%1)").arg(idList), db());
        while (q.next()) {
            OrderItem item;
            item.productId   = q.value(1).toInt();
            item.productName = q.value(2).toString();
            item.quantity    = q.value(3).toInt();
            item.unitPrice   = q.value(4).toDouble();
            itemsByOrder[q.value(0).toInt()].append(item);
        }
    }

    for (Order& o : list)
        o.setItems(itemsByOrder.value(o.id()));
    return list;
}

int AccessOrderRepository::getTotalCount() {
    QSqlQuery query("SELECT COUNT(*) FROM Orders", db());
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool AccessOrderRepository::save(Order& order) {
    QSqlQuery query(db());
    db().transaction();

    bool success = false;
    if (order.id() > 0) {
        query.prepare("UPDATE \"Orders\" SET \"CustomerId\" = ?, \"RegisterSessionId\" = ?, \"DateTime\" = ?, \"DeliveryFee\" = ?, \"Subtotal\" = ?, \"GrandTotal\" = ?, "
                      "\"Status\" = ?, \"CancelReason\" = ?, \"DriverId\" = ?, \"DriverName\" = ?, "
                      "\"DiscountAmount\" = ?, \"DiscountReason\" = ?, \"PaymentMethod\" = ?, \"PaymentOtherDetail\" = ?, \"InvoiceNumber\" = ?, \"InvoiceBarcode\" = ? "
                      "WHERE \"Id\" = ?");
        query.addBindValue(order.customerId());
        query.addBindValue(order.registerSessionId());
        query.addBindValue(order.dateTime().toString(Qt::ISODate));
        query.addBindValue(order.deliveryFee());
        query.addBindValue(order.subtotal());
        query.addBindValue(order.grandTotal());
        query.addBindValue(order.status());
        query.addBindValue(order.cancelReason());
        query.addBindValue(order.driverId());
        query.addBindValue(order.driverName());
        query.addBindValue(order.discountAmount());
        query.addBindValue(order.discountReason());
        query.addBindValue(order.paymentMethod());
        query.addBindValue(order.paymentOtherDetail());
        query.addBindValue(order.invoiceNumber());
        query.addBindValue(order.invoiceBarcode());
        query.addBindValue(order.id());
        success = query.exec();
    } else {
        query.prepare("INSERT INTO \"Orders\" (\"CustomerId\", \"RegisterSessionId\", \"DateTime\", \"DeliveryFee\", \"Subtotal\", \"GrandTotal\", "
                      "\"Status\", \"CancelReason\", \"DriverId\", \"DriverName\", \"DiscountAmount\", \"DiscountReason\", "
                      "\"PaymentMethod\", \"PaymentOtherDetail\", \"InvoiceNumber\", \"InvoiceBarcode\") "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(order.customerId());
        query.addBindValue(order.registerSessionId());
        query.addBindValue(order.dateTime().toString(Qt::ISODate));
        query.addBindValue(order.deliveryFee());
        query.addBindValue(order.subtotal());
        query.addBindValue(order.grandTotal());
        query.addBindValue(order.status());
        query.addBindValue(order.cancelReason());
        query.addBindValue(order.driverId());
        query.addBindValue(order.driverName());
        query.addBindValue(order.discountAmount());
        query.addBindValue(order.discountReason());
        query.addBindValue(order.paymentMethod());
        query.addBindValue(order.paymentOtherDetail());
        query.addBindValue(order.invoiceNumber());
        query.addBindValue(order.invoiceBarcode());
        if (query.exec()) {
            order.setId(getAccessInsertId(query, db()));
            success = true;
        }
    }

    if (success) {
        success = saveItems(order);
    }

    if (success) {
        query.prepare("UPDATE Customers SET LastOrderDate = ? WHERE Id = ?");
        query.addBindValue(order.dateTime().toString(Qt::ISODate));
        query.addBindValue(order.customerId());
        query.exec();
        
        db().commit();
    } else {
        db().rollback();
        Logger::instance().error("Failed to save Access Order: " + query.lastError().text());
    }
    return success;
}

bool AccessOrderRepository::remove(int id) {
    QSqlQuery query(db());
    db().transaction();

    query.prepare("DELETE FROM OrderItems WHERE OrderId = ?");
    query.addBindValue(id);
    query.exec();

    query.prepare("DELETE FROM Orders WHERE Id = ?");
    query.addBindValue(id);
    bool success = query.exec();

    if (success) {
        db().commit();
    } else {
        db().rollback();
    }
    return success;
}

QList<Order> AccessOrderRepository::getOrdersByCustomerId(int customerId) {
    QList<Order> list;
    QSqlQuery query(db());
    query.prepare("SELECT o.Id, o.CustomerId, c.Name, o.DateTime, o.DeliveryFee, o.Subtotal, o.GrandTotal "
                  "FROM Orders o "
                  "LEFT JOIN Customers c ON o.CustomerId = c.Id "
                  "WHERE o.CustomerId = ? ORDER BY o.DateTime DESC");
    query.addBindValue(customerId);
    if (query.exec()) {
        while (query.next()) {
            Order o;
            o.setId(query.value(0).toInt());
            o.setCustomerId(query.value(1).toInt());
            o.setCustomerName(query.value(2).toString());
            o.setDateTime(QDateTime::fromString(query.value(3).toString(), Qt::ISODate));
            o.setDeliveryFee(query.value(4).toDouble());
            loadItems(o);
            list.append(o);
        }
    }
    return list;
}

QList<Order> AccessOrderRepository::getOrdersDateRange(const QDateTime& start, const QDateTime& end) {
    QList<Order> list;
    QSqlQuery query(db());
    query.prepare("SELECT o.Id, o.CustomerId, c.Name, o.DateTime, o.DeliveryFee, o.Subtotal, o.GrandTotal "
                  "FROM Orders o "
                  "LEFT JOIN Customers c ON o.CustomerId = c.Id "
                  "WHERE o.DateTime BETWEEN ? AND ? ORDER BY o.DateTime ASC");
    query.addBindValue(start.toString(Qt::ISODate));
    query.addBindValue(end.toString(Qt::ISODate));
    if (query.exec()) {
        while (query.next()) {
            Order o;
            o.setId(query.value(0).toInt());
            o.setCustomerId(query.value(1).toInt());
            o.setCustomerName(query.value(2).toString());
            o.setDateTime(QDateTime::fromString(query.value(3).toString(), Qt::ISODate));
            o.setDeliveryFee(query.value(4).toDouble());
            loadItems(o);
            list.append(o);
        }
    }
    return list;
}

// ==========================================
// AccessScheduledOrderRepository
// ==========================================

void AccessScheduledOrderRepository::loadItems(ScheduledOrder& order) {
    QSqlQuery query(db());
    query.prepare("SELECT soi.ProductId, p.Name, soi.Quantity, p.Price "
                  "FROM ScheduledOrderItems soi "
                  "LEFT JOIN Products p ON soi.ProductId = p.Id "
                  "WHERE soi.ScheduledOrderId = ?");
    query.addBindValue(order.id);
    if (query.exec()) {
        QList<OrderItem> items;
        while (query.next()) {
            OrderItem item;
            item.productId = query.value(0).toInt();
            item.productName = query.value(1).toString();
            item.quantity = query.value(2).toInt();
            item.unitPrice = query.value(3).toDouble();
            items.append(item);
        }
        order.items = items;
    }
}

bool AccessScheduledOrderRepository::saveItems(const ScheduledOrder& order) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM ScheduledOrderItems WHERE ScheduledOrderId = ?");
    query.addBindValue(order.id);
    if (!query.exec()) return false;

    for (const auto& item : order.items) {
        query.prepare("INSERT INTO ScheduledOrderItems (ScheduledOrderId, ProductId, Quantity) VALUES (?, ?, ?)");
        query.addBindValue(order.id);
        query.addBindValue(item.productId);
        query.addBindValue(item.quantity);
        if (!query.exec()) return false;
    }
    return true;
}

QList<ScheduledOrder> AccessScheduledOrderRepository::getAll() {
    QList<ScheduledOrder> list;

    QHash<int, QList<OrderItem>> itemsByScheduledOrder;
    {
        QSqlQuery q("SELECT soi.ScheduledOrderId, soi.ProductId, p.Name, soi.Quantity, p.Price "
                    "FROM ScheduledOrderItems soi LEFT JOIN Products p ON soi.ProductId = p.Id", db());
        while (q.next()) {
            OrderItem item;
            item.productId   = q.value(1).toInt();
            item.productName = q.value(2).toString();
            item.quantity    = q.value(3).toInt();
            item.unitPrice   = q.value(4).toDouble();
            itemsByScheduledOrder[q.value(0).toInt()].append(item);
        }
    }

    QSqlQuery query("SELECT s.Id, s.CustomerId, c.Name, s.Weekdays, s.Time, "
                    "s.IsRecurring, s.OneTimeDate, s.RemindMinutesBefore, s.RemindRepeatInterval, s.AutoCreateOrder "
                    "FROM ScheduledOrders s "
                    "LEFT JOIN Customers c ON s.CustomerId = c.Id", db());
    while (query.next()) {
        ScheduledOrder s;
        s.id = query.value(0).toInt();
        s.customerId = query.value(1).toInt();
        s.customerName = query.value(2).toString();
        s.weekdays = query.value(3).toString();
        s.time = QTime::fromString(query.value(4).toString(), "hh:mm:ss");
        s.isRecurring = query.value(5).toBool();
        QString oneTimeDateStr = query.value(6).toString();
        if (!oneTimeDateStr.isEmpty()) {
            s.oneTimeDate = QDate::fromString(oneTimeDateStr, Qt::ISODate);
        }
        s.remindMinutesBefore = query.value(7).toInt();
        s.remindRepeatInterval = query.value(8).toInt();
        s.autoCreateOrder = query.value(9).toBool();
        s.items = itemsByScheduledOrder.value(s.id);
        list.append(s);
    }
    return list;
}

ScheduledOrder AccessScheduledOrderRepository::getById(int id) {
    ScheduledOrder s;
    QSqlQuery query(db());
    query.prepare("SELECT s.Id, s.CustomerId, c.Name, s.Weekdays, s.Time, "
                  "s.IsRecurring, s.OneTimeDate, s.RemindMinutesBefore, s.RemindRepeatInterval, s.AutoCreateOrder "
                  "FROM ScheduledOrders s "
                  "LEFT JOIN Customers c ON s.CustomerId = c.Id "
                  "WHERE s.Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        s.id = query.value(0).toInt();
        s.customerId = query.value(1).toInt();
        s.customerName = query.value(2).toString();
        s.weekdays = query.value(3).toString();
        s.time = QTime::fromString(query.value(4).toString(), "hh:mm:ss");
        s.isRecurring = query.value(5).toBool();
        QString oneTimeDateStr = query.value(6).toString();
        if (!oneTimeDateStr.isEmpty()) {
            s.oneTimeDate = QDate::fromString(oneTimeDateStr, Qt::ISODate);
        }
        s.remindMinutesBefore = query.value(7).toInt();
        s.remindRepeatInterval = query.value(8).toInt();
        s.autoCreateOrder = query.value(9).toBool();
        loadItems(s);
    }
    return s;
}

bool AccessScheduledOrderRepository::save(ScheduledOrder& order) {
    QSqlQuery query(db());
    db().transaction();

    bool success = false;
    if (order.id > 0) {
        query.prepare("UPDATE ScheduledOrders SET CustomerId = ?, Weekdays = ?, Time = ?, "
                      "IsRecurring = ?, OneTimeDate = ?, RemindMinutesBefore = ?, RemindRepeatInterval = ?, AutoCreateOrder = ? "
                      "WHERE Id = ?");
        query.addBindValue(order.customerId);
        query.addBindValue(order.weekdays);
        query.addBindValue(order.time.toString("hh:mm:ss"));
        query.addBindValue(order.isRecurring ? 1 : 0);
        query.addBindValue(order.oneTimeDate.isValid() ? order.oneTimeDate.toString(Qt::ISODate) : QString());
        query.addBindValue(order.remindMinutesBefore);
        query.addBindValue(order.remindRepeatInterval);
        query.addBindValue(order.autoCreateOrder ? 1 : 0);
        query.addBindValue(order.id);
        success = query.exec();
    } else {
        query.prepare("INSERT INTO ScheduledOrders (CustomerId, Weekdays, Time, "
                      "IsRecurring, OneTimeDate, RemindMinutesBefore, RemindRepeatInterval, AutoCreateOrder) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(order.customerId);
        query.addBindValue(order.weekdays);
        query.addBindValue(order.time.toString("hh:mm:ss"));
        query.addBindValue(order.isRecurring ? 1 : 0);
        query.addBindValue(order.oneTimeDate.isValid() ? order.oneTimeDate.toString(Qt::ISODate) : QString());
        query.addBindValue(order.remindMinutesBefore);
        query.addBindValue(order.remindRepeatInterval);
        query.addBindValue(order.autoCreateOrder ? 1 : 0);
        if (query.exec()) {
            order.id = getAccessInsertId(query, db());
            success = true;
        }
    }

    if (success) {
        success = saveItems(order);
    }

    if (success) {
        db().commit();
    } else {
        db().rollback();
        Logger::instance().error("Failed to save Access ScheduledOrder: " + query.lastError().text());
    }
    return success;
}

bool AccessScheduledOrderRepository::remove(int id) {
    QSqlQuery query(db());
    db().transaction();

    query.prepare("DELETE FROM ScheduledOrderItems WHERE ScheduledOrderId = ?");
    query.addBindValue(id);
    query.exec();

    query.prepare("DELETE FROM ScheduledOrders WHERE Id = ?");
    query.addBindValue(id);
    bool success = query.exec();

    if (success) {
        db().commit();
    } else {
        db().rollback();
    }
    return success;
}

QList<ScheduledOrder> AccessScheduledOrderRepository::getByCustomerId(int customerId) {
    QList<ScheduledOrder> list;
    QSqlQuery query(db());
    query.prepare("SELECT s.Id, s.CustomerId, c.Name, s.Weekdays, s.Time, "
                  "s.IsRecurring, s.OneTimeDate, s.RemindMinutesBefore, s.RemindRepeatInterval, s.AutoCreateOrder "
                  "FROM ScheduledOrders s "
                  "LEFT JOIN Customers c ON s.CustomerId = c.Id "
                  "WHERE s.CustomerId = ?");
    query.addBindValue(customerId);
    if (query.exec()) {
        while (query.next()) {
            ScheduledOrder s;
            s.id = query.value(0).toInt();
            s.customerId = query.value(1).toInt();
            s.customerName = query.value(2).toString();
            s.weekdays = query.value(3).toString();
            s.time = QTime::fromString(query.value(4).toString(), "hh:mm:ss");
            s.isRecurring = query.value(5).toBool();
            QString oneTimeDateStr = query.value(6).toString();
            if (!oneTimeDateStr.isEmpty()) {
                s.oneTimeDate = QDate::fromString(oneTimeDateStr, Qt::ISODate);
            }
            s.remindMinutesBefore = query.value(7).toInt();
            s.remindRepeatInterval = query.value(8).toInt();
            s.autoCreateOrder = query.value(9).toBool();
            loadItems(s);
            list.append(s);
        }
    }
    return list;
}

// ==========================================
// AccessMissingProductRepository
// ==========================================

QList<MissingProduct> AccessMissingProductRepository::getAll() {
    QList<MissingProduct> list;
    QSqlQuery query("SELECT m.Id, m.ProductId, p.Name, m.QuantityNeeded, m.BranchId, m.DateAdded, m.Purchased, "
                    "m.Barcode, m.UnitLabel, m.Source, m.CurrentQty "
                    "FROM MissingProducts m "
                    "LEFT JOIN Products p ON m.ProductId = p.Id", db());
    while (query.next()) {
        MissingProduct m;
        m.id = query.value(0).toInt();
        m.productId = query.value(1).toInt();
        m.productName = query.value(2).toString();
        m.quantityNeeded = query.value(3).toInt();
        m.branchId = query.value(4).toInt();
        m.dateAdded = QDate::fromString(query.value(5).toString(), Qt::ISODate);
        m.purchased = query.value(6).toBool();
        m.barcode = query.value(7).toString();
        m.unitLabel = query.value(8).toString();
        m.source = query.value(9).toString();
        m.currentQty = query.value(10).toInt();
        list.append(m);
    }
    return list;
}

MissingProduct AccessMissingProductRepository::getById(int id) {
    MissingProduct m;
    QSqlQuery query(db());
    query.prepare("SELECT m.Id, m.ProductId, p.Name, m.QuantityNeeded, m.BranchId, m.DateAdded, m.Purchased, "
                  "m.Barcode, m.UnitLabel, m.Source, m.CurrentQty "
                  "FROM MissingProducts m "
                  "LEFT JOIN Products p ON m.ProductId = p.Id "
                  "WHERE m.Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        m.id = query.value(0).toInt();
        m.productId = query.value(1).toInt();
        m.productName = query.value(2).toString();
        m.quantityNeeded = query.value(3).toInt();
        m.branchId = query.value(4).toInt();
        m.dateAdded = QDate::fromString(query.value(5).toString(), Qt::ISODate);
        m.purchased = query.value(6).toBool();
        m.barcode = query.value(7).toString();
        m.unitLabel = query.value(8).toString();
        m.source = query.value(9).toString();
        m.currentQty = query.value(10).toInt();
    }
    return m;
}

bool AccessMissingProductRepository::save(MissingProduct& entry) {
    QSqlQuery query(db());
    if (entry.id > 0) {
        query.prepare("UPDATE MissingProducts SET ProductId = ?, QuantityNeeded = ?, BranchId = ?, DateAdded = ?, Purchased = ?, "
                      "Barcode = ?, UnitLabel = ?, Source = ?, CurrentQty = ? WHERE Id = ?");
        query.addBindValue(entry.productId);
        query.addBindValue(entry.quantityNeeded);
        query.addBindValue(entry.branchId);
        query.addBindValue(entry.dateAdded.toString(Qt::ISODate));
        query.addBindValue(entry.purchased ? 1 : 0);
        query.addBindValue(entry.barcode);
        query.addBindValue(entry.unitLabel);
        query.addBindValue(entry.source);
        query.addBindValue(entry.currentQty);
        query.addBindValue(entry.id);
        return query.exec();
    } else {
        query.prepare("INSERT INTO MissingProducts (ProductId, QuantityNeeded, BranchId, DateAdded, Purchased, "
                      "Barcode, UnitLabel, Source, CurrentQty) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(entry.productId);
        query.addBindValue(entry.quantityNeeded);
        query.addBindValue(entry.branchId);
        query.addBindValue(entry.dateAdded.toString(Qt::ISODate));
        query.addBindValue(entry.purchased ? 1 : 0);
        query.addBindValue(entry.barcode);
        query.addBindValue(entry.unitLabel);
        query.addBindValue(entry.source);
        query.addBindValue(entry.currentQty);
        if (query.exec()) {
            entry.id = getAccessInsertId(query, db());
            return true;
        }
    }
    return false;
}

bool AccessMissingProductRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM MissingProducts WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessUserRepository
// ==========================================

QList<User> AccessUserRepository::getAll() {
    QList<User> list;
    QSqlQuery query("SELECT Id, Username, PasswordHash, PasswordSalt, RoleId, Name, Phone FROM Users", db());
    while (query.next()) {
        User u(query.value(0).toInt(), query.value(5).toString());
        u.setUsername(query.value(1).toString());
        u.setPasswordHash(query.value(2).toString());
        u.setPasswordSalt(query.value(3).toString());
        u.setRoleId(query.value(4).toInt());
        u.setPhone(query.value(6).toString());
        list.append(u);
    }
    return list;
}

User AccessUserRepository::getById(int id) {
    User u;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Username, PasswordHash, PasswordSalt, RoleId, Name, Phone FROM Users WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        u.setId(query.value(0).toInt());
        u.setName(query.value(5).toString());
        u.setUsername(query.value(1).toString());
        u.setPasswordHash(query.value(2).toString());
        u.setPasswordSalt(query.value(3).toString());
        u.setRoleId(query.value(4).toInt());
        u.setPhone(query.value(6).toString());
    }
    return u;
}

User AccessUserRepository::getByUsername(const QString& username) {
    User u;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Username, PasswordHash, PasswordSalt, RoleId, Name, Phone, Email FROM Users WHERE Username = ?");
    query.addBindValue(username);
    if (query.exec() && query.next()) {
        u.setId(query.value(0).toInt());
        u.setName(query.value(5).toString());
        u.setUsername(query.value(1).toString());
        u.setPasswordHash(query.value(2).toString());
        u.setPasswordSalt(query.value(3).toString());
        u.setRoleId(query.value(4).toInt());
        u.setPhone(query.value(6).toString());
        u.setEmail(query.value(7).toString());
    }
    return u;
}

User AccessUserRepository::getByEmail(const QString& email) {
    User u;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Username, PasswordHash, PasswordSalt, RoleId, Name, Phone, Email FROM Users WHERE Email = ?");
    query.addBindValue(email);
    if (query.exec() && query.next()) {
        u.setId(query.value(0).toInt());
        u.setName(query.value(5).toString());
        u.setUsername(query.value(1).toString());
        u.setPasswordHash(query.value(2).toString());
        u.setPasswordSalt(query.value(3).toString());
        u.setRoleId(query.value(4).toInt());
        u.setPhone(query.value(6).toString());
        u.setEmail(query.value(7).toString());
    }
    return u;
}

bool AccessUserRepository::save(User& user) {
    QSqlQuery query(db());
    if (user.id() > 0) {
        query.prepare("UPDATE Users SET Username = ?, PasswordHash = ?, PasswordSalt = ?, RoleId = ?, Name = ?, Phone = ?, Email = ? WHERE Id = ?");
        query.addBindValue(user.username());
        query.addBindValue(user.passwordHash());
        query.addBindValue(user.passwordSalt());
        query.addBindValue(user.roleId());
        query.addBindValue(user.name());
        query.addBindValue(user.phone());
        query.addBindValue(user.email());
        query.addBindValue(user.id());
        return query.exec();
    } else {
        query.prepare("INSERT INTO Users (Username, PasswordHash, PasswordSalt, RoleId, Name, Phone, Email) VALUES (?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(user.username());
        query.addBindValue(user.passwordHash());
        query.addBindValue(user.passwordSalt());
        query.addBindValue(user.roleId());
        query.addBindValue(user.name());
        query.addBindValue(user.phone());
        query.addBindValue(user.email());
        if (query.exec()) {
            user.setId(getAccessInsertId(query, db()));
            return true;
        }
    }
    return false;
}

bool AccessUserRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM Users WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// ==========================================
// AccessAuditLogRepository
// ==========================================

QList<AuditLogEntry> AccessAuditLogRepository::getAll() {
    QList<AuditLogEntry> list;
    QSqlQuery query("SELECT l.Id, l.UserId, u.Username, l.Action, l.EntityType, l.EntityId, l.Details, l.Timestamp "
                    "FROM AuditLogs l "
                    "LEFT JOIN Users u ON l.UserId = u.Id ORDER BY l.Timestamp DESC", db());
    while (query.next()) {
        AuditLogEntry entry;
        entry.id = query.value(0).toInt();
        entry.userId = query.value(1).toInt();
        entry.username = query.value(2).toString();
        entry.action = query.value(3).toString();
        entry.entityType = query.value(4).toString();
        entry.entityId = query.value(5).toInt();
        entry.details = query.value(6).toString();
        entry.timestamp = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        list.append(entry);
    }
    return list;
}

bool AccessAuditLogRepository::addEntry(const AuditLogEntry& entry) {
    QSqlQuery query(db());
    query.prepare("INSERT INTO AuditLogs (UserId, Action, EntityType, EntityId, Details, Timestamp) VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(entry.userId);
    query.addBindValue(entry.action);
    query.addBindValue(entry.entityType);
    query.addBindValue(entry.entityId);
    query.addBindValue(entry.details);
    query.addBindValue(entry.timestamp.toString(Qt::ISODate));
    return query.exec();
}

QList<AuditLogEntry> AccessAuditLogRepository::getEntriesDateRange(const QDateTime& start, const QDateTime& end) {
    QList<AuditLogEntry> list;
    QSqlQuery query(db());
    query.prepare("SELECT l.Id, l.UserId, u.Username, l.Action, l.EntityType, l.EntityId, l.Details, l.Timestamp "
                  "FROM AuditLogs l "
                  "LEFT JOIN Users u ON l.UserId = u.Id "
                  "WHERE l.Timestamp BETWEEN ? AND ? ORDER BY l.Timestamp ASC");
    query.addBindValue(start.toString(Qt::ISODate));
    query.addBindValue(end.toString(Qt::ISODate));
    if (query.exec()) {
        while (query.next()) {
            AuditLogEntry entry;
            entry.id = query.value(0).toInt();
            entry.userId = query.value(1).toInt();
            entry.username = query.value(2).toString();
            entry.action = query.value(3).toString();
            entry.entityType = query.value(4).toString();
            entry.entityId = query.value(5).toInt();
            entry.details = query.value(6).toString();
            entry.timestamp = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
            list.append(entry);
        }
    }
    return list;
}

// ==========================================
// AccessDeliveryDriverRepository — delegates to SQLite impl
// (ODBC Access doesn't change the driver logic)
// ==========================================
QList<DeliveryDriver> AccessDeliveryDriverRepository::getAll() {
    QList<DeliveryDriver> list;
    QSqlQuery query("SELECT Id, Name, Phone, NationalId, Active FROM DeliveryDrivers", db());
    while (query.next()) {
        DeliveryDriver d;
        d.setId(query.value(0).toInt());
        d.setName(query.value(1).toString());
        d.setPhone(query.value(2).toString());
        d.setNationalId(query.value(3).toString());
        d.setActive(query.value(4).toBool());
        list.append(d);
    }
    return list;
}

QList<DeliveryDriver> AccessDeliveryDriverRepository::getActive() {
    QList<DeliveryDriver> list;
    QSqlQuery query("SELECT Id, Name, Phone, NationalId, Active FROM DeliveryDrivers WHERE Active = true", db());
    while (query.next()) {
        DeliveryDriver d;
        d.setId(query.value(0).toInt());
        d.setName(query.value(1).toString());
        d.setPhone(query.value(2).toString());
        d.setNationalId(query.value(3).toString());
        d.setActive(query.value(4).toBool());
        list.append(d);
    }
    return list;
}

DeliveryDriver AccessDeliveryDriverRepository::getById(int id) {
    DeliveryDriver d;
    QSqlQuery query(db());
    query.prepare("SELECT Id, Name, Phone, NationalId, Active FROM DeliveryDrivers WHERE Id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        d.setId(query.value(0).toInt());
        d.setName(query.value(1).toString());
        d.setPhone(query.value(2).toString());
        d.setNationalId(query.value(3).toString());
        d.setActive(query.value(4).toBool());
    }
    return d;
}

bool AccessDeliveryDriverRepository::save(DeliveryDriver& d) {
    QSqlQuery query(db());
    db().transaction();
    
    bool success = false;
    if (d.id() > 0) {
        query.prepare("UPDATE DeliveryDrivers SET Name = ?, Phone = ?, NationalId = ?, Active = ? WHERE Id = ?");
        query.addBindValue(d.name());
        query.addBindValue(d.phone());
        query.addBindValue(d.nationalId());
        query.addBindValue(d.active());
        query.addBindValue(d.id());
        success = query.exec();
    } else {
        query.prepare("INSERT INTO DeliveryDrivers (Name, Phone, NationalId, Active) VALUES (?, ?, ?, ?)");
        query.addBindValue(d.name());
        query.addBindValue(d.phone());
        query.addBindValue(d.nationalId());
        query.addBindValue(d.active());
        if (query.exec()) {
            d.setId(getAccessInsertId(query, db()));
            success = true;
        }
    }
    
    if (success) {
        db().commit();
    } else {
        db().rollback();
        Logger::instance().error("Failed to save Access DeliveryDriver: " + query.lastError().text());
    }
    return success;
}

bool AccessDeliveryDriverRepository::remove(int id) {
    QSqlQuery query(db());
    query.prepare("DELETE FROM DeliveryDrivers WHERE Id = ?");
    query.addBindValue(id);
    return query.exec();
}

// AccessCouponRepository — delegates to SQLite
QList<Coupon>  AccessCouponRepository::getAll()                         { return SQLiteCouponRepository().getAll(); }
Coupon         AccessCouponRepository::getByCode(const QString& c)      { return SQLiteCouponRepository().getByCode(c); }
Coupon         AccessCouponRepository::getById(int id)                  { return SQLiteCouponRepository().getById(id); }
bool           AccessCouponRepository::save(Coupon& c)                  { return SQLiteCouponRepository().save(c); }
bool           AccessCouponRepository::remove(int id)                   { return SQLiteCouponRepository().remove(id); }
bool           AccessCouponRepository::incrementUsage(int id)           { return SQLiteCouponRepository().incrementUsage(id); }

// AccessOrderReturnRepository — delegates to SQLite
QList<OrderReturn> AccessOrderReturnRepository::getAll()                { return SQLiteOrderReturnRepository().getAll(); }
QList<OrderReturn> AccessOrderReturnRepository::getByOrderId(int oid)  { return SQLiteOrderReturnRepository().getByOrderId(oid); }
OrderReturn        AccessOrderReturnRepository::getById(int id)         { return SQLiteOrderReturnRepository().getById(id); }
bool               AccessOrderReturnRepository::save(OrderReturn& r)    { return SQLiteOrderReturnRepository().save(r); }
bool               AccessOrderReturnRepository::remove(int id)          { return SQLiteOrderReturnRepository().remove(id); }

// ==========================================
// AccessNoteRepository
// Uses the active connection (QPSQL or QSQLITE).
// Notes table is created by schema_deployer on cloud branches.
// ==========================================
#include "core/note.h"

static Note accessNoteFromQuery(const QSqlQuery& q) {
    Note n;
    n.id          = q.value(0).toInt();
    n.title       = q.value(1).toString();
    n.content     = q.value(2).toString();
    n.color       = q.value(3).toString().isEmpty() ? "yellow" : q.value(3).toString();
    n.pinned      = q.value(4).toBool();
    n.attachments = q.value(5).toString();
    n.createdAt   = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
    n.updatedAt   = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
    return n;
}

static const char* kAccessNoteSelect =
    "SELECT id,title,content,color,pinned,attachments,createdat,updatedat FROM notes";

// Older cloud schemas (deployed before Notes existed) won't have this table.
// IF NOT EXISTS makes this safe/cheap to call before every operation.
static void ensureNotesTable(QSqlDatabase db) {
    if (db.driverName() != "QPSQL") return;
    QSqlQuery q(db);
    q.exec(
        "CREATE TABLE IF NOT EXISTS \"Notes\" (Id SERIAL PRIMARY KEY, "
        "Title VARCHAR(255) NOT NULL DEFAULT '', Content TEXT NOT NULL DEFAULT '', "
        "Color VARCHAR(50) NOT NULL DEFAULT 'yellow', Pinned BOOLEAN NOT NULL DEFAULT FALSE, "
        "Attachments TEXT DEFAULT '', CreatedAt VARCHAR(50) NOT NULL DEFAULT '', "
        "UpdatedAt VARCHAR(50) NOT NULL DEFAULT '')");
}

QList<Note> AccessNoteRepository::getAll() {
    ensureNotesTable(db());
    QList<Note> list;
    QSqlQuery q(QString("%1 ORDER BY pinned DESC, updatedat DESC").arg(kAccessNoteSelect), db());
    while (q.next()) list.append(accessNoteFromQuery(q));
    return list;
}

Note AccessNoteRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE id = ?").arg(kAccessNoteSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return accessNoteFromQuery(q);
    return {};
}

bool AccessNoteRepository::save(Note& n) {
    DatabaseConnectionManager::ensureSearchPath(db());  // CRITICAL: set search_path before query
    ensureNotesTable(db());
    QSqlQuery q(db());
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const bool isPsql = (db().driverName() == "QPSQL");

    if (n.id > 0) {
        n.updatedAt = QDateTime::currentDateTime();
        if (isPsql) {
            // QPSQL: use exec(QString) with escaped literals, lowercase unquoted identifiers
            QString title = n.title; title.replace("'", "''");
            QString content = n.content; content.replace("'", "''");
            QString color = n.color.isEmpty() ? "yellow" : n.color; color.replace("'", "''");
            QString attachments = n.attachments.isNull() ? "" : n.attachments; attachments.replace("'", "''");
            QString updatedAt = n.updatedAt.toString(Qt::ISODate); updatedAt.replace("'", "''");
            
            QString sql = QString("UPDATE notes SET title='%1',content='%2',color='%3',"
                                  "pinned=%4,attachments='%5',updatedat='%6' WHERE id=%7")
                              .arg(title, content, color)
                              .arg(n.pinned ? "true" : "false")
                              .arg(attachments, updatedAt)
                              .arg(n.id);
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"Notes\" SET \"Title\"=?,\"Content\"=?,\"Color\"=?,"
                      "\"Pinned\"=?,\"Attachments\"=?,\"UpdatedAt\"=? WHERE \"Id\"=?");
            q.addBindValue(n.title);
            q.addBindValue(n.content);
            q.addBindValue(n.color.isEmpty() ? "yellow" : n.color);
            q.addBindValue(n.pinned);
            q.addBindValue(n.attachments.isNull() ? QString("") : n.attachments);
            q.addBindValue(n.updatedAt.toString(Qt::ISODate));
            q.addBindValue(n.id);
            return q.exec();
        }
    } else {
        n.createdAt = QDateTime::currentDateTime();
        n.updatedAt = n.createdAt;
        if (isPsql) {
            // QPSQL: use exec(QString) + RETURNING, lowercase unquoted identifiers
            QString title = n.title; title.replace("'", "''");
            QString content = n.content; content.replace("'", "''");
            QString color = n.color.isEmpty() ? "yellow" : n.color; color.replace("'", "''");
            QString attachments = n.attachments.isNull() ? "" : n.attachments; attachments.replace("'", "''");
            QString createdAt = n.createdAt.toString(Qt::ISODate); createdAt.replace("'", "''");
            QString updatedAt = n.updatedAt.toString(Qt::ISODate); updatedAt.replace("'", "''");
            
            QString sql = QString("INSERT INTO notes (title,content,color,pinned,"
                                  "attachments,createdat,updatedat) VALUES ('%1','%2','%3',%4,'%5','%6','%7') RETURNING id")
                              .arg(title, content, color)
                              .arg(n.pinned ? "true" : "false")
                              .arg(attachments, createdAt, updatedAt);
            if (!q.exec(sql)) return false;
            if (q.next()) n.id = q.value(0).toInt();
            return true;
        } else {
            q.prepare("INSERT INTO \"Notes\" (\"Title\",\"Content\",\"Color\",\"Pinned\","
                      "\"Attachments\",\"CreatedAt\",\"UpdatedAt\") VALUES (?,?,?,?,?,?,?)");
            q.addBindValue(n.title);
            q.addBindValue(n.content);
            q.addBindValue(n.color.isEmpty() ? "yellow" : n.color);
            q.addBindValue(n.pinned);
            q.addBindValue(n.attachments.isNull() ? QString("") : n.attachments);
            q.addBindValue(n.createdAt.toString(Qt::ISODate));
            q.addBindValue(n.updatedAt.toString(Qt::ISODate));
            if (!q.exec()) return false;
            n.id = q.lastInsertId().toInt();
            return true;
        }
    }
}

bool AccessNoteRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());  // CRITICAL: set search_path before query
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM notes WHERE id = %1").arg(id));
    } else {
        q.prepare("DELETE FROM \"Notes\" WHERE \"Id\" = ?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Phase 1: New Access Repository Implementations
// ══════════════════════════════════════════════════════════════════════════════

// ==========================================
// AccessSupplierRepository
// ==========================================
static Supplier accessSupplierFromQuery(const QSqlQuery& q) {
    Supplier s;
    s.setId(q.value(0).toInt());
    s.setName(q.value(1).toString());
    s.setContactName(q.value(2).toString());
    s.setPhone(q.value(3).toString());
    s.setEmail(q.value(4).toString());
    s.setAddress(q.value(5).toString());
    s.setNotes(q.value(6).toString());
    s.setActive(q.value(7).toBool());
    return s;
}

static const char* kAccessSupplierSelect = 
    "SELECT id,name,contactname,phone,email,address,notes,active FROM suppliers";

QList<Supplier> AccessSupplierRepository::getAll() {
    QList<Supplier> list;
    QSqlQuery q(QString("%1 ORDER BY name").arg(kAccessSupplierSelect), db());
    while (q.next()) list.append(accessSupplierFromQuery(q));
    return list;
}

QList<Supplier> AccessSupplierRepository::getActive() {
    QList<Supplier> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE active=true ORDER BY name").arg(kAccessSupplierSelect));
    } else {
        q.prepare(QString("%1 WHERE active=1 ORDER BY name").arg(kAccessSupplierSelect));
        q.exec();
    }
    while (q.next()) list.append(accessSupplierFromQuery(q));
    return list;
}

Supplier AccessSupplierRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessSupplierSelect).arg(id)) && q.next())
            return accessSupplierFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessSupplierSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessSupplierFromQuery(q);
    }
    return {};
}

bool AccessSupplierRepository::save(Supplier& s) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (s.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE suppliers SET name='%1',contactname='%2',phone='%3',email='%4',"
                                  "address='%5',notes='%6',active=%7 WHERE id=%8")
                              .arg(escape(s.name()), escape(s.contactName()), escape(s.phone()), escape(s.email()))
                              .arg(escape(s.address()), escape(s.notes()))
                              .arg(s.active() ? "true" : "false")
                              .arg(s.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"Suppliers\" SET \"Name\"=?,\"ContactName\"=?,\"Phone\"=?,\"Email\"=?,"
                      "\"Address\"=?,\"Notes\"=?,\"Active\"=? WHERE \"Id\"=?");
            q.addBindValue(s.name()); q.addBindValue(s.contactName()); q.addBindValue(s.phone()); q.addBindValue(s.email());
            q.addBindValue(s.address()); q.addBindValue(s.notes()); q.addBindValue(s.active() ? 1 : 0); q.addBindValue(s.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO suppliers (name,contactname,phone,email,address,notes,active) "
                                  "VALUES ('%1','%2','%3','%4','%5','%6',%7) RETURNING id")
                              .arg(escape(s.name()), escape(s.contactName()), escape(s.phone()), escape(s.email()))
                              .arg(escape(s.address()), escape(s.notes()))
                              .arg(s.active() ? "true" : "false");
            if (!q.exec(sql)) return false;
            if (q.next()) s.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"Suppliers\" (\"Name\",\"ContactName\",\"Phone\",\"Email\",\"Address\",\"Notes\",\"Active\") VALUES (?,?,?,?,?,?,?)");
            q.addBindValue(s.name()); q.addBindValue(s.contactName()); q.addBindValue(s.phone()); q.addBindValue(s.email());
            q.addBindValue(s.address()); q.addBindValue(s.notes()); q.addBindValue(s.active() ? 1 : 0);
            if (!q.exec()) return false;
            s.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessSupplierRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM suppliers WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"Suppliers\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessStockMovementRepository
// ==========================================
static StockMovement accessStockMovementFromQuery(const QSqlQuery& q) {
    StockMovement sm;
    sm.id = q.value(0).toInt();
    sm.productId = q.value(1).toInt();
    sm.movementType = q.value(2).toString();
    sm.quantity = q.value(3).toInt();
    sm.referenceType = q.value(4).toString();
    sm.referenceId = q.value(5).toInt();
    sm.dateTime = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
    sm.userId = q.value(7).toInt();
    sm.notes = q.value(8).toString();
    return sm;
}

static const char* kAccessStockMovementSelect =
    "SELECT id,productid,movementtype,quantity,referencetype,referenceid,datetime,userid,notes FROM stockmovements";

QList<StockMovement> AccessStockMovementRepository::getAll() {
    QList<StockMovement> list;
    QSqlQuery q(QString("%1 ORDER BY datetime DESC").arg(kAccessStockMovementSelect), db());
    while (q.next()) list.append(accessStockMovementFromQuery(q));
    return list;
}

QList<StockMovement> AccessStockMovementRepository::getByProductId(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<StockMovement> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE productid=%2 ORDER BY datetime DESC").arg(kAccessStockMovementSelect).arg(productId));
    } else {
        q.prepare(QString("%1 WHERE productid=? ORDER BY datetime DESC").arg(kAccessStockMovementSelect));
        q.addBindValue(productId);
        q.exec();
    }
    while (q.next()) list.append(accessStockMovementFromQuery(q));
    return list;
}

QList<StockMovement> AccessStockMovementRepository::getByDateRange(const QDateTime& start, const QDateTime& end) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<StockMovement> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    if (isPsql) {
        q.exec(QString("%1 WHERE datetime>='%2' AND datetime<='%3' ORDER BY datetime DESC")
                   .arg(kAccessStockMovementSelect)
                   .arg(escape(start.toString(Qt::ISODate)))
                   .arg(escape(end.toString(Qt::ISODate))));
    } else {
        q.prepare(QString("%1 WHERE datetime>=? AND datetime<=? ORDER BY datetime DESC").arg(kAccessStockMovementSelect));
        q.addBindValue(start.toString(Qt::ISODate));
        q.addBindValue(end.toString(Qt::ISODate));
        q.exec();
    }
    while (q.next()) list.append(accessStockMovementFromQuery(q));
    return list;
}

StockMovement AccessStockMovementRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessStockMovementSelect).arg(id)) && q.next())
            return accessStockMovementFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessStockMovementSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessStockMovementFromQuery(q);
    }
    return {};
}

bool AccessStockMovementRepository::save(StockMovement& sm) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (sm.id > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE stockmovements SET productid=%1,movementtype='%2',quantity=%3,"
                                  "referencetype='%4',referenceid=%5,datetime='%6',userid=%7,notes='%8' WHERE id=%9")
                              .arg(sm.productId)
                              .arg(escape(sm.movementType))
                              .arg(sm.quantity)
                              .arg(escape(sm.referenceType))
                              .arg(sm.referenceId)
                              .arg(escape(sm.dateTime.toString(Qt::ISODate)))
                              .arg(sm.userId)
                              .arg(escape(sm.notes))
                              .arg(sm.id);
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"StockMovements\" SET \"ProductId\"=?,\"MovementType\"=?,\"Quantity\"=?,"
                      "\"ReferenceType\"=?,\"ReferenceId\"=?,\"DateTime\"=?,\"UserId\"=?,\"Notes\"=? WHERE \"Id\"=?");
            q.addBindValue(sm.productId); q.addBindValue(sm.movementType); q.addBindValue(sm.quantity);
            q.addBindValue(sm.referenceType); q.addBindValue(sm.referenceId); q.addBindValue(sm.dateTime.toString(Qt::ISODate));
            q.addBindValue(sm.userId); q.addBindValue(sm.notes); q.addBindValue(sm.id);
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO stockmovements (productid,movementtype,quantity,referencetype,referenceid,datetime,userid,notes) "
                                  "VALUES (%1,'%2',%3,'%4',%5,'%6',%7,'%8') RETURNING id")
                              .arg(sm.productId)
                              .arg(escape(sm.movementType))
                              .arg(sm.quantity)
                              .arg(escape(sm.referenceType))
                              .arg(sm.referenceId)
                              .arg(escape(sm.dateTime.toString(Qt::ISODate)))
                              .arg(sm.userId)
                              .arg(escape(sm.notes));
            if (!q.exec(sql)) return false;
            if (q.next()) sm.id = q.value(0).toInt();
            return true;
        } else {
            q.prepare("INSERT INTO \"StockMovements\" (\"ProductId\",\"MovementType\",\"Quantity\",\"ReferenceType\",\"ReferenceId\",\"DateTime\",\"UserId\",\"Notes\") VALUES (?,?,?,?,?,?,?,?)");
            q.addBindValue(sm.productId); q.addBindValue(sm.movementType); q.addBindValue(sm.quantity);
            q.addBindValue(sm.referenceType); q.addBindValue(sm.referenceId); q.addBindValue(sm.dateTime.toString(Qt::ISODate));
            q.addBindValue(sm.userId); q.addBindValue(sm.notes);
            if (!q.exec()) return false;
            sm.id = q.lastInsertId().toInt();
            return true;
        }
    }
}

bool AccessStockMovementRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM stockmovements WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"StockMovements\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessRegisterSessionRepository
// ==========================================
static RegisterSession accessRegisterSessionFromQuery(const QSqlQuery& q) {
    RegisterSession rs;
    rs.setId(q.value(0).toInt());
    rs.setUserId(q.value(1).toInt());
    rs.setOpeningCash(q.value(2).toDouble());
    rs.setClosingCash(q.value(3).toDouble());
    rs.setOpenedAt(QDateTime::fromString(q.value(4).toString(), Qt::ISODate));
    rs.setClosedAt(QDateTime::fromString(q.value(5).toString(), Qt::ISODate));
    rs.setStatus(RegisterSession::stringToStatus(q.value(6).toString()));
    return rs;
}

static const char* kAccessRegisterSessionSelect =
    "SELECT id,userid,openingcash,closingcash,openedat,closedat,status FROM registersessions";

QList<RegisterSession> AccessRegisterSessionRepository::getAll() {
    QList<RegisterSession> list;
    QSqlQuery q(QString("%1 ORDER BY openedat DESC").arg(kAccessRegisterSessionSelect), db());
    while (q.next()) list.append(accessRegisterSessionFromQuery(q));
    return list;
}

QList<RegisterSession> AccessRegisterSessionRepository::getByUserId(int userId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<RegisterSession> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE userid=%2 ORDER BY openedat DESC").arg(kAccessRegisterSessionSelect).arg(userId));
    } else {
        q.prepare(QString("%1 WHERE userid=? ORDER BY openedat DESC").arg(kAccessRegisterSessionSelect));
        q.addBindValue(userId);
        q.exec();
    }
    while (q.next()) list.append(accessRegisterSessionFromQuery(q));
    return list;
}

RegisterSession AccessRegisterSessionRepository::getOpenSessionByUserId(int userId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE userid=%2 AND status='Open' ORDER BY openedat DESC LIMIT 1")
                       .arg(kAccessRegisterSessionSelect).arg(userId)) && q.next())
            return accessRegisterSessionFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE userid=? AND status='Open' ORDER BY openedat DESC LIMIT 1").arg(kAccessRegisterSessionSelect));
        q.addBindValue(userId);
        if (q.exec() && q.next()) return accessRegisterSessionFromQuery(q);
    }
    return {};
}

RegisterSession AccessRegisterSessionRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessRegisterSessionSelect).arg(id)) && q.next())
            return accessRegisterSessionFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessRegisterSessionSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessRegisterSessionFromQuery(q);
    }
    return {};
}

bool AccessRegisterSessionRepository::save(RegisterSession& rs) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (rs.id() > 0) {
        if (isPsql) {
            QString closedAtStr = rs.closedAt().isValid() ? QString("'%1'").arg(escape(rs.closedAt().toString(Qt::ISODate))) : "NULL";
            QString sql = QString("UPDATE registersessions SET userid=%1,openingcash=%2,closingcash=%3,"
                                  "openedat='%4',closedat=%5,status='%6' WHERE id=%7")
                              .arg(rs.userId())
                              .arg(rs.openingCash())
                              .arg(rs.closingCash())
                              .arg(escape(rs.openedAt().toString(Qt::ISODate)))
                              .arg(closedAtStr)
                              .arg(escape(RegisterSession::statusToString(rs.status())))
                              .arg(rs.id());
            if (!q.exec(sql)) {
                Logger::instance().error(QString("Failed to update RegisterSession #%1 (PSQL): %2").arg(rs.id()).arg(q.lastError().text()));
                return false;
            }
            return true;
        } else {
            q.prepare("UPDATE \"RegisterSessions\" SET \"UserId\"=?,\"OpeningCash\"=?,\"ClosingCash\"=?,"
                      "\"OpenedAt\"=?,\"ClosedAt\"=?,\"Status\"=? WHERE \"Id\"=?");
            q.addBindValue(rs.userId()); q.addBindValue(rs.openingCash()); q.addBindValue(rs.closingCash());
            q.addBindValue(rs.openedAt().toString(Qt::ISODate));
            q.addBindValue(rs.closedAt().isValid() ? rs.closedAt().toString(Qt::ISODate) : QVariant());
            q.addBindValue(RegisterSession::statusToString(rs.status()));
            q.addBindValue(rs.id());
            if (!q.exec()) {
                Logger::instance().error(QString("Failed to update RegisterSession #%1 (Access): %2").arg(rs.id()).arg(q.lastError().text()));
                return false;
            }
            return true;
        }
    } else {
        if (isPsql) {
            QString closedAtStr = rs.closedAt().isValid() ? QString("'%1'").arg(escape(rs.closedAt().toString(Qt::ISODate))) : "NULL";
            QString sql = QString("INSERT INTO registersessions (userid,openingcash,closingcash,openedat,closedat,status) "
                                  "VALUES (%1,%2,%3,'%4',%5,'%6') RETURNING id")
                              .arg(rs.userId())
                              .arg(rs.openingCash())
                              .arg(rs.closingCash())
                              .arg(escape(rs.openedAt().toString(Qt::ISODate)))
                              .arg(closedAtStr)
                              .arg(escape(RegisterSession::statusToString(rs.status())));
            if (!q.exec(sql)) {
                Logger::instance().error(QString("Failed to insert RegisterSession (PSQL): %1").arg(q.lastError().text()));
                return false;
            }
            if (q.next()) {
                rs.setId(q.value(0).toInt());
                Logger::instance().info(QString("Created RegisterSession #%1 for userId=%2 (PSQL)").arg(rs.id()).arg(rs.userId()));
            }
            return true;
        } else {
            q.prepare("INSERT INTO \"RegisterSessions\" (\"UserId\",\"OpeningCash\",\"ClosingCash\",\"OpenedAt\",\"ClosedAt\",\"Status\") VALUES (?,?,?,?,?,?)");
            q.addBindValue(rs.userId()); q.addBindValue(rs.openingCash()); q.addBindValue(rs.closingCash());
            q.addBindValue(rs.openedAt().toString(Qt::ISODate));
            q.addBindValue(rs.closedAt().isValid() ? rs.closedAt().toString(Qt::ISODate) : QVariant());
            q.addBindValue(RegisterSession::statusToString(rs.status()));
            if (!q.exec()) {
                Logger::instance().error(QString("Failed to insert RegisterSession (Access): %1").arg(q.lastError().text()));
                return false;
            }
            rs.setId(q.lastInsertId().toInt());
            Logger::instance().info(QString("Created RegisterSession #%1 for userId=%2 (Access)").arg(rs.id()).arg(rs.userId()));
            return true;
        }
    }
}

bool AccessRegisterSessionRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM registersessions WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"RegisterSessions\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessCashMovementRepository
// ==========================================
static CashMovement accessCashMovementFromQuery(const QSqlQuery& q) {
    CashMovement cm;
    cm.setId(q.value(0).toInt());
    cm.setRegisterSessionId(q.value(1).toInt());
    cm.setType(CashMovement::stringToType(q.value(2).toString()));
    cm.setAmount(q.value(3).toDouble());
    cm.setReason(q.value(4).toString());
    cm.setDateTime(QDateTime::fromString(q.value(5).toString(), Qt::ISODate));
    cm.setUserId(q.value(6).toInt());
    return cm;
}

static const char* kAccessCashMovementSelect =
    "SELECT id,registersessionid,type,amount,reason,datetime,userid FROM cashmovements";

QList<CashMovement> AccessCashMovementRepository::getAll() {
    QList<CashMovement> list;
    QSqlQuery q(QString("%1 ORDER BY datetime DESC").arg(kAccessCashMovementSelect), db());
    while (q.next()) list.append(accessCashMovementFromQuery(q));
    return list;
}

QList<CashMovement> AccessCashMovementRepository::getByRegisterSessionId(int sessionId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<CashMovement> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE registersessionid=%2 ORDER BY datetime DESC").arg(kAccessCashMovementSelect).arg(sessionId));
    } else {
        q.prepare(QString("%1 WHERE registersessionid=? ORDER BY datetime DESC").arg(kAccessCashMovementSelect));
        q.addBindValue(sessionId);
        q.exec();
    }
    while (q.next()) list.append(accessCashMovementFromQuery(q));
    return list;
}

CashMovement AccessCashMovementRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessCashMovementSelect).arg(id)) && q.next())
            return accessCashMovementFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessCashMovementSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessCashMovementFromQuery(q);
    }
    return {};
}

bool AccessCashMovementRepository::save(CashMovement& cm) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (cm.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE cashmovements SET registersessionid=%1,type='%2',amount=%3,"
                                  "reason='%4',datetime='%5',userid=%6 WHERE id=%7")
                              .arg(cm.registerSessionId())
                              .arg(escape(CashMovement::typeToString(cm.type())))
                              .arg(cm.amount())
                              .arg(escape(cm.reason()))
                              .arg(escape(cm.dateTime().toString(Qt::ISODate)))
                              .arg(cm.userId())
                              .arg(cm.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"CashMovements\" SET \"RegisterSessionId\"=?,\"Type\"=?,\"Amount\"=?,"
                      "\"Reason\"=?,\"DateTime\"=?,\"UserId\"=? WHERE \"Id\"=?");
            q.addBindValue(cm.registerSessionId()); q.addBindValue(CashMovement::typeToString(cm.type())); q.addBindValue(cm.amount());
            q.addBindValue(cm.reason()); q.addBindValue(cm.dateTime().toString(Qt::ISODate)); q.addBindValue(cm.userId());
            q.addBindValue(cm.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO cashmovements (registersessionid,type,amount,reason,datetime,userid) "
                                  "VALUES (%1,'%2',%3,'%4','%5',%6) RETURNING id")
                              .arg(cm.registerSessionId())
                              .arg(escape(CashMovement::typeToString(cm.type())))
                              .arg(cm.amount())
                              .arg(escape(cm.reason()))
                              .arg(escape(cm.dateTime().toString(Qt::ISODate)))
                              .arg(cm.userId());
            if (!q.exec(sql)) return false;
            if (q.next()) cm.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"CashMovements\" (\"RegisterSessionId\",\"Type\",\"Amount\",\"Reason\",\"DateTime\",\"UserId\") VALUES (?,?,?,?,?,?)");
            q.addBindValue(cm.registerSessionId()); q.addBindValue(CashMovement::typeToString(cm.type())); q.addBindValue(cm.amount());
            q.addBindValue(cm.reason()); q.addBindValue(cm.dateTime().toString(Qt::ISODate)); q.addBindValue(cm.userId());
            if (!q.exec()) return false;
            cm.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessCashMovementRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM cashmovements WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"CashMovements\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessExpenseCategoryRepository
// ==========================================
static ExpenseCategory accessExpenseCategoryFromQuery(const QSqlQuery& q) {
    ExpenseCategory ec;
    ec.setId(q.value(0).toInt());
    ec.setName(q.value(1).toString());
    ec.setDescription(q.value(2).toString());
    return ec;
}

static const char* kAccessExpenseCategorySelect =
    "SELECT id,name,description FROM expensecategories";

QList<ExpenseCategory> AccessExpenseCategoryRepository::getAll() {
    QList<ExpenseCategory> list;
    QSqlQuery q(QString("%1 ORDER BY name").arg(kAccessExpenseCategorySelect), db());
    while (q.next()) list.append(accessExpenseCategoryFromQuery(q));
    return list;
}

ExpenseCategory AccessExpenseCategoryRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessExpenseCategorySelect).arg(id)) && q.next())
            return accessExpenseCategoryFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessExpenseCategorySelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessExpenseCategoryFromQuery(q);
    }
    return {};
}

bool AccessExpenseCategoryRepository::save(ExpenseCategory& ec) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (ec.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE expensecategories SET name='%1',description='%2' WHERE id=%3")
                              .arg(escape(ec.name()), escape(ec.description())).arg(ec.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"ExpenseCategories\" SET \"Name\"=?,\"Description\"=? WHERE \"Id\"=?");
            q.addBindValue(ec.name()); q.addBindValue(ec.description()); q.addBindValue(ec.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO expensecategories (name,description) VALUES ('%1','%2') RETURNING id")
                              .arg(escape(ec.name()), escape(ec.description()));
            if (!q.exec(sql)) return false;
            if (q.next()) ec.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"ExpenseCategories\" (\"Name\",\"Description\") VALUES (?,?)");
            q.addBindValue(ec.name()); q.addBindValue(ec.description());
            if (!q.exec()) return false;
            ec.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessExpenseCategoryRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM expensecategories WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"ExpenseCategories\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessExpenseRepository
// ==========================================
static Expense accessExpenseFromQuery(const QSqlQuery& q) {
    Expense e;
    e.setId(q.value(0).toInt());
    e.setCategoryId(q.value(1).toInt());
    e.setAmount(q.value(2).toDouble());
    e.setDescription(q.value(3).toString());
    e.setDate(QDate::fromString(q.value(4).toString(), Qt::ISODate));
    e.setUserId(q.value(5).toInt());
    e.setRegisterSessionId(q.value(6).toInt());
    return e;
}

static const char* kAccessExpenseSelect =
    "SELECT id,categoryid,amount,description,date,userid,registersessionid FROM expenses";

QList<Expense> AccessExpenseRepository::getAll() {
    QList<Expense> list;
    QSqlQuery q(QString("%1 ORDER BY date DESC").arg(kAccessExpenseSelect), db());
    while (q.next()) list.append(accessExpenseFromQuery(q));
    return list;
}

QList<Expense> AccessExpenseRepository::getByDateRange(const QDate& start, const QDate& end) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Expense> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    if (isPsql) {
        q.exec(QString("%1 WHERE date>='%2' AND date<='%3' ORDER BY date DESC")
                   .arg(kAccessExpenseSelect)
                   .arg(escape(start.toString(Qt::ISODate)))
                   .arg(escape(end.toString(Qt::ISODate))));
    } else {
        q.prepare(QString("%1 WHERE date>=? AND date<=? ORDER BY date DESC").arg(kAccessExpenseSelect));
        q.addBindValue(start.toString(Qt::ISODate));
        q.addBindValue(end.toString(Qt::ISODate));
        q.exec();
    }
    while (q.next()) list.append(accessExpenseFromQuery(q));
    return list;
}

QList<Expense> AccessExpenseRepository::getByRegisterSessionId(int sessionId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Expense> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE registersessionid=%2 ORDER BY date DESC").arg(kAccessExpenseSelect).arg(sessionId));
    } else {
        q.prepare(QString("%1 WHERE registersessionid=? ORDER BY date DESC").arg(kAccessExpenseSelect));
        q.addBindValue(sessionId);
        q.exec();
    }
    while (q.next()) list.append(accessExpenseFromQuery(q));
    return list;
}

Expense AccessExpenseRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessExpenseSelect).arg(id)) && q.next())
            return accessExpenseFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessExpenseSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessExpenseFromQuery(q);
    }
    return {};
}

bool AccessExpenseRepository::save(Expense& e) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (e.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE expenses SET categoryid=%1,amount=%2,description='%3',"
                                  "date='%4',userid=%5,registersessionid=%6 WHERE id=%7")
                              .arg(e.categoryId())
                              .arg(e.amount())
                              .arg(escape(e.description()))
                              .arg(escape(e.date().toString(Qt::ISODate)))
                              .arg(e.userId())
                              .arg(e.registerSessionId())
                              .arg(e.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"Expenses\" SET \"CategoryId\"=?,\"Amount\"=?,\"Description\"=?,"
                      "\"Date\"=?,\"UserId\"=?,\"RegisterSessionId\"=? WHERE \"Id\"=?");
            q.addBindValue(e.categoryId()); q.addBindValue(e.amount()); q.addBindValue(e.description());
            q.addBindValue(e.date().toString(Qt::ISODate)); q.addBindValue(e.userId()); q.addBindValue(e.registerSessionId());
            q.addBindValue(e.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO expenses (categoryid,amount,description,date,userid,registersessionid) "
                                  "VALUES (%1,%2,'%3','%4',%5,%6) RETURNING id")
                              .arg(e.categoryId())
                              .arg(e.amount())
                              .arg(escape(e.description()))
                              .arg(escape(e.date().toString(Qt::ISODate)))
                              .arg(e.userId())
                              .arg(e.registerSessionId());
            if (!q.exec(sql)) return false;
            if (q.next()) e.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"Expenses\" (\"CategoryId\",\"Amount\",\"Description\",\"Date\",\"UserId\",\"RegisterSessionId\") VALUES (?,?,?,?,?,?)");
            q.addBindValue(e.categoryId()); q.addBindValue(e.amount()); q.addBindValue(e.description());
            q.addBindValue(e.date().toString(Qt::ISODate)); q.addBindValue(e.userId()); q.addBindValue(e.registerSessionId());
            if (!q.exec()) return false;
            e.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessExpenseRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM expenses WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"Expenses\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ==========================================
// AccessPurchaseInvoiceRepository
// ==========================================
static PurchaseInvoice accessPurchaseInvoiceFromQuery(const QSqlQuery& q) {
    PurchaseInvoice pi;
    pi.setId(q.value(0).toInt());
    pi.setSupplierId(q.value(1).toInt());
    pi.setInvoiceNumber(q.value(2).toString());
    pi.setDate(QDate::fromString(q.value(3).toString(), Qt::ISODate));
    pi.setTotalAmount(q.value(4).toDouble());
    pi.setNotes(q.value(5).toString());
    pi.setUserId(q.value(6).toInt());
    pi.setStatus(PurchaseInvoice::stringToStatus(q.value(7).toString()));
    return pi;
}

static const char* kAccessPurchaseInvoiceSelect =
    "SELECT id,supplierid,invoicenumber,date,totalamount,notes,userid,status FROM purchaseinvoices";

QList<PurchaseInvoice> AccessPurchaseInvoiceRepository::getAll() {
    QList<PurchaseInvoice> list;
    QSqlQuery q(QString("%1 ORDER BY date DESC").arg(kAccessPurchaseInvoiceSelect), db());
    while (q.next()) list.append(accessPurchaseInvoiceFromQuery(q));
    return list;
}

QList<PurchaseInvoice> AccessPurchaseInvoiceRepository::getBySupplierId(int supplierId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<PurchaseInvoice> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE supplierid=%2 ORDER BY date DESC").arg(kAccessPurchaseInvoiceSelect).arg(supplierId));
    } else {
        q.prepare(QString("%1 WHERE supplierid=? ORDER BY date DESC").arg(kAccessPurchaseInvoiceSelect));
        q.addBindValue(supplierId);
        q.exec();
    }
    while (q.next()) list.append(accessPurchaseInvoiceFromQuery(q));
    return list;
}

QList<PurchaseInvoice> AccessPurchaseInvoiceRepository::getByDateRange(const QDate& start, const QDate& end) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<PurchaseInvoice> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    if (isPsql) {
        q.exec(QString("%1 WHERE date>='%2' AND date<='%3' ORDER BY date DESC")
                   .arg(kAccessPurchaseInvoiceSelect)
                   .arg(escape(start.toString(Qt::ISODate)))
                   .arg(escape(end.toString(Qt::ISODate))));
    } else {
        q.prepare(QString("%1 WHERE date>=? AND date<=? ORDER BY date DESC").arg(kAccessPurchaseInvoiceSelect));
        q.addBindValue(start.toString(Qt::ISODate));
        q.addBindValue(end.toString(Qt::ISODate));
        q.exec();
    }
    while (q.next()) list.append(accessPurchaseInvoiceFromQuery(q));
    return list;
}

PurchaseInvoice AccessPurchaseInvoiceRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessPurchaseInvoiceSelect).arg(id)) && q.next())
            return accessPurchaseInvoiceFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessPurchaseInvoiceSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessPurchaseInvoiceFromQuery(q);
    }
    return {};
}

bool AccessPurchaseInvoiceRepository::save(PurchaseInvoice& pi) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (pi.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE purchaseinvoices SET supplierid=%1,invoicenumber='%2',date='%3',"
                                  "totalamount=%4,notes='%5',userid=%6,status='%7' WHERE id=%8")
                              .arg(pi.supplierId())
                              .arg(escape(pi.invoiceNumber()))
                              .arg(escape(pi.date().toString(Qt::ISODate)))
                              .arg(pi.totalAmount())
                              .arg(escape(pi.notes()))
                              .arg(pi.userId())
                              .arg(escape(PurchaseInvoice::statusToString(pi.status())))
                              .arg(pi.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"PurchaseInvoices\" SET \"SupplierId\"=?,\"InvoiceNumber\"=?,\"Date\"=?,"
                      "\"TotalAmount\"=?,\"Notes\"=?,\"UserId\"=?,\"Status\"=? WHERE \"Id\"=?");
            q.addBindValue(pi.supplierId()); q.addBindValue(pi.invoiceNumber()); q.addBindValue(pi.date().toString(Qt::ISODate));
            q.addBindValue(pi.totalAmount()); q.addBindValue(pi.notes()); q.addBindValue(pi.userId());
            q.addBindValue(PurchaseInvoice::statusToString(pi.status())); q.addBindValue(pi.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO purchaseinvoices (supplierid,invoicenumber,date,totalamount,notes,userid,status) "
                                  "VALUES (%1,'%2','%3',%4,'%5',%6,'%7') RETURNING id")
                              .arg(pi.supplierId())
                              .arg(escape(pi.invoiceNumber()))
                              .arg(escape(pi.date().toString(Qt::ISODate)))
                              .arg(pi.totalAmount())
                              .arg(escape(pi.notes()))
                              .arg(pi.userId())
                              .arg(escape(PurchaseInvoice::statusToString(pi.status())));
            if (!q.exec(sql)) return false;
            if (q.next()) pi.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"PurchaseInvoices\" (\"SupplierId\",\"InvoiceNumber\",\"Date\",\"TotalAmount\",\"Notes\",\"UserId\",\"Status\") VALUES (?,?,?,?,?,?,?)");
            q.addBindValue(pi.supplierId()); q.addBindValue(pi.invoiceNumber()); q.addBindValue(pi.date().toString(Qt::ISODate));
            q.addBindValue(pi.totalAmount()); q.addBindValue(pi.notes()); q.addBindValue(pi.userId());
            q.addBindValue(PurchaseInvoice::statusToString(pi.status()));
            if (!q.exec()) return false;
            pi.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessPurchaseInvoiceRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM purchaseinvoices WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"PurchaseInvoices\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

QList<PurchaseInvoiceItem> AccessPurchaseInvoiceRepository::getItems(int invoiceId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<PurchaseInvoiceItem> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("SELECT id,invoiceid,productid,quantity,unitcost,totalcost FROM purchaseinvoiceitems WHERE invoiceid=%1").arg(invoiceId));
    } else {
        q.prepare("SELECT id,invoiceid,productid,quantity,unitcost,totalcost FROM \"PurchaseInvoiceItems\" WHERE invoiceid=?");
        q.addBindValue(invoiceId);
        q.exec();
    }
    while (q.next()) {
        PurchaseInvoiceItem item;
        item.setId(q.value(0).toInt());
        item.setInvoiceId(q.value(1).toInt());
        item.setProductId(q.value(2).toInt());
        item.setQuantity(q.value(3).toInt());
        item.setUnitCost(q.value(4).toDouble());
        item.setTotalCost(q.value(5).toDouble());
        list.append(item);
    }
    return list;
}

bool AccessPurchaseInvoiceRepository::saveItems(int invoiceId, const QList<PurchaseInvoiceItem>& items) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    // Delete existing items
    if (isPsql) {
        if (!q.exec(QString("DELETE FROM purchaseinvoiceitems WHERE invoiceid=%1").arg(invoiceId)))
            return false;
    } else {
        q.prepare("DELETE FROM \"PurchaseInvoiceItems\" WHERE invoiceid=?");
        q.addBindValue(invoiceId);
        if (!q.exec()) return false;
    }
    
    // Insert new items
    for (const auto& item : items) {
        if (isPsql) {
            QString sql = QString("INSERT INTO purchaseinvoiceitems (invoiceid,productid,quantity,unitcost,totalcost) "
                                  "VALUES (%1,%2,%3,%4,%5)")
                              .arg(invoiceId)
                              .arg(item.productId())
                              .arg(item.quantity())
                              .arg(item.unitCost())
                              .arg(item.totalCost());
            if (!q.exec(sql)) return false;
        } else {
            q.prepare("INSERT INTO \"PurchaseInvoiceItems\" (\"InvoiceId\",\"ProductId\",\"Quantity\",\"UnitCost\",\"TotalCost\") VALUES (?,?,?,?,?)");
            q.addBindValue(invoiceId); q.addBindValue(item.productId()); q.addBindValue(item.quantity());
            q.addBindValue(item.unitCost()); q.addBindValue(item.totalCost());
            if (!q.exec()) return false;
        }
    }
    return true;
}

// ==========================================
// AccessStockCountRepository
// ==========================================
static StockCount accessStockCountFromQuery(const QSqlQuery& q) {
    StockCount sc;
    sc.setId(q.value(0).toInt());
    sc.setDate(QDate::fromString(q.value(1).toString(), Qt::ISODate));
    sc.setUserId(q.value(2).toInt());
    sc.setStatus(StockCount::stringToStatus(q.value(3).toString()));
    sc.setNotes(q.value(4).toString());
    return sc;
}

static const char* kAccessStockCountSelect =
    "SELECT id,date,userid,status,notes FROM stockcounts";

QList<StockCount> AccessStockCountRepository::getAll() {
    QList<StockCount> list;
    QSqlQuery q(QString("%1 ORDER BY date DESC").arg(kAccessStockCountSelect), db());
    while (q.next()) list.append(accessStockCountFromQuery(q));
    return list;
}

QList<StockCount> AccessStockCountRepository::getByDateRange(const QDate& start, const QDate& end) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<StockCount> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    if (isPsql) {
        q.exec(QString("%1 WHERE date>='%2' AND date<='%3' ORDER BY date DESC")
                   .arg(kAccessStockCountSelect)
                   .arg(escape(start.toString(Qt::ISODate)))
                   .arg(escape(end.toString(Qt::ISODate))));
    } else {
        q.prepare(QString("%1 WHERE date>=? AND date<=? ORDER BY date DESC").arg(kAccessStockCountSelect));
        q.addBindValue(start.toString(Qt::ISODate));
        q.addBindValue(end.toString(Qt::ISODate));
        q.exec();
    }
    while (q.next()) list.append(accessStockCountFromQuery(q));
    return list;
}

StockCount AccessStockCountRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessStockCountSelect).arg(id)) && q.next())
            return accessStockCountFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessStockCountSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessStockCountFromQuery(q);
    }
    return {};
}

bool AccessStockCountRepository::save(StockCount& sc) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (sc.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE stockcounts SET date='%1',userid=%2,status='%3',notes='%4' WHERE id=%5")
                              .arg(escape(sc.date().toString(Qt::ISODate)))
                              .arg(sc.userId())
                              .arg(escape(StockCount::statusToString(sc.status())))
                              .arg(escape(sc.notes()))
                              .arg(sc.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"StockCounts\" SET \"Date\"=?,\"UserId\"=?,\"Status\"=?,\"Notes\"=? WHERE \"Id\"=?");
            q.addBindValue(sc.date().toString(Qt::ISODate)); q.addBindValue(sc.userId());
            q.addBindValue(StockCount::statusToString(sc.status())); q.addBindValue(sc.notes());
            q.addBindValue(sc.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO stockcounts (date,userid,status,notes) VALUES ('%1',%2,'%3','%4') RETURNING id")
                              .arg(escape(sc.date().toString(Qt::ISODate)))
                              .arg(sc.userId())
                              .arg(escape(StockCount::statusToString(sc.status())))
                              .arg(escape(sc.notes()));
            if (!q.exec(sql)) return false;
            if (q.next()) sc.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"StockCounts\" (\"Date\",\"UserId\",\"Status\",\"Notes\") VALUES (?,?,?,?)");
            q.addBindValue(sc.date().toString(Qt::ISODate)); q.addBindValue(sc.userId());
            q.addBindValue(StockCount::statusToString(sc.status())); q.addBindValue(sc.notes());
            if (!q.exec()) return false;
            sc.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessStockCountRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM stockcounts WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"StockCounts\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

QList<StockCountItem> AccessStockCountRepository::getItems(int countId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<StockCountItem> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("SELECT id,stockcountid,productid,systemqty,actualqty,difference FROM stockcountitems WHERE stockcountid=%1").arg(countId));
    } else {
        q.prepare("SELECT id,stockcountid,productid,systemqty,actualqty,difference FROM \"StockCountItems\" WHERE stockcountid=?");
        q.addBindValue(countId);
        q.exec();
    }
    while (q.next()) {
        StockCountItem item;
        item.setId(q.value(0).toInt());
        item.setStockCountId(q.value(1).toInt());
        item.setProductId(q.value(2).toInt());
        item.setSystemQty(q.value(3).toInt());
        item.setActualQty(q.value(4).toInt());
        item.setDifference(q.value(5).toInt());
        list.append(item);
    }
    return list;
}

bool AccessStockCountRepository::saveItems(int countId, const QList<StockCountItem>& items) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    // Delete existing items
    if (isPsql) {
        if (!q.exec(QString("DELETE FROM stockcountitems WHERE stockcountid=%1").arg(countId)))
            return false;
    } else {
        q.prepare("DELETE FROM \"StockCountItems\" WHERE stockcountid=?");
        q.addBindValue(countId);
        if (!q.exec()) return false;
    }
    
    // Insert new items
    for (const auto& item : items) {
        if (isPsql) {
            QString sql = QString("INSERT INTO stockcountitems (stockcountid,productid,systemqty,actualqty,difference) "
                                  "VALUES (%1,%2,%3,%4,%5)")
                              .arg(countId)
                              .arg(item.productId())
                              .arg(item.systemQty())
                              .arg(item.actualQty())
                              .arg(item.difference());
            if (!q.exec(sql)) return false;
        } else {
            q.prepare("INSERT INTO \"StockCountItems\" (\"StockCountId\",\"ProductId\",\"SystemQty\",\"ActualQty\",\"Difference\") VALUES (?,?,?,?,?)");
            q.addBindValue(countId); q.addBindValue(item.productId()); q.addBindValue(item.systemQty());
            q.addBindValue(item.actualQty()); q.addBindValue(item.difference());
            if (!q.exec()) return false;
        }
    }
    return true;
}

// ==========================================
// AccessProductCostHistoryRepository
// ==========================================
static ProductCostHistory accessProductCostHistoryFromQuery(const QSqlQuery& q) {
    ProductCostHistory pch;
    pch.setId(q.value(0).toInt());
    pch.setProductId(q.value(1).toInt());
    pch.setCostPrice(q.value(2).toDouble());
    pch.setEffectiveDate(QDate::fromString(q.value(3).toString(), Qt::ISODate));
    pch.setPurchaseInvoiceId(q.value(4).toInt());
    return pch;
}

static const char* kAccessProductCostHistorySelect =
    "SELECT id,productid,costprice,effectivedate,purchaseinvoiceid FROM productcosthistory";

QList<ProductCostHistory> AccessProductCostHistoryRepository::getByProductId(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<ProductCostHistory> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        q.exec(QString("%1 WHERE productid=%2 ORDER BY effectivedate DESC").arg(kAccessProductCostHistorySelect).arg(productId));
    } else {
        q.prepare(QString("%1 WHERE productid=? ORDER BY effectivedate DESC").arg(kAccessProductCostHistorySelect));
        q.addBindValue(productId);
        q.exec();
    }
    while (q.next()) list.append(accessProductCostHistoryFromQuery(q));
    return list;
}

ProductCostHistory AccessProductCostHistoryRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessProductCostHistorySelect).arg(id)) && q.next())
            return accessProductCostHistoryFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessProductCostHistorySelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessProductCostHistoryFromQuery(q);
    }
    return {};
}

bool AccessProductCostHistoryRepository::save(ProductCostHistory& pch) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (pch.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE productcosthistory SET productid=%1,costprice=%2,effectivedate='%3',purchaseinvoiceid=%4 WHERE id=%5")
                              .arg(pch.productId())
                              .arg(pch.costPrice())
                              .arg(escape(pch.effectiveDate().toString(Qt::ISODate)))
                              .arg(pch.purchaseInvoiceId())
                              .arg(pch.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE \"ProductCostHistory\" SET \"ProductId\"=?,\"CostPrice\"=?,\"EffectiveDate\"=?,\"PurchaseInvoiceId\"=? WHERE \"Id\"=?");
            q.addBindValue(pch.productId()); q.addBindValue(pch.costPrice());
            q.addBindValue(pch.effectiveDate().toString(Qt::ISODate)); q.addBindValue(pch.purchaseInvoiceId());
            q.addBindValue(pch.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO productcosthistory (productid,costprice,effectivedate,purchaseinvoiceid) "
                                  "VALUES (%1,%2,'%3',%4) RETURNING id")
                              .arg(pch.productId())
                              .arg(pch.costPrice())
                              .arg(escape(pch.effectiveDate().toString(Qt::ISODate)))
                              .arg(pch.purchaseInvoiceId());
            if (!q.exec(sql)) return false;
            if (q.next()) pch.setId(q.value(0).toInt());
            return true;
        } else {
            q.prepare("INSERT INTO \"ProductCostHistory\" (\"ProductId\",\"CostPrice\",\"EffectiveDate\",\"PurchaseInvoiceId\") VALUES (?,?,?,?)");
            q.addBindValue(pch.productId()); q.addBindValue(pch.costPrice());
            q.addBindValue(pch.effectiveDate().toString(Qt::ISODate)); q.addBindValue(pch.purchaseInvoiceId());
            if (!q.exec()) return false;
            pch.setId(q.lastInsertId().toInt());
            return true;
        }
    }
}

bool AccessProductCostHistoryRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM productcosthistory WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM \"ProductCostHistory\" WHERE \"Id\"=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// ProductSubUnit Repository (Access)
// ════════════════════════════════════════════════════════════════════════════

static ProductSubUnit accessSubUnitFromQuery(const QSqlQuery& q) {
    ProductSubUnit su;
    su.setId(q.value("id").toInt());
    su.setProductId(q.value("productid").toInt());
    su.setName(q.value("name").toString());
    su.setBarcode(q.value("barcode").toString());
    su.setQuantityPerUnit(q.value("quantityperunit").toDouble());
    su.setCostPrice(q.value("costprice").toDouble());
    su.setSalePrice(q.value("saleprice").toDouble());
    return su;
}

static const char* kAccessSubUnitSelect =
    "SELECT id,productid,name,barcode,quantityperunit,costprice,saleprice FROM productsubunits";

QList<ProductSubUnit> AccessProductSubUnitRepository::getByProductId(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<ProductSubUnit> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE productid=%2").arg(kAccessSubUnitSelect).arg(productId)))
            while (q.next()) list.append(accessSubUnitFromQuery(q));
    } else {
        q.prepare(QString("%1 WHERE productid=?").arg(kAccessSubUnitSelect));
        q.addBindValue(productId);
        if (q.exec()) while (q.next()) list.append(accessSubUnitFromQuery(q));
    }
    return list;
}

ProductSubUnit AccessProductSubUnitRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        if (q.exec(QString("%1 WHERE id=%2").arg(kAccessSubUnitSelect).arg(id)) && q.next())
            return accessSubUnitFromQuery(q);
    } else {
        q.prepare(QString("%1 WHERE id=?").arg(kAccessSubUnitSelect));
        q.addBindValue(id);
        if (q.exec() && q.next()) return accessSubUnitFromQuery(q);
    }
    return {};
}

bool AccessProductSubUnitRepository::save(ProductSubUnit& su) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    auto escape = [](QString str) { str.replace("'", "''"); return str; };
    
    if (su.id() > 0) {
        if (isPsql) {
            QString sql = QString("UPDATE productsubunits SET productid=%1,name='%2',barcode='%3',quantityperunit=%4,costprice=%5,saleprice=%6 WHERE id=%7")
                .arg(su.productId())
                .arg(escape(su.name()))
                .arg(escape(su.barcode()))
                .arg(su.quantityPerUnit())
                .arg(su.costPrice())
                .arg(su.salePrice())
                .arg(su.id());
            return q.exec(sql);
        } else {
            q.prepare("UPDATE productsubunits SET productid=?,name=?,barcode=?,quantityperunit=?,costprice=?,saleprice=? WHERE id=?");
            q.addBindValue(su.productId());
            q.addBindValue(su.name());
            q.addBindValue(su.barcode());
            q.addBindValue(su.quantityPerUnit());
            q.addBindValue(su.costPrice());
            q.addBindValue(su.salePrice());
            q.addBindValue(su.id());
            return q.exec();
        }
    } else {
        if (isPsql) {
            QString sql = QString("INSERT INTO productsubunits (productid,name,barcode,quantityperunit,costprice,saleprice) VALUES (%1,'%2','%3',%4,%5,%6)")
                .arg(su.productId())
                .arg(escape(su.name()))
                .arg(escape(su.barcode()))
                .arg(su.quantityPerUnit())
                .arg(su.costPrice())
                .arg(su.salePrice());
            if (q.exec(sql)) {
                if (q.exec("SELECT lastval()") && q.next()) {
                    su.setId(q.value(0).toInt());
                    return true;
                }
            }
        } else {
            q.prepare("INSERT INTO productsubunits (productid,name,barcode,quantityperunit,costprice,saleprice) VALUES (?,?,?,?,?,?)");
            q.addBindValue(su.productId());
            q.addBindValue(su.name());
            q.addBindValue(su.barcode());
            q.addBindValue(su.quantityPerUnit());
            q.addBindValue(su.costPrice());
            q.addBindValue(su.salePrice());
            if (q.exec()) {
                su.setId(q.lastInsertId().toInt());
                return true;
            }
        }
    }
    return false;
}

bool AccessProductSubUnitRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    if (isPsql) {
        return q.exec(QString("DELETE FROM productsubunits WHERE id=%1").arg(id));
    } else {
        q.prepare("DELETE FROM productsubunits WHERE id=?");
        q.addBindValue(id);
        return q.exec();
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// AccessProductSupplierRepository Implementation
// ══════════════════════════════════════════════════════════════════════════════

bool AccessProductSupplierRepository::save(const ProductSupplier& ps) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    // Check if exists
    if (isPsql) {
        q.exec(QString("SELECT COUNT(*) FROM productsuppliers WHERE productid=%1 AND supplierid=%2")
               .arg(ps.productId).arg(ps.supplierId));
    } else {
        q.prepare("SELECT COUNT(*) FROM ProductSuppliers WHERE ProductId=? AND SupplierId=?");
        q.addBindValue(ps.productId);
        q.addBindValue(ps.supplierId);
        q.exec();
    }
    
    bool exists = false;
    if (q.next()) {
        exists = q.value(0).toInt() > 0;
    }
    
    if (exists) {
        if (isPsql) {
            return q.exec(QString("UPDATE productsuppliers SET purchaseprice=%1, ispreferred=%2, lastpurchasedate='%3' "
                                  "WHERE productid=%4 AND supplierid=%5")
                          .arg(ps.purchasePrice)
                          .arg(ps.isPreferred ? "true" : "false")
                          .arg(ps.lastPurchaseDate)
                          .arg(ps.productId)
                          .arg(ps.supplierId));
        } else {
            q.prepare("UPDATE ProductSuppliers SET PurchasePrice=?, IsPreferred=?, LastPurchaseDate=? "
                      "WHERE ProductId=? AND SupplierId=?");
            q.addBindValue(ps.purchasePrice);
            q.addBindValue(ps.isPreferred ? 1 : 0);
            q.addBindValue(ps.lastPurchaseDate);
            q.addBindValue(ps.productId);
            q.addBindValue(ps.supplierId);
            return q.exec();
        }
    } else {
        if (isPsql) {
            return q.exec(QString("INSERT INTO productsuppliers (productid, supplierid, purchaseprice, ispreferred, lastpurchasedate) "
                                  "VALUES (%1, %2, %3, %4, '%5')")
                          .arg(ps.productId)
                          .arg(ps.supplierId)
                          .arg(ps.purchasePrice)
                          .arg(ps.isPreferred ? "true" : "false")
                          .arg(ps.lastPurchaseDate));
        } else {
            q.prepare("INSERT INTO ProductSuppliers (ProductId, SupplierId, PurchasePrice, IsPreferred, LastPurchaseDate) "
                      "VALUES (?,?,?,?,?)");
            q.addBindValue(ps.productId);
            q.addBindValue(ps.supplierId);
            q.addBindValue(ps.purchasePrice);
            q.addBindValue(ps.isPreferred ? 1 : 0);
            q.addBindValue(ps.lastPurchaseDate);
            return q.exec();
        }
    }
    
    return false;
}

bool AccessProductSupplierRepository::remove(int productId, int supplierId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        return q.exec(QString("DELETE FROM productsuppliers WHERE productid=%1 AND supplierid=%2")
                      .arg(productId).arg(supplierId));
    } else {
        q.prepare("DELETE FROM ProductSuppliers WHERE ProductId=? AND SupplierId=?");
        q.addBindValue(productId);
        q.addBindValue(supplierId);
        return q.exec();
    }
}

std::vector<ProductSupplier> AccessProductSupplierRepository::getByProductId(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    std::vector<ProductSupplier> result;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.exec(QString("SELECT ps.productid, ps.supplierid, ps.purchaseprice, ps.ispreferred, ps.lastpurchasedate, "
                       "s.name as suppliername "
                       "FROM productsuppliers ps "
                       "LEFT JOIN suppliers s ON ps.supplierid = s.id "
                       "WHERE ps.productid=%1 "
                       "ORDER BY ps.ispreferred DESC, ps.purchaseprice ASC").arg(productId));
    } else {
        q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
                  "s.Name as SupplierName "
                  "FROM ProductSuppliers ps "
                  "LEFT JOIN Suppliers s ON ps.SupplierId = s.Id "
                  "WHERE ps.ProductId=? "
                  "ORDER BY ps.IsPreferred DESC, ps.PurchasePrice ASC");
        q.addBindValue(productId);
        q.exec();
    }
    
    while (q.next()) {
        ProductSupplier ps;
        ps.productId = q.value(0).toInt();
        ps.supplierId = q.value(1).toInt();
        ps.purchasePrice = q.value(2).toDouble();
        ps.isPreferred = isPsql ? q.value(3).toBool() : (q.value(3).toInt() == 1);
        ps.lastPurchaseDate = q.value(4).toString();
        ps.supplierName = q.value(5).toString();
        result.push_back(ps);
    }
    
    return result;
}

std::vector<ProductSupplier> AccessProductSupplierRepository::getBySupplierId(int supplierId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    std::vector<ProductSupplier> result;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.exec(QString("SELECT ps.productid, ps.supplierid, ps.purchaseprice, ps.ispreferred, ps.lastpurchasedate, "
                       "p.name as productname "
                       "FROM productsuppliers ps "
                       "LEFT JOIN products p ON ps.productid = p.id "
                       "WHERE ps.supplierid=%1 "
                       "ORDER BY p.name ASC").arg(supplierId));
    } else {
        q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
                  "p.Name as ProductName "
                  "FROM ProductSuppliers ps "
                  "LEFT JOIN Products p ON ps.ProductId = p.Id "
                  "WHERE ps.SupplierId=? "
                  "ORDER BY p.Name ASC");
        q.addBindValue(supplierId);
        q.exec();
    }
    
    while (q.next()) {
        ProductSupplier ps;
        ps.productId = q.value(0).toInt();
        ps.supplierId = q.value(1).toInt();
        ps.purchasePrice = q.value(2).toDouble();
        ps.isPreferred = isPsql ? q.value(3).toBool() : (q.value(3).toInt() == 1);
        ps.lastPurchaseDate = q.value(4).toString();
        ps.productName = q.value(5).toString();
        result.push_back(ps);
    }
    
    return result;
}

ProductSupplier AccessProductSupplierRepository::getPreferredSupplier(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    ProductSupplier ps;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.exec(QString("SELECT ps.productid, ps.supplierid, ps.purchaseprice, ps.ispreferred, ps.lastpurchasedate, "
                       "s.name as suppliername "
                       "FROM productsuppliers ps "
                       "LEFT JOIN suppliers s ON ps.supplierid = s.id "
                       "WHERE ps.productid=%1 AND ps.ispreferred=true "
                       "LIMIT 1").arg(productId));
    } else {
        q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
                  "s.Name as SupplierName "
                  "FROM ProductSuppliers ps "
                  "LEFT JOIN Suppliers s ON ps.SupplierId = s.Id "
                  "WHERE ps.ProductId=? AND ps.IsPreferred=1");
        q.addBindValue(productId);
        q.exec();
    }
    
    if (q.next()) {
        ps.productId = q.value(0).toInt();
        ps.supplierId = q.value(1).toInt();
        ps.purchasePrice = q.value(2).toDouble();
        ps.isPreferred = isPsql ? q.value(3).toBool() : (q.value(3).toInt() == 1);
        ps.lastPurchaseDate = q.value(4).toString();
        ps.supplierName = q.value(5).toString();
    }
    
    return ps;
}

bool AccessProductSupplierRepository::setPreferred(int productId, int supplierId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlDatabase database = db();
    database.transaction();
    
    QSqlQuery q(database);
    const bool isPsql = (database.driverName() == "QPSQL");
    
    // First, unset all preferred flags for this product
    if (isPsql) {
        if (!q.exec(QString("UPDATE productsuppliers SET ispreferred=false WHERE productid=%1").arg(productId))) {
            database.rollback();
            return false;
        }
    } else {
        q.prepare("UPDATE ProductSuppliers SET IsPreferred=0 WHERE ProductId=?");
        q.addBindValue(productId);
        if (!q.exec()) {
            database.rollback();
            return false;
        }
    }
    
    // Then set the preferred flag for the specified supplier
    if (isPsql) {
        if (!q.exec(QString("UPDATE productsuppliers SET ispreferred=true WHERE productid=%1 AND supplierid=%2")
                    .arg(productId).arg(supplierId))) {
            database.rollback();
            return false;
        }
    } else {
        q.prepare("UPDATE ProductSuppliers SET IsPreferred=1 WHERE ProductId=? AND SupplierId=?");
        q.addBindValue(productId);
        q.addBindValue(supplierId);
        if (!q.exec()) {
            database.rollback();
            return false;
        }
    }
    
    database.commit();
    return true;
}
// ══════════════════════════════════════════════════════════════════════════════
// AccessPromotionRepository Implementation
// ══════════════════════════════════════════════════════════════════════════════

QList<Promotion> AccessPromotionRepository::getAll() {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Promotion> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.exec("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
               "productid, categoryid, minpurchaseamount FROM promotions ORDER BY startdate DESC");
    } else {
        q.exec("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
               "ProductId, CategoryId, MinPurchaseAmount FROM Promotions ORDER BY StartDate DESC");
    }
    
    while (q.next()) {
        Promotion p;
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
        list.append(p);
    }
    
    return list;
}

QList<Promotion> AccessPromotionRepository::getActive() {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Promotion> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    QDateTime now = QDateTime::currentDateTime();
    
    if (isPsql) {
        q.prepare("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
                  "productid, categoryid, minpurchaseamount FROM promotions "
                  "WHERE status=1 AND startdate <= :now AND enddate >= :now "
                  "ORDER BY startdate DESC");
    } else {
        q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
                  "ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
                  "WHERE Status=1 AND StartDate <= ? AND EndDate >= ? "
                  "ORDER BY StartDate DESC");
    }
    
    if (isPsql) {
        q.bindValue(":now", now);
    } else {
        q.addBindValue(now);
        q.addBindValue(now);
    }
    q.exec();
    
    while (q.next()) {
        Promotion p;
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
        list.append(p);
    }
    
    return list;
}

QList<Promotion> AccessPromotionRepository::getByDateRange(const QDate& start, const QDate& end) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Promotion> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
                  "productid, categoryid, minpurchaseamount FROM promotions "
                  "WHERE startdate::date >= :start AND enddate::date <= :end "
                  "ORDER BY startdate DESC");
        q.bindValue(":start", start);
        q.bindValue(":end", end);
    } else {
        q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
                  "ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
                  "WHERE CAST(StartDate AS DATE) >= ? AND CAST(EndDate AS DATE) <= ? "
                  "ORDER BY StartDate DESC");
        q.addBindValue(start);
        q.addBindValue(end);
    }
    q.exec();
    
    while (q.next()) {
        Promotion p;
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
        list.append(p);
    }
    
    return list;
}

QList<Promotion> AccessPromotionRepository::getByProductId(int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Promotion> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
                  "productid, categoryid, minpurchaseamount FROM promotions "
                  "WHERE productid = :productId ORDER BY startdate DESC");
        q.bindValue(":productId", productId);
    } else {
        q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
                  "ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
                  "WHERE ProductId = ? ORDER BY StartDate DESC");
        q.addBindValue(productId);
    }
    q.exec();
    
    while (q.next()) {
        Promotion p;
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
        list.append(p);
    }
    
    return list;
}

QList<Promotion> AccessPromotionRepository::getByCategoryId(int categoryId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QList<Promotion> list;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
                  "productid, categoryid, minpurchaseamount FROM promotions "
                  "WHERE categoryid = :categoryId ORDER BY startdate DESC");
        q.bindValue(":categoryId", categoryId);
    } else {
        q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
                  "ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
                  "WHERE CategoryId = ? ORDER BY StartDate DESC");
        q.addBindValue(categoryId);
    }
    q.exec();
    
    while (q.next()) {
        Promotion p;
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
        list.append(p);
    }
    
    return list;
}

Promotion AccessPromotionRepository::getById(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    Promotion p;
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT id, title, description, type, discountvalue, startdate, enddate, status, imagepath, "
                  "productid, categoryid, minpurchaseamount FROM promotions WHERE id = :id");
        q.bindValue(":id", id);
    } else {
        q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, ImagePath, "
                  "ProductId, CategoryId, MinPurchaseAmount FROM Promotions WHERE Id = ?");
        q.addBindValue(id);
    }
    q.exec();
    
    if (q.next()) {
        p.id = q.value(0).toInt();
        p.title = q.value(1).toString();
        p.description = q.value(2).toString();
        p.type = static_cast<Promotion::Type>(q.value(3).toInt());
        p.discountValue = q.value(4).toDouble();
        p.startDate = q.value(5).toDateTime();
        p.endDate = q.value(6).toDateTime();
        p.status = static_cast<Promotion::Status>(q.value(7).toInt());
        p.imagePath = q.value(8).toString();
        p.productId = q.value(9).toInt();
        p.categoryId = q.value(10).toInt();
        p.minPurchaseAmount = q.value(11).toDouble();
    }
    
    return p;
}

bool AccessPromotionRepository::save(Promotion& promotion) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (promotion.id == 0) {
        // Insert
        if (isPsql) {
            q.prepare("INSERT INTO promotions (title, description, type, discountvalue, startdate, enddate, status, "
                      "imagepath, productid, categoryid, minpurchaseamount) "
                      "VALUES (:title, :desc, :type, :value, :start, :end, :status, :img, :prod, :cat, :min) "
                      "RETURNING id");
            q.bindValue(":title", promotion.title);
            q.bindValue(":desc", promotion.description);
            q.bindValue(":type", static_cast<int>(promotion.type));
            q.bindValue(":value", promotion.discountValue);
            q.bindValue(":start", promotion.startDate);
            q.bindValue(":end", promotion.endDate);
            q.bindValue(":status", static_cast<int>(promotion.status));
            q.bindValue(":img", promotion.imagePath);
            q.bindValue(":prod", promotion.productId == 0 ? QVariant() : promotion.productId);
            q.bindValue(":cat", promotion.categoryId == 0 ? QVariant() : promotion.categoryId);
            q.bindValue(":min", promotion.minPurchaseAmount);
            
            if (q.exec() && q.next()) {
                promotion.id = q.value(0).toInt();
                return true;
            }
        } else {
            q.prepare("INSERT INTO Promotions (Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
                      "ImagePath, ProductId, CategoryId, MinPurchaseAmount) "
                      "VALUES (?,?,?,?,?,?,?,?,?,?,?)");
            q.addBindValue(promotion.title);
            q.addBindValue(promotion.description);
            q.addBindValue(static_cast<int>(promotion.type));
            q.addBindValue(promotion.discountValue);
            q.addBindValue(promotion.startDate);
            q.addBindValue(promotion.endDate);
            q.addBindValue(static_cast<int>(promotion.status));
            q.addBindValue(promotion.imagePath);
            q.addBindValue(promotion.productId == 0 ? QVariant() : promotion.productId);
            q.addBindValue(promotion.categoryId == 0 ? QVariant() : promotion.categoryId);
            q.addBindValue(promotion.minPurchaseAmount);
            
            if (q.exec()) {
                promotion.id = getAccessInsertId(q, db());
                return promotion.id > 0;
            }
        }
        
        return false;
        
    } else {
        // Update
        if (isPsql) {
            q.prepare("UPDATE promotions SET title=:title, description=:desc, type=:type, discountvalue=:value, "
                      "startdate=:start, enddate=:end, status=:status, imagepath=:img, productid=:prod, "
                      "categoryid=:cat, minpurchaseamount=:min WHERE id=:id");
            q.bindValue(":title", promotion.title);
            q.bindValue(":desc", promotion.description);
            q.bindValue(":type", static_cast<int>(promotion.type));
            q.bindValue(":value", promotion.discountValue);
            q.bindValue(":start", promotion.startDate);
            q.bindValue(":end", promotion.endDate);
            q.bindValue(":status", static_cast<int>(promotion.status));
            q.bindValue(":img", promotion.imagePath);
            q.bindValue(":prod", promotion.productId == 0 ? QVariant() : promotion.productId);
            q.bindValue(":cat", promotion.categoryId == 0 ? QVariant() : promotion.categoryId);
            q.bindValue(":min", promotion.minPurchaseAmount);
            q.bindValue(":id", promotion.id);
        } else {
            q.prepare("UPDATE Promotions SET Title=?, Description=?, Type=?, DiscountValue=?, StartDate=?, EndDate=?, "
                      "Status=?, ImagePath=?, ProductId=?, CategoryId=?, MinPurchaseAmount=? WHERE Id=?");
            q.addBindValue(promotion.title);
            q.addBindValue(promotion.description);
            q.addBindValue(static_cast<int>(promotion.type));
            q.addBindValue(promotion.discountValue);
            q.addBindValue(promotion.startDate);
            q.addBindValue(promotion.endDate);
            q.addBindValue(static_cast<int>(promotion.status));
            q.addBindValue(promotion.imagePath);
            q.addBindValue(promotion.productId == 0 ? QVariant() : promotion.productId);
            q.addBindValue(promotion.categoryId == 0 ? QVariant() : promotion.categoryId);
            q.addBindValue(promotion.minPurchaseAmount);
            q.addBindValue(promotion.id);
        }
        
        if (!q.exec()) {
            return false;
        }
        
        return true;
    }
}

bool AccessPromotionRepository::remove(int id) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("DELETE FROM promotions WHERE id = :id");
        q.bindValue(":id", id);
    } else {
        q.prepare("DELETE FROM Promotions WHERE Id = ?");
        q.addBindValue(id);
    }
    
    if (!q.exec()) {
        return false;
    }
    
    return true;
}

bool AccessPromotionRepository::addProductToPromotion(int promotionId, int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        // Check if column exists in PostgreSQL
        QSqlQuery checkCol(db());
        checkCol.prepare("SELECT column_name FROM information_schema.columns "
                        "WHERE table_name='promotion_products' AND column_name='magazineprice'");
        checkCol.exec();
        
        if (!checkCol.next()) {
            // Add column if it doesn't exist
            QSqlQuery alterQuery(db());
            alterQuery.exec("ALTER TABLE promotion_products ADD COLUMN magazineprice DOUBLE PRECISION DEFAULT 0");
        }
        
        q.prepare("INSERT INTO promotion_products (promotion_id, product_id, magazineprice) VALUES (:promo, :prod, 0)");
        q.bindValue(":promo", promotionId);
        q.bindValue(":prod", productId);
    } else {
        // For Access/other databases, check if column exists
        QSqlQuery checkCol(db());
        checkCol.exec("SELECT TOP 1 * FROM PromotionProducts");
        QSqlRecord rec = checkCol.record();
        bool hasMagazinePriceCol = (rec.indexOf("MagazinePrice") != -1);
        
        if (!hasMagazinePriceCol) {
            // Add column if it doesn't exist
            QSqlQuery alterQuery(db());
            alterQuery.exec("ALTER TABLE PromotionProducts ADD COLUMN MagazinePrice DOUBLE DEFAULT 0");
        }
        
        q.prepare("INSERT INTO PromotionProducts (PromotionId, ProductId, MagazinePrice) VALUES (?, ?, 0)");
        q.addBindValue(promotionId);
        q.addBindValue(productId);
    }
    
    return q.exec();
}

bool AccessPromotionRepository::removeProductFromPromotion(int promotionId, int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("DELETE FROM promotion_products WHERE promotion_id = :promo AND product_id = :prod");
        q.bindValue(":promo", promotionId);
        q.bindValue(":prod", productId);
    } else {
        q.prepare("DELETE FROM PromotionProducts WHERE PromotionId=? AND ProductId=?");
        q.addBindValue(promotionId);
        q.addBindValue(productId);
    }
    
    return q.exec();
}

bool AccessPromotionRepository::removeAllProductsFromPromotion(int promotionId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("DELETE FROM promotion_products WHERE promotion_id = :promo");
        q.bindValue(":promo", promotionId);
    } else {
        q.prepare("DELETE FROM PromotionProducts WHERE PromotionId=?");
        q.addBindValue(promotionId);
    }
    
    return q.exec();
}

QList<int> AccessPromotionRepository::getProductsForPromotion(int promotionId) {
    QList<int> productIds;
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT product_id FROM promotion_products WHERE promotion_id = :promo");
        q.bindValue(":promo", promotionId);
    } else {
        q.prepare("SELECT ProductId FROM PromotionProducts WHERE PromotionId=?");
        q.addBindValue(promotionId);
    }
    
    if (q.exec()) {
        while (q.next()) {
            productIds.append(q.value(0).toInt());
        }
    }
    
    return productIds;
}

bool AccessPromotionRepository::setProductMagazinePrice(int promotionId, int productId, double magazinePrice) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        // PostgreSQL: Use ON CONFLICT to upsert
        q.prepare("INSERT INTO promotion_products (promotion_id, product_id, magazineprice) "
                  "VALUES (:promo, :prod, :price) "
                  "ON CONFLICT (promotion_id, product_id) DO UPDATE SET magazineprice = :price");
        q.bindValue(":promo", promotionId);
        q.bindValue(":prod", productId);
        q.bindValue(":price", magazinePrice);
    } else {
        // Access: Try UPDATE first, then INSERT if no rows affected
        q.prepare("UPDATE PromotionProducts SET MagazinePrice=? WHERE PromotionId=? AND ProductId=?");
        q.addBindValue(magazinePrice);
        q.addBindValue(promotionId);
        q.addBindValue(productId);
        
        if (!q.exec() || q.numRowsAffected() == 0) {
            // Row doesn't exist, INSERT it
            q.clear();
            q.prepare("INSERT INTO PromotionProducts (PromotionId, ProductId, MagazinePrice) VALUES (?, ?, ?)");
            q.addBindValue(promotionId);
            q.addBindValue(productId);
            q.addBindValue(magazinePrice);
            return q.exec();
        }
    }
    
    return q.exec();
}

double AccessPromotionRepository::getProductMagazinePrice(int promotionId, int productId) {
    DatabaseConnectionManager::ensureSearchPath(db());
    QSqlQuery q(db());
    const bool isPsql = (db().driverName() == "QPSQL");
    
    if (isPsql) {
        q.prepare("SELECT magazineprice FROM promotion_products WHERE promotion_id=:promo AND product_id=:prod");
        q.bindValue(":promo", promotionId);
        q.bindValue(":prod", productId);
    } else {
        q.prepare("SELECT MagazinePrice FROM PromotionProducts WHERE PromotionId=? AND ProductId=?");
        q.addBindValue(promotionId);
        q.addBindValue(productId);
    }
    
    if (q.exec() && q.next()) {
        return q.value(0).toDouble();
    }
    
    return 0.0;
}
