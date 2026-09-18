#include "sqlite_repositories.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QSet>
#include <QDebug>
#include "infra/logger.h"

QSqlDatabase BaseSQLiteRepository::db() const {
    return QSqlDatabase::database(m_connectionName);
}

// ==========================================
// SQLiteBranchRepository
// ==========================================
QList<Branch> SQLiteBranchRepository::getAll() {
    QList<Branch> list;
    QSqlQuery q("SELECT Id, Name, Address FROM Branches", db());
    while (q.next()) {
        Branch b; b.id = q.value(0).toInt(); b.name = q.value(1).toString(); b.address = q.value(2).toString();
        list.append(b);
    }
    return list;
}
Branch SQLiteBranchRepository::getById(int id) {
    Branch b; QSqlQuery q(db());
    q.prepare("SELECT Id, Name, Address FROM Branches WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) { b.id=q.value(0).toInt(); b.name=q.value(1).toString(); b.address=q.value(2).toString(); }
    return b;
}
bool SQLiteBranchRepository::save(Branch& b) {
    QSqlQuery q(db());
    if (b.id > 0) {
        q.prepare("UPDATE Branches SET Name=?,Address=? WHERE Id=?");
        q.addBindValue(b.name); q.addBindValue(b.address); q.addBindValue(b.id);
        return q.exec();
    } else {
        q.prepare("INSERT INTO Branches (Name,Address) VALUES (?,?)");
        q.addBindValue(b.name); q.addBindValue(b.address);
        if (q.exec()) { b.id=q.lastInsertId().toInt(); return true; }
    }
    return false;
}
bool SQLiteBranchRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Branches WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteCategoryRepository
// ==========================================
QList<Category> SQLiteCategoryRepository::getAll() {
    QList<Category> list;
    QSqlQuery q("SELECT Id, Name FROM Categories", db());
    while (q.next()) { Category c; c.id=q.value(0).toInt(); c.name=q.value(1).toString(); list.append(c); }
    return list;
}
Category SQLiteCategoryRepository::getById(int id) {
    Category c; QSqlQuery q(db());
    q.prepare("SELECT Id, Name FROM Categories WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) { c.id=q.value(0).toInt(); c.name=q.value(1).toString(); }
    return c;
}
bool SQLiteCategoryRepository::save(Category& c) {
    QSqlQuery q(db());
    if (c.id > 0) {
        q.prepare("UPDATE Categories SET Name=? WHERE Id=?"); q.addBindValue(c.name); q.addBindValue(c.id); return q.exec();
    } else {
        q.prepare("INSERT INTO Categories (Name) VALUES (?)"); q.addBindValue(c.name);
        if (q.exec()) { c.id=q.lastInsertId().toInt(); return true; }
    }
    return false;
}
bool SQLiteCategoryRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Categories WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteRegionRepository
// ==========================================
// Helper: ensure DistanceKm and DeliveryFee columns exist (added in Task 1)
static void ensureRegionColumns(const QSqlDatabase& dbConn) {
    QSqlRecord rec = dbConn.record("Regions");
    QSqlQuery q(dbConn);
    if (!rec.contains("DistanceKm"))
        q.exec("ALTER TABLE Regions ADD COLUMN DistanceKm REAL DEFAULT 0");
    if (!rec.contains("DeliveryFee"))
        q.exec("ALTER TABLE Regions ADD COLUMN DeliveryFee REAL DEFAULT 0");
}

QList<Region> SQLiteRegionRepository::getAll() {
    QSqlDatabase dbConn = db();
    ensureRegionColumns(dbConn);
    QList<Region> list;
    QSqlQuery q("SELECT Id, Name, COALESCE(DistanceKm,0), COALESCE(DeliveryFee,0) FROM Regions", dbConn);
    while (q.next()) {
        Region r;
        r.id          = q.value(0).toInt();
        r.name        = q.value(1).toString();
        r.distanceKm  = q.value(2).toDouble();
        r.deliveryFee = q.value(3).toDouble();
        list.append(r);
    }
    return list;
}
Region SQLiteRegionRepository::getById(int id) {
    QSqlDatabase dbConn = db();
    ensureRegionColumns(dbConn);
    Region r; QSqlQuery q(dbConn);
    q.prepare("SELECT Id, Name, COALESCE(DistanceKm,0), COALESCE(DeliveryFee,0) FROM Regions WHERE Id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        r.id          = q.value(0).toInt();
        r.name        = q.value(1).toString();
        r.distanceKm  = q.value(2).toDouble();
        r.deliveryFee = q.value(3).toDouble();
    }
    return r;
}
bool SQLiteRegionRepository::save(Region& r) {
    QSqlDatabase dbConn = db();
    ensureRegionColumns(dbConn);
    QSqlQuery q(dbConn);
    if (r.id > 0) {
        q.prepare("UPDATE Regions SET Name=?, DistanceKm=?, DeliveryFee=? WHERE Id=?");
        q.addBindValue(r.name); q.addBindValue(r.distanceKm); q.addBindValue(r.deliveryFee); q.addBindValue(r.id);
        return q.exec();
    } else {
        q.prepare("INSERT INTO Regions (Name, DistanceKm, DeliveryFee) VALUES (?,?,?)");
        q.addBindValue(r.name); q.addBindValue(r.distanceKm); q.addBindValue(r.deliveryFee);
        if (q.exec()) { r.id=q.lastInsertId().toInt(); return true; }
    }
    return false;
}
bool SQLiteRegionRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Regions WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteRoleRepository
// ==========================================
// ─────────────────────────────────────────────────────────────────────────────
// Schema migrations.
//
// WHY THIS EXISTS: kRoleSelect names every permission column explicitly. If ONE
// of them is missing from the database file the app happens to open, the whole
// SELECT fails, getById() returns a default-constructed Role, and every
// permission silently reads as false — which hides Users / Settings / Audit Log
// from the sidebar even for an admin. Adding a column to the SELECT without a
// matching migration here is what broke it. Never do one without the other.
// ─────────────────────────────────────────────────────────────────────────────
static void ensureRoleColumns(const QSqlDatabase& dbConn) {
    const QSqlRecord rec = dbConn.record("Roles");
    if (rec.isEmpty()) return;                 // table itself missing — nothing to patch
    QSqlQuery q(dbConn);

    struct ColDef { const char* name; const char* ddl; };
    static const ColDef cols[] = {
        { "CanManageUsers",     "CanManageUsers INTEGER DEFAULT 0" },
        { "CanViewReports",     "CanViewReports INTEGER DEFAULT 0" },
        { "CanAddCustomer",     "CanAddCustomer INTEGER DEFAULT 0" },
        { "CanEditCustomer",    "CanEditCustomer INTEGER DEFAULT 0" },
        { "CanDeleteCustomer",  "CanDeleteCustomer INTEGER DEFAULT 0" },
        { "CanAddProduct",      "CanAddProduct INTEGER DEFAULT 0" },
        { "CanEditProduct",     "CanEditProduct INTEGER DEFAULT 0" },
        { "CanDeleteProduct",   "CanDeleteProduct INTEGER DEFAULT 0" },
        { "CanAddOrder",        "CanAddOrder INTEGER DEFAULT 0" },
        { "CanEditOrder",       "CanEditOrder INTEGER DEFAULT 0" },
        { "CanCancelOrder",     "CanCancelOrder INTEGER DEFAULT 0" },
        { "CanDeleteOrders",    "CanDeleteOrders INTEGER DEFAULT 0" },
        { "CanManageRegions",   "CanManageRegions INTEGER DEFAULT 0" },
        { "CanAccessSettings",  "CanAccessSettings INTEGER DEFAULT 0" },
        { "CanEditOrderDate",   "CanEditOrderDate INTEGER DEFAULT 0" },
        { "CanAccessDashboard", "CanAccessDashboard INTEGER DEFAULT 1" },
        { "CanManagePurchases", "CanManagePurchases INTEGER DEFAULT 0" },
        { "CanAccessPOS",       "CanAccessPOS INTEGER DEFAULT 0" },
        { "CanManageRegister",  "CanManageRegister INTEGER DEFAULT 0" },
        { "CanManageExpenses",  "CanManageExpenses INTEGER DEFAULT 0" },
        { "CanManageInventory", "CanManageInventory INTEGER DEFAULT 0" },
        { "canDeleteFromPOS",   "canDeleteFromPOS INTEGER DEFAULT 0" },
    };

    for (const auto& c : cols) {
        if (rec.contains(QLatin1String(c.name))) continue;
        if (q.exec(QString("ALTER TABLE Roles ADD COLUMN %1").arg(QLatin1String(c.ddl)))) {
            Logger::instance().info(QString("Migration: added Roles.%1").arg(QLatin1String(c.name)));
            // A brand-new permission column would otherwise silently revoke
            // access from roles that already had the equivalent rights.
            if (qstrcmp(c.name, "canDeleteFromPOS") == 0)
                q.exec("UPDATE Roles SET canDeleteFromPOS=1 WHERE CanManageUsers=1");
            if (qstrcmp(c.name, "CanAccessDashboard") == 0)
                q.exec("UPDATE Roles SET CanAccessDashboard=1");
        } else {
            Logger::instance().error(QString("Migration failed for Roles.%1: %2")
                                     .arg(QLatin1String(c.name), q.lastError().text()));
        }
    }
}

static void ensureUserColumns(const QSqlDatabase& dbConn) {
    const QSqlRecord rec = dbConn.record("Users");
    if (rec.isEmpty()) return;
    QSqlQuery q(dbConn);

    struct ColDef { const char* name; const char* ddl; };
    static const ColDef cols[] = {
        { "Phone",               "Phone TEXT DEFAULT ''" },
        { "PhotoPath",           "PhotoPath TEXT DEFAULT ''" },
        { "FingerprintBarcode",  "FingerprintBarcode TEXT DEFAULT ''" },
        { "HourlyRate",          "HourlyRate REAL DEFAULT 0" },
        { "WeeklyOffDays",       "WeeklyOffDays INTEGER DEFAULT 0" },
    };

    for (const auto& c : cols) {
        if (rec.contains(QLatin1String(c.name))) continue;
        if (q.exec(QString("ALTER TABLE Users ADD COLUMN %1").arg(QLatin1String(c.ddl))))
            Logger::instance().info(QString("Migration: added Users.%1").arg(QLatin1String(c.name)));
        else
            Logger::instance().error(QString("Migration failed for Users.%1: %2")
                                     .arg(QLatin1String(c.name), q.lastError().text()));
    }
}

static void roleFromQuery(Role& r, const QSqlQuery& q) {
    r.id=q.value(0).toInt(); r.name=q.value(1).toString();
    r.canManageUsers=q.value(2).toBool();   r.canViewReports=q.value(3).toBool();
    r.canAddCustomer=q.value(4).toBool();   r.canEditCustomer=q.value(5).toBool();
    r.canDeleteCustomer=q.value(6).toBool();
    r.canAddProduct=q.value(7).toBool();    r.canEditProduct=q.value(8).toBool();
    r.canDeleteProduct=q.value(9).toBool();
    r.canAddOrder=q.value(10).toBool();     r.canEditOrder=q.value(11).toBool();
    r.canCancelOrder=q.value(12).toBool();  r.canDeleteOrders=q.value(13).toBool();
    r.canManageRegions=q.value(14).toBool(); r.canAccessSettings=q.value(15).toBool();
    r.canEditOrderDate=q.value(16).toBool();
    r.canAccessDashboard=q.value(17).isNull() ? true : q.value(17).toBool();
    // Phase 1 permissions
    r.canManagePurchases=q.value(18).toBool(); r.canAccessPOS=q.value(19).toBool();
    r.canManageRegister=q.value(20).toBool();  r.canManageExpenses=q.value(21).toBool();
    r.canManageInventory=q.value(22).toBool();
    r.canDeleteFromPOS=q.value(23).toBool();
    // NOTE: index 23 must stay in sync with kRoleSelect AND with ensureRoleColumns().
}
static const char* kRoleSelect =
    "SELECT Id,Name,CanManageUsers,CanViewReports,"
    "CanAddCustomer,CanEditCustomer,CanDeleteCustomer,"
    "CanAddProduct,CanEditProduct,CanDeleteProduct,"
    "CanAddOrder,CanEditOrder,CanCancelOrder,CanDeleteOrders,"
    "CanManageRegions,CanAccessSettings,"
    "CanEditOrderDate,"
    "CanAccessDashboard,"
    "CanManagePurchases,CanAccessPOS,CanManageRegister,CanManageExpenses,CanManageInventory,canDeleteFromPOS FROM Roles";

QList<Role> SQLiteRoleRepository::getAll() {
    QSqlDatabase dbConn = db();
    ensureRoleColumns(dbConn);
    QList<Role> list;
    QSqlQuery q(QString(kRoleSelect), dbConn);
    if (q.lastError().isValid()) {
        // Used to fail silently, which read as "this role has no permissions".
        Logger::instance().error("Role getAll() SQL failed: " + q.lastError().text());
        return list;
    }
    while (q.next()) { Role r; roleFromQuery(r, q); list.append(r); }
    return list;
}
Role SQLiteRoleRepository::getById(int id) {
    QSqlDatabase dbConn = db();
    ensureRoleColumns(dbConn);
    Role r; QSqlQuery q(dbConn);
    q.prepare(QString(kRoleSelect) + " WHERE Id=?"); q.addBindValue(id);
    if (!q.exec()) {
        // CRITICAL: returning a default Role here silently strips every
        // permission from the logged-in user and empties the sidebar.
        Logger::instance().error(QString("Role getById(%1) SQL failed: %2")
                                 .arg(id).arg(q.lastError().text()));
        return r;
    }
    if (q.next()) roleFromQuery(r, q);
    else Logger::instance().warn(QString("Role getById(%1): no such role").arg(id));
    return r;
}
bool SQLiteRoleRepository::save(Role& role) {
    QSqlDatabase dbConn = db();
    ensureRoleColumns(dbConn);
    QSqlQuery q(dbConn);
    auto b=[](bool v){ return v?1:0; };
    if (role.id > 0) {
        q.prepare("UPDATE Roles SET Name=?,CanManageUsers=?,CanViewReports=?,"
                  "CanAddCustomer=?,CanEditCustomer=?,CanDeleteCustomer=?,"
                  "CanAddProduct=?,CanEditProduct=?,CanDeleteProduct=?,"
                  "CanAddOrder=?,CanEditOrder=?,CanCancelOrder=?,CanDeleteOrders=?,"
                  "CanManageRegions=?,CanAccessSettings=?,CanEditOrderDate=?,CanAccessDashboard=?,"
                  "CanManagePurchases=?,CanAccessPOS=?,CanManageRegister=?,CanManageExpenses=?,CanManageInventory=?,"
                  "canDeleteFromPOS=? WHERE Id=?");
        q.addBindValue(role.name);
        q.addBindValue(b(role.canManageUsers));   q.addBindValue(b(role.canViewReports));
        q.addBindValue(b(role.canAddCustomer));   q.addBindValue(b(role.canEditCustomer));
        q.addBindValue(b(role.canDeleteCustomer));
        q.addBindValue(b(role.canAddProduct));    q.addBindValue(b(role.canEditProduct));
        q.addBindValue(b(role.canDeleteProduct));
        q.addBindValue(b(role.canAddOrder));      q.addBindValue(b(role.canEditOrder));
        q.addBindValue(b(role.canCancelOrder));   q.addBindValue(b(role.canDeleteOrders));
        q.addBindValue(b(role.canManageRegions)); q.addBindValue(b(role.canAccessSettings));
        q.addBindValue(b(role.canEditOrderDate));
        q.addBindValue(b(role.canAccessDashboard));
        // Phase 1 permissions
        q.addBindValue(b(role.canManagePurchases)); q.addBindValue(b(role.canAccessPOS));
        q.addBindValue(b(role.canManageRegister));  q.addBindValue(b(role.canManageExpenses));
        q.addBindValue(b(role.canManageInventory));
        q.addBindValue(b(role.canDeleteFromPOS));   // FIX: was read but never written
        q.addBindValue(role.id);
        bool ok = q.exec();
        if (!ok) Logger::instance().error("Role UPDATE failed: " + q.lastError().text());
        return ok;
    } else {
        q.prepare("INSERT INTO Roles (Name,CanManageUsers,CanViewReports,"
                  "CanAddCustomer,CanEditCustomer,CanDeleteCustomer,"
                  "CanAddProduct,CanEditProduct,CanDeleteProduct,"
                  "CanAddOrder,CanEditOrder,CanCancelOrder,CanDeleteOrders,"
                  "CanManageRegions,CanAccessSettings,CanEditOrderDate,CanAccessDashboard,"
                  "CanManagePurchases,CanAccessPOS,CanManageRegister,CanManageExpenses,CanManageInventory,"
                  "canDeleteFromPOS) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(role.name);
        q.addBindValue(b(role.canManageUsers));   q.addBindValue(b(role.canViewReports));
        q.addBindValue(b(role.canAddCustomer));   q.addBindValue(b(role.canEditCustomer));
        q.addBindValue(b(role.canDeleteCustomer));
        q.addBindValue(b(role.canAddProduct));    q.addBindValue(b(role.canEditProduct));
        q.addBindValue(b(role.canDeleteProduct));
        q.addBindValue(b(role.canAddOrder));      q.addBindValue(b(role.canEditOrder));
        q.addBindValue(b(role.canCancelOrder));   q.addBindValue(b(role.canDeleteOrders));
        q.addBindValue(b(role.canManageRegions)); q.addBindValue(b(role.canAccessSettings));
        q.addBindValue(b(role.canEditOrderDate));
        q.addBindValue(b(role.canAccessDashboard));
        // Phase 1 permissions
        q.addBindValue(b(role.canManagePurchases)); q.addBindValue(b(role.canAccessPOS));
        q.addBindValue(b(role.canManageRegister));  q.addBindValue(b(role.canManageExpenses));
        q.addBindValue(b(role.canManageInventory));
        q.addBindValue(b(role.canDeleteFromPOS));   // FIX: was read but never written
        if (q.exec()) { role.id=q.lastInsertId().toInt(); return true; }
        Logger::instance().error("Role INSERT failed: " + q.lastError().text());
    }
    return false;
}
bool SQLiteRoleRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Roles WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteCustomerRepository
// ==========================================
void SQLiteCustomerRepository::loadPhonesAndAddresses(Customer& c) {
    QSqlQuery q(db());
    q.prepare("SELECT PhoneNumber FROM CustomerPhones WHERE CustomerId=?");
    q.addBindValue(c.id());
    if (q.exec()) { QList<Phone> ph; while(q.next()) ph.append(Phone{q.value(0).toString()}); c.setPhones(ph); }
    q.prepare("SELECT AddressText FROM CustomerAddresses WHERE CustomerId=?");
    q.addBindValue(c.id());
    if (q.exec()) { QList<Address> ad; while(q.next()) ad.append(Address{q.value(0).toString()}); c.setAddresses(ad); }
}
void SQLiteCustomerRepository::loadFavorites(Customer& c) {
    QSqlQuery q(db());
    q.prepare("SELECT ProductId FROM CustomerFavorites WHERE CustomerId=?");
    q.addBindValue(c.id());
    if (q.exec()) { QList<int> favs; while(q.next()) favs.append(q.value(0).toInt()); c.setFavoriteProductIds(favs); }
}
bool SQLiteCustomerRepository::savePhonesAndAddresses(const Customer& c) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM CustomerPhones WHERE CustomerId=?"); q.addBindValue(c.id()); if (!q.exec()) return false;
    q.prepare("DELETE FROM CustomerAddresses WHERE CustomerId=?"); q.addBindValue(c.id()); if (!q.exec()) return false;
    for (const auto& ph : c.phones()) {
        q.prepare("INSERT INTO CustomerPhones (CustomerId,PhoneNumber) VALUES (?,?)");
        q.addBindValue(c.id()); q.addBindValue(ph.number); if (!q.exec()) return false;
    }
    for (const auto& ad : c.addresses()) {
        q.prepare("INSERT INTO CustomerAddresses (CustomerId,AddressText) VALUES (?,?)");
        q.addBindValue(c.id()); q.addBindValue(ad.text); if (!q.exec()) return false;
    }
    return true;
}
bool SQLiteCustomerRepository::saveFavorites(const Customer& c) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM CustomerFavorites WHERE CustomerId=?"); q.addBindValue(c.id()); if (!q.exec()) return false;
    for (int pid : c.favoriteProductIds()) {
        q.prepare("INSERT INTO CustomerFavorites (CustomerId,ProductId) VALUES (?,?)");
        q.addBindValue(c.id()); q.addBindValue(pid); if (!q.exec()) return false;
    }
    return true;
}
static Customer customerFromQuery(const QSqlQuery& q) {
    Customer c(q.value(0).toInt(), q.value(1).toString());
    c.setDistanceKm(q.value(2).toDouble()); c.setRegionId(q.value(3).toInt());
    c.setNotes(q.value(4).toString());
    c.setFirstContactDate(QDateTime::fromString(q.value(5).toString(), Qt::ISODate));
    c.setLastOrderDate(QDateTime::fromString(q.value(6).toString(), Qt::ISODate));
    c.setStatus(Customer::stringToStatus(q.value(7).toString()));
    c.setPreferredPaymentMethod(q.value(8).toString());
    c.setPreferredPaymentOther(q.value(9).toString());
    c.setDebt(q.value(10).toDouble());
    return c;
}
static const char* kCustSelect =
    "SELECT Id,Name,DistanceKm,RegionId,Notes,FirstContactDate,LastOrderDate,Status,"
    "PreferredPaymentMethod,PreferredPaymentOther,Debt FROM Customers";
QList<Customer> SQLiteCustomerRepository::getAll() {
    QList<Customer> list;
    QSqlQuery q(kCustSelect, db());
    while (q.next()) { auto c = customerFromQuery(q); loadPhonesAndAddresses(c); loadFavorites(c); list.append(c); }
    return list;
}
Customer SQLiteCustomerRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString(kCustSelect) + " WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) { auto c = customerFromQuery(q); loadPhonesAndAddresses(c); loadFavorites(c); return c; }
    return {};
}

QList<Customer> SQLiteCustomerRepository::getPage(int offset, int limit) {
    QList<Customer> list;
    QSqlQuery q(db());
    q.prepare(QString(kCustSelect) + " LIMIT ? OFFSET ?");
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (q.exec()) {
        while (q.next()) {
            auto c = customerFromQuery(q);
            loadPhonesAndAddresses(c);
            loadFavorites(c);
            list.append(c);
        }
    }
    return list;
}

int SQLiteCustomerRepository::getTotalCount() {
    QSqlQuery q("SELECT COUNT(*) FROM Customers", db());
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool SQLiteCustomerRepository::save(Customer& c) {
    QSqlDatabase dbConn = db(); QSqlQuery q(dbConn); bool ok = false;
    dbConn.transaction();
    if (c.id() > 0) {
        q.prepare("UPDATE Customers SET Name=?,DistanceKm=?,RegionId=?,Notes=?,FirstContactDate=?,LastOrderDate=?,Status=?,"
                  "PreferredPaymentMethod=?,PreferredPaymentOther=?,Debt=? WHERE Id=?");
        q.addBindValue(c.name()); q.addBindValue(c.distanceKm()); q.addBindValue(c.regionId()); q.addBindValue(c.notes());
        q.addBindValue(c.firstContactDate().toString(Qt::ISODate)); q.addBindValue(c.lastOrderDate().toString(Qt::ISODate));
        q.addBindValue(Customer::statusToString(c.status()));
        q.addBindValue(c.preferredPaymentMethod()); q.addBindValue(c.preferredPaymentOther());
        q.addBindValue(c.debt());
        q.addBindValue(c.id()); ok = q.exec();
    } else {
        q.prepare("INSERT INTO Customers (Name,DistanceKm,RegionId,Notes,FirstContactDate,LastOrderDate,Status,"
                  "PreferredPaymentMethod,PreferredPaymentOther,Debt) VALUES (?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(c.name()); q.addBindValue(c.distanceKm()); q.addBindValue(c.regionId()); q.addBindValue(c.notes());
        q.addBindValue(c.firstContactDate().toString(Qt::ISODate)); q.addBindValue(c.lastOrderDate().toString(Qt::ISODate));
        q.addBindValue(Customer::statusToString(c.status()));
        q.addBindValue(c.preferredPaymentMethod()); q.addBindValue(c.preferredPaymentOther());
        q.addBindValue(c.debt());
        if (q.exec()) { c.setId(q.lastInsertId().toInt()); ok = true; }
    }
    if (ok) ok = savePhonesAndAddresses(c) && saveFavorites(c);
    ok ? dbConn.commit() : dbConn.rollback();
    return ok;
}
bool SQLiteCustomerRepository::remove(int id) {
    QSqlDatabase dbConn = db(); QSqlQuery q(dbConn); dbConn.transaction();
    q.prepare("DELETE FROM CustomerPhones WHERE CustomerId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM CustomerAddresses WHERE CustomerId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM CustomerFavorites WHERE CustomerId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM Customers WHERE Id=?"); q.addBindValue(id); bool ok = q.exec();
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}
QList<Customer> SQLiteCustomerRepository::search(const QString& kw) {
    QList<Customer> list; QSqlQuery q(db());
    q.prepare("SELECT DISTINCT c.Id,c.Name,c.DistanceKm,c.RegionId,c.Notes,c.FirstContactDate,c.LastOrderDate,c.Status,"
              "c.PreferredPaymentMethod,c.PreferredPaymentOther "
              "FROM Customers c LEFT JOIN CustomerPhones cp ON c.Id=cp.CustomerId "
              "LEFT JOIN CustomerAddresses ca ON c.Id=ca.CustomerId LEFT JOIN Regions r ON c.RegionId=r.Id "
              "WHERE c.Name LIKE ? OR cp.PhoneNumber LIKE ? OR ca.AddressText LIKE ? OR r.Name LIKE ?");
    QString v="%"+kw+"%"; q.addBindValue(v); q.addBindValue(v); q.addBindValue(v); q.addBindValue(v);
    if (q.exec()) while (q.next()) { auto c=customerFromQuery(q); loadPhonesAndAddresses(c); loadFavorites(c); list.append(c); }
    return list;
}

// ==========================================
// ==========================================
// SQLiteProductRepository
// ==========================================
static Product productFromQuery(const QSqlQuery& q) {
    Product p(q.value(0).toInt(), q.value(1).toString(), q.value(3).toDouble(),
              q.value(4).toInt(), Product::stringToStatus(q.value(5).toString()));
    p.setBarcode(q.value(2).toString());
    p.setManufactureDate(QDate::fromString(q.value(6).toString(), Qt::ISODate));
    p.setExpiryDate(QDate::fromString(q.value(7).toString(), Qt::ISODate));
    p.setStockQty(q.value(8).toInt());
    p.setLowStockThreshold(q.value(9).toInt());
    p.setCostPrice(q.value(10).toDouble());
    p.setUnitLabel(q.value(11).toString());
    p.setImagePath(q.value(12).toString());
    p.setHasTax(q.value(13).toInt() != 0);
    p.setType(Product::stringToType(q.value(14).toString()));
    p.setPluBarcode(q.value(15).toString());
    return p;
}
static const char* kProdSel =
    "SELECT Id,Name,Barcode,Price,CategoryId,Status,ManufactureDate,ExpiryDate,StockQty,LowStockThreshold,CostPrice,UnitLabel,ImagePath,HasTax,Type,PluBarcode FROM Products";

QList<Product> SQLiteProductRepository::getAll() {
    QList<Product> list;
    QSqlQuery q(kProdSel, db());
    while (q.next()) {
        Product mainProduct = productFromQuery(q);
        list.append(mainProduct);
        
        // Add sub-units as virtual products
        QSqlQuery subQ(db());
        subQ.prepare("SELECT Id,ProductId,Name,Barcode,QuantityPerUnit,CostPrice,SalePrice FROM ProductSubUnits WHERE ProductId=?");
        subQ.addBindValue(mainProduct.id());
        if (subQ.exec()) {
            while (subQ.next()) {
                Product subProduct = mainProduct;  // Copy main product
                int subUnitId = subQ.value(0).toInt();
                QString subUnitName = subQ.value(2).toString();
                QString subUnitBarcode = subQ.value(3).toString();
                double qtyPerUnit = subQ.value(4).toDouble();
                double subCostPrice = subQ.value(5).toDouble();
                double subSalePrice = subQ.value(6).toDouble();
                
                // Modify for sub-unit display
                subProduct.setId(-subUnitId);  // Negative ID to distinguish sub-units
                subProduct.setName(QString("%1*%2 (%3)").arg(mainProduct.name(), subUnitName).arg(qtyPerUnit, 0, 'f', 0));
                subProduct.setBarcode(subUnitBarcode);
                subProduct.setPrice(subSalePrice);
                subProduct.setCostPrice(subCostPrice);
                // Stock is divided by qty per unit (approximate)
                subProduct.setStockQty(static_cast<int>(mainProduct.stockQty() / qtyPerUnit));
                
                list.append(subProduct);
            }
        }
    }
    return list;
}
Product SQLiteProductRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString(kProdSel)+" WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) return productFromQuery(q);
    return {};
}

QList<Product> SQLiteProductRepository::getPage(int offset, int limit) {
    QList<Product> list;
    QSqlQuery q(db());
    q.prepare(QString(kProdSel) + " LIMIT ? OFFSET ?");
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (q.exec()) {
        while (q.next()) list.append(productFromQuery(q));
    }
    return list;
}

int SQLiteProductRepository::getTotalCount() {
    QSqlQuery q("SELECT COUNT(*) FROM Products", db());
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool SQLiteProductRepository::save(Product& p) {
    QSqlDatabase dbConn = db(); bool isNew = p.id()==0; double oldPrice = 0;
    if (!isNew) { Product op = getById(p.id()); oldPrice = op.price(); }
    dbConn.transaction(); QSqlQuery q(dbConn); bool ok = false;
    auto d2s = [](const QDate& d) -> QVariant { return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant(); };
    if (p.id() > 0) {
        q.prepare("UPDATE Products SET Name=?,Barcode=?,Price=?,CategoryId=?,Status=?,ManufactureDate=?,ExpiryDate=?,StockQty=?,LowStockThreshold=?,CostPrice=?,UnitLabel=?,ImagePath=?,HasTax=?,Type=?,PluBarcode=? WHERE Id=?");
        q.addBindValue(p.name()); q.addBindValue(p.barcode()); q.addBindValue(p.price());
        q.addBindValue(p.categoryId()); q.addBindValue(Product::statusToString(p.status()));
        q.addBindValue(d2s(p.manufactureDate())); q.addBindValue(d2s(p.expiryDate()));
        q.addBindValue(p.stockQty()); q.addBindValue(p.lowStockThreshold());
        q.addBindValue(p.costPrice()); q.addBindValue(p.unitLabel()); q.addBindValue(p.imagePath());
        q.addBindValue(p.hasTax() ? 1 : 0);
        q.addBindValue(Product::typeToString(p.type())); q.addBindValue(p.pluBarcode());
        q.addBindValue(p.id()); ok = q.exec();
    } else {
        q.prepare("INSERT INTO Products (Name,Barcode,Price,CategoryId,Status,ManufactureDate,ExpiryDate,StockQty,LowStockThreshold,CostPrice,UnitLabel,ImagePath,HasTax,Type,PluBarcode) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(p.name()); q.addBindValue(p.barcode()); q.addBindValue(p.price());
        q.addBindValue(p.categoryId()); q.addBindValue(Product::statusToString(p.status()));
        q.addBindValue(d2s(p.manufactureDate())); q.addBindValue(d2s(p.expiryDate()));
        q.addBindValue(p.stockQty()); q.addBindValue(p.lowStockThreshold());
        q.addBindValue(p.costPrice()); q.addBindValue(p.unitLabel()); q.addBindValue(p.imagePath());
        q.addBindValue(p.hasTax() ? 1 : 0);
        q.addBindValue(Product::typeToString(p.type())); q.addBindValue(p.pluBarcode());
        if (q.exec()) { p.setId(q.lastInsertId().toInt()); ok = true; }
    }
    if (ok && (isNew || qAbs(p.price()-oldPrice)>0.001))
        ok = addPriceHistoryEntry(p.id(), p.price(), QDateTime::currentDateTime());
    if (ok) dbConn.commit(); else { dbConn.rollback(); Logger::instance().error("Product save: "+q.lastError().text()); }
    return ok;
}
bool SQLiteProductRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Products WHERE Id=?"); q.addBindValue(id); return q.exec();
}
QList<Product> SQLiteProductRepository::search(const QString& kw) {
    QList<Product> list;
    QString v = "%" + kw + "%";
    
    // Search main products
    QSqlQuery q(db());
    q.prepare(QString(kProdSel) + " WHERE Name LIKE ? OR Barcode LIKE ?");
    q.addBindValue(v);
    q.addBindValue(v);
    
    QSet<int> addedSubUnits;  // Track added sub-units to avoid duplicates
    
    if (q.exec()) {
        while (q.next()) {
            Product mainProduct = productFromQuery(q);
            list.append(mainProduct);
            
            // Add ALL sub-units of matched main products
            QSqlQuery subQ(db());
            subQ.prepare("SELECT Id,Name,Barcode,QuantityPerUnit,CostPrice,SalePrice FROM ProductSubUnits WHERE ProductId=?");
            subQ.addBindValue(mainProduct.id());
            if (subQ.exec()) {
                while (subQ.next()) {
                    int subUnitId = subQ.value(0).toInt();
                    Product subProduct = mainProduct;
                    subProduct.setId(-subUnitId);
                    subProduct.setName(QString("%1*%2 (%3)").arg(mainProduct.name(), subQ.value(1).toString()).arg(subQ.value(3).toDouble(), 0, 'f', 0));
                    subProduct.setBarcode(subQ.value(2).toString());
                    subProduct.setPrice(subQ.value(5).toDouble());
                    subProduct.setCostPrice(subQ.value(4).toDouble());
                    subProduct.setStockQty(static_cast<int>(mainProduct.stockQty() / subQ.value(3).toDouble()));
                    list.append(subProduct);
                    addedSubUnits.insert(subUnitId);
                }
            }
        }
    }
    
    // Search sub-units directly (by barcode or name)
    QSqlQuery subDirectQ(db());
    subDirectQ.prepare("SELECT ps.Id,ps.ProductId,ps.Name,ps.Barcode,ps.QuantityPerUnit,ps.CostPrice,ps.SalePrice "
                       "FROM ProductSubUnits ps WHERE ps.Name LIKE ? OR ps.Barcode LIKE ?");
    subDirectQ.addBindValue(v);
    subDirectQ.addBindValue(v);
    
    if (subDirectQ.exec()) {
        while (subDirectQ.next()) {
            int subUnitId = subDirectQ.value(0).toInt();
            if (addedSubUnits.contains(subUnitId)) continue;  // Already added
            
            int mainProductId = subDirectQ.value(1).toInt();
            Product mainProduct = getById(mainProductId);
            if (mainProduct.id() == 0) continue;
            
            Product subProduct = mainProduct;
            subProduct.setId(-subUnitId);
            subProduct.setName(QString("%1*%2 (%3)").arg(mainProduct.name(), subDirectQ.value(2).toString()).arg(subDirectQ.value(4).toDouble(), 0, 'f', 0));
            subProduct.setBarcode(subDirectQ.value(3).toString());
            subProduct.setPrice(subDirectQ.value(6).toDouble());
            subProduct.setCostPrice(subDirectQ.value(5).toDouble());
            subProduct.setStockQty(static_cast<int>(mainProduct.stockQty() / subDirectQ.value(4).toDouble()));
            list.append(subProduct);
            addedSubUnits.insert(subUnitId);
        }
    }
    
    return list;
}
QList<ProductPriceHistory> SQLiteProductRepository::getPriceHistory(int pid) {
    QList<ProductPriceHistory> list; QSqlQuery q(db());
    q.prepare("SELECT ProductId,Price,EffectiveDate FROM ProductPriceHistory WHERE ProductId=? ORDER BY Id DESC");
    q.addBindValue(pid);
    if (q.exec()) while (q.next()) {
        ProductPriceHistory h; h.productId=q.value(0).toInt(); h.price=q.value(1).toDouble();
        h.effectiveDate=QDateTime::fromString(q.value(2).toString(),Qt::ISODate); list.append(h);
    }
    return list;
}
bool SQLiteProductRepository::addPriceHistoryEntry(int pid, double price, const QDateTime& dt) {
    QSqlQuery q(db());
    q.prepare("INSERT INTO ProductPriceHistory (ProductId,Price,EffectiveDate) VALUES (?,?,?)");
    q.addBindValue(pid); q.addBindValue(price); q.addBindValue(dt.toString(Qt::ISODate));
    return q.exec();
}

void SQLiteProductRepository::checkAndUpdateLowStockFlag(const Product& p,
                                                          IMissingProductRepository* missingRepo) {
    if (!missingRepo || p.id() <= 0) return;

    bool isLow = p.stockQty() <= p.lowStockThreshold() && p.lowStockThreshold() > 0;
    MissingProduct existing = missingRepo->getActiveAutoFlagByProductId(p.id());
    bool hasFlag = (existing.id > 0);

    if (isLow && !hasFlag) {
        // Create a new auto-flag entry
        MissingProduct mp;
        mp.productId      = p.id();
        mp.productName    = p.name();
        mp.barcode        = p.barcode();
        mp.quantityNeeded = p.lowStockThreshold(); // reorder target = safety margin
        mp.dateAdded      = QDate::currentDate();
        mp.purchased      = false;
        mp.source         = "auto_low_stock";
        mp.currentQty     = p.stockQty();
        missingRepo->save(mp);
        Logger::instance().info(QString("Auto low-stock flag created for product '%1' (qty=%2, threshold=%3)")
            .arg(p.name()).arg(p.stockQty()).arg(p.lowStockThreshold()));
    } else if (isLow && hasFlag) {
        // Update existing flag with latest stock level
        existing.currentQty     = p.stockQty();
        existing.quantityNeeded = p.lowStockThreshold();
        existing.productName    = p.name();
        existing.barcode        = p.barcode();
        missingRepo->save(existing);
    } else if (!isLow && hasFlag) {
        // Quantity is back above margin — auto-remove the flag
        missingRepo->remove(existing.id);
        Logger::instance().info(QString("Auto low-stock flag removed for product '%1' (qty=%2 now above threshold=%3)")
            .arg(p.name()).arg(p.stockQty()).arg(p.lowStockThreshold()));
    }
}

// ==========================================
// SQLiteOrderRepository
// ==========================================
void SQLiteOrderRepository::loadItems(Order& o) {
    QSqlQuery q(db());
    // Join Products to get name and barcode for invoice display
    q.prepare(
        "SELECT oi.ProductId, oi.Quantity, oi.UnitPrice, "
        "       COALESCE(p.Name,''), COALESCE(p.Barcode,'') "
        "FROM OrderItems oi "
        "LEFT JOIN Products p ON oi.ProductId = p.Id "
        "WHERE oi.OrderId = ?");
    q.addBindValue(o.id());
    if (q.exec()) {
        QList<OrderItem> items;
        while (q.next()) {
            OrderItem item;
            item.productId      = q.value(0).toInt();
            item.quantity       = q.value(1).toInt();
            item.unitPrice      = q.value(2).toDouble();
            item.productName    = q.value(3).toString();
            item.productBarcode = q.value(4).toString();
            items.append(item);
        }
        o.setItems(items);
    }
}
bool SQLiteOrderRepository::saveItems(const Order& o) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM OrderItems WHERE OrderId=?"); q.addBindValue(o.id()); if (!q.exec()) return false;
    for (const auto& item : o.items()) {
        q.prepare("INSERT INTO OrderItems (OrderId,ProductId,Quantity,UnitPrice) VALUES (?,?,?,?)");
        q.addBindValue(o.id()); q.addBindValue(item.productId);
        q.addBindValue(item.quantity); q.addBindValue(item.unitPrice);
        if (!q.exec()) return false;
    }
    return true;
}
// Safe order query — works even if DeliveryDrivers or new columns don't exist yet
static QString buildOrderQuery(const QSqlDatabase& dbConn) {
    // Check which optional columns exist
    QSqlRecord rec = dbConn.record("Orders");
    bool hasPayOther  = rec.contains("PaymentOtherDetail");
    bool hasInvNum    = rec.contains("InvoiceNumber");
    bool hasInvBarcode = rec.contains("InvoiceBarcode");
    bool hasRegSession = rec.contains("RegisterSessionId");
    bool hasDrivers   = dbConn.tables().contains("DeliveryDrivers", Qt::CaseInsensitive);

    QString driverPhone = hasDrivers  ? "COALESCE(d.Phone,'')" : "''";
    QString payOther    = hasPayOther ? "COALESCE(o.PaymentOtherDetail,'')" : "''";
    QString invNum      = hasInvNum   ? "COALESCE(o.InvoiceNumber,0)" : "0";
    QString invBarcode  = hasInvBarcode ? "COALESCE(o.InvoiceBarcode,'')" : "''";
    QString regSession  = hasRegSession ? "COALESCE(o.RegisterSessionId,0)" : "0";
    QString join        = hasDrivers  ? " LEFT JOIN DeliveryDrivers d ON o.DriverId=d.Id" : "";

    return QString(
        "SELECT o.Id,o.CustomerId,%1,o.DateTime,o.DeliveryFee,o.Status,o.CancelReason,"
        "o.DriverId,o.DriverName,o.DiscountAmount,o.DiscountReason,o.PaymentMethod,"
        "%2,%3,%4,%5 "
        "FROM Orders o%6")
        .arg(regSession, driverPhone, payOther, invNum, invBarcode, join);
}

static void orderFromQuery(Order& o, const QSqlQuery& q) {
    o.setId(q.value(0).toInt()); 
    o.setCustomerId(q.value(1).toInt());
    o.setRegisterSessionId(q.value(2).toInt());
    o.setDateTime(QDateTime::fromString(q.value(3).toString(),Qt::ISODate));
    o.setDeliveryFee(q.value(4).toDouble());
    o.setStatus(q.value(5).toString().isEmpty() ? "Pending" : q.value(5).toString());
    o.setCancelReason(q.value(6).toString());
    o.setDriverId(q.value(7).toInt()); 
    o.setDriverName(q.value(8).toString());
    o.setDiscountAmount(q.value(9).toDouble()); 
    o.setDiscountReason(q.value(10).toString());
    o.setPaymentMethod(q.value(11).toString().isEmpty() ? "Cash" : q.value(11).toString());
    o.setDriverPhone(q.value(12).toString());
    o.setPaymentOtherDetail(q.value(13).toString());
    o.setInvoiceNumber(q.value(14).toInt());
    o.setInvoiceBarcode(q.value(15).toString());
}

QList<Order> SQLiteOrderRepository::getAll() {
    QList<Order> list;
    QSqlDatabase dbConn = db();
    QString sel = buildOrderQuery(dbConn);
    QSqlQuery q(dbConn);
    if (!q.exec(sel + " ORDER BY o.DateTime DESC")) {
        Logger::instance().error("Order getAll() SQL failed: " + q.lastError().text()
                                 + " | Query: " + sel);
        return list;
    }
    while (q.next()) { Order o; orderFromQuery(o,q); loadItems(o); list.append(o); }
    Logger::instance().info(QString("Order getAll() returned %1 rows").arg(list.size()));
    return list;
}
Order SQLiteOrderRepository::getById(int id) {
    Order o;
    QSqlDatabase dbConn = db();
    QString sel = buildOrderQuery(dbConn);
    QSqlQuery q(dbConn);
    q.prepare(sel + " WHERE o.Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) { orderFromQuery(o,q); loadItems(o); }
    return o;
}

QList<Order> SQLiteOrderRepository::getPage(int offset, int limit) {
    QList<Order> list;
    QSqlDatabase dbConn = db();
    QString sel = buildOrderQuery(dbConn);
    QSqlQuery q(dbConn);
    q.prepare(sel + " LIMIT ? OFFSET ?");
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (q.exec()) {
        while (q.next()) {
            Order o;
            orderFromQuery(o, q);
            loadItems(o);
            list.append(o);
        }
    }
    return list;
}

int SQLiteOrderRepository::getTotalCount() {
    QSqlQuery q("SELECT COUNT(*) FROM Orders", db());
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool SQLiteOrderRepository::save(Order& o) {
    QSqlDatabase dbConn = db(); dbConn.transaction(); QSqlQuery q(dbConn); bool ok = false;
    if (o.id() > 0) {
        q.prepare("UPDATE Orders SET CustomerId=?,RegisterSessionId=?,DateTime=?,DeliveryFee=?,Subtotal=?,GrandTotal=?,"
                  "Status=?,CancelReason=?,DriverId=?,DriverName=?,DiscountAmount=?,DiscountReason=?,"
                  "PaymentMethod=?,PaymentOtherDetail=?,InvoiceNumber=?,InvoiceBarcode=? WHERE Id=?");
        q.addBindValue(o.customerId()); q.addBindValue(o.registerSessionId()); q.addBindValue(o.dateTime().toString(Qt::ISODate));
        q.addBindValue(o.deliveryFee()); q.addBindValue(o.subtotal()); q.addBindValue(o.grandTotal());
        q.addBindValue(o.status()); q.addBindValue(o.cancelReason());
        q.addBindValue(o.driverId()); q.addBindValue(o.driverName());
        q.addBindValue(o.discountAmount()); q.addBindValue(o.discountReason());
        q.addBindValue(o.paymentMethod()); q.addBindValue(o.paymentOtherDetail());
        q.addBindValue(o.invoiceNumber());
        q.addBindValue(o.invoiceBarcode());
        q.addBindValue(o.id()); ok = q.exec();
    } else {
        // Auto-assign InvoiceNumber = DB row Id (insert first, then set InvoiceNumber = Id)
        q.prepare("INSERT INTO Orders (CustomerId,RegisterSessionId,DateTime,DeliveryFee,Subtotal,GrandTotal,"
                  "Status,CancelReason,DriverId,DriverName,DiscountAmount,DiscountReason,"
                  "PaymentMethod,PaymentOtherDetail,InvoiceNumber,InvoiceBarcode) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(o.customerId()); q.addBindValue(o.registerSessionId()); q.addBindValue(o.dateTime().toString(Qt::ISODate));
        q.addBindValue(o.deliveryFee()); q.addBindValue(o.subtotal()); q.addBindValue(o.grandTotal());
        q.addBindValue(o.status()); q.addBindValue(o.cancelReason());
        q.addBindValue(o.driverId()); q.addBindValue(o.driverName());
        q.addBindValue(o.discountAmount()); q.addBindValue(o.discountReason());
        q.addBindValue(o.paymentMethod()); q.addBindValue(o.paymentOtherDetail());
        q.addBindValue(0);  // placeholder — will be updated to Id immediately below
        q.addBindValue(o.invoiceBarcode());
        if (q.exec()) {
            o.setId(q.lastInsertId().toInt());
            // Set InvoiceNumber = Id so both are always identical and searchable
            o.setInvoiceNumber(o.id());
            QSqlQuery upd(dbConn);
            upd.prepare("UPDATE Orders SET InvoiceNumber=? WHERE Id=?");
            upd.addBindValue(o.id()); upd.addBindValue(o.id());
            upd.exec();
            ok = true;
        }
    }
    if (ok) ok = saveItems(o);
    if (ok) dbConn.commit(); else { dbConn.rollback(); Logger::instance().error("Order save: "+q.lastError().text()); }
    return ok;
}
bool SQLiteOrderRepository::remove(int id) {
    QSqlDatabase dbConn = db(); dbConn.transaction(); QSqlQuery q(dbConn);
    q.prepare("DELETE FROM OrderItems WHERE OrderId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM Orders WHERE Id=?"); q.addBindValue(id); bool ok = q.exec();
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}
QList<Order> SQLiteOrderRepository::getOrdersByCustomerId(int cid) {
    QList<Order> list;
    QSqlDatabase dbConn = db();
    QString sel = buildOrderQuery(dbConn);
    QSqlQuery q(dbConn);
    q.prepare(sel + " WHERE o.CustomerId=? ORDER BY o.DateTime DESC");
    q.addBindValue(cid);
    if (q.exec()) while (q.next()) { Order o; orderFromQuery(o,q); loadItems(o); list.append(o); }
    return list;
}
QList<Order> SQLiteOrderRepository::getOrdersDateRange(const QDateTime& s, const QDateTime& e) {
    QList<Order> list;
    QSqlDatabase dbConn = db();
    QString sel = buildOrderQuery(dbConn);
    QSqlQuery q(dbConn);
    q.prepare(sel + " WHERE o.DateTime BETWEEN ? AND ? ORDER BY o.DateTime ASC");
    q.addBindValue(s.toString(Qt::ISODate)); q.addBindValue(e.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) { Order o; orderFromQuery(o,q); loadItems(o); list.append(o); }
    return list;
}

// ==========================================
// SQLiteScheduledOrderRepository
// ==========================================
// Helper: ensure Task 5 columns exist
static void ensureScheduledOrderColumns(const QSqlDatabase& dbConn) {
    QSqlRecord rec = dbConn.record("ScheduledOrders");
    QSqlQuery q(dbConn);
    if (!rec.contains("IsRecurring"))
        q.exec("ALTER TABLE ScheduledOrders ADD COLUMN IsRecurring INTEGER DEFAULT 1");
    if (!rec.contains("OneTimeDate"))
        q.exec("ALTER TABLE ScheduledOrders ADD COLUMN OneTimeDate TEXT DEFAULT NULL");
}

void SQLiteScheduledOrderRepository::loadItems(ScheduledOrder& so) {
    QSqlQuery q(db());
    q.prepare("SELECT ProductId,Quantity FROM ScheduledOrderItems WHERE ScheduledOrderId=?");
    q.addBindValue(so.id);
    if (q.exec()) {
        QList<OrderItem> items;
        while (q.next()) {
            OrderItem i; i.productId=q.value(0).toInt(); i.quantity=q.value(1).toInt(); items.append(i);
        }
        so.items = items;
    }
}
bool SQLiteScheduledOrderRepository::saveItems(const ScheduledOrder& so) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM ScheduledOrderItems WHERE ScheduledOrderId=?"); q.addBindValue(so.id); if (!q.exec()) return false;
    for (const auto& item : so.items) {
        q.prepare("INSERT INTO ScheduledOrderItems (ScheduledOrderId,ProductId,Quantity) VALUES (?,?,?)");
        q.addBindValue(so.id); q.addBindValue(item.productId); q.addBindValue(item.quantity);
        if (!q.exec()) return false;
    }
    return true;
}
QList<ScheduledOrder> SQLiteScheduledOrderRepository::getAll() {
    QSqlDatabase dbConn = db();
    ensureScheduledOrderColumns(dbConn);
    QList<ScheduledOrder> list;
    QSqlQuery q("SELECT Id,CustomerId,Weekdays,Time,"
                "COALESCE(RemindMinutesBefore,60),"
                "COALESCE(RemindRepeatInterval,0),"
                "COALESCE(AutoCreateOrder,0),"
                "COALESCE(IsRecurring,1),"
                "OneTimeDate "
                "FROM ScheduledOrders", dbConn);
    while (q.next()) {
        ScheduledOrder s;
        s.id         = q.value(0).toInt();
        s.customerId = q.value(1).toInt();
        s.weekdays   = q.value(2).toString();
        s.time       = QTime::fromString(q.value(3).toString(),"HH:mm");
        s.remindMinutesBefore  = q.value(4).toInt();
        s.remindRepeatInterval = q.value(5).toInt();
        s.autoCreateOrder      = q.value(6).toBool();
        s.isRecurring          = q.value(7).toBool();
        QString otd            = q.value(8).toString();
        if (!otd.isEmpty()) s.oneTimeDate = QDate::fromString(otd, Qt::ISODate);
        loadItems(s);
        list.append(s);
    }
    return list;
}
ScheduledOrder SQLiteScheduledOrderRepository::getById(int id) {
    QSqlDatabase dbConn = db();
    ensureScheduledOrderColumns(dbConn);
    ScheduledOrder s; QSqlQuery q(dbConn);
    q.prepare("SELECT Id,CustomerId,Weekdays,Time,"
              "COALESCE(RemindMinutesBefore,60),"
              "COALESCE(RemindRepeatInterval,0),"
              "COALESCE(AutoCreateOrder,0),"
              "COALESCE(IsRecurring,1),"
              "OneTimeDate "
              "FROM ScheduledOrders WHERE Id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        s.id         = q.value(0).toInt();
        s.customerId = q.value(1).toInt();
        s.weekdays   = q.value(2).toString();
        s.time       = QTime::fromString(q.value(3).toString(),"HH:mm");
        s.remindMinutesBefore  = q.value(4).toInt();
        s.remindRepeatInterval = q.value(5).toInt();
        s.autoCreateOrder      = q.value(6).toBool();
        s.isRecurring          = q.value(7).toBool();
        QString otd            = q.value(8).toString();
        if (!otd.isEmpty()) s.oneTimeDate = QDate::fromString(otd, Qt::ISODate);
        loadItems(s);
    }
    return s;
}
bool SQLiteScheduledOrderRepository::save(ScheduledOrder& so) {
    QSqlDatabase dbConn=db();
    ensureScheduledOrderColumns(dbConn);
    dbConn.transaction(); QSqlQuery q(dbConn); bool ok=false;
    // For one-time orders, OneTimeDate must be a valid date string; use NULL-safe value
    QVariant otdVal = (!so.isRecurring && so.oneTimeDate.isValid())
        ? QVariant(so.oneTimeDate.toString(Qt::ISODate))
        : QVariant(QMetaType(QMetaType::QString));   // NULL

    // BUG 3 FIX: Weekdays is NOT NULL in the schema. For one-time orders
    // so.weekdays is a null QString, which Qt binds as SQL NULL — violating
    // the NOT NULL constraint. Coerce it to an empty string.
    QString weekdaysVal = so.weekdays.isNull() ? QString("") : so.weekdays;
    if (so.id > 0) {
        q.prepare("UPDATE ScheduledOrders SET CustomerId=?,Weekdays=?,Time=?,"
                  "RemindMinutesBefore=?,RemindRepeatInterval=?,AutoCreateOrder=?,"
                  "IsRecurring=?,OneTimeDate=? WHERE Id=?");
        q.addBindValue(so.customerId); q.addBindValue(weekdaysVal);
        q.addBindValue(so.time.toString("HH:mm"));
        q.addBindValue(so.remindMinutesBefore);
        q.addBindValue(so.remindRepeatInterval);
        q.addBindValue(so.autoCreateOrder ? 1 : 0);
        q.addBindValue(so.isRecurring ? 1 : 0);
        q.addBindValue(otdVal);
        q.addBindValue(so.id);
        ok = q.exec();
        if (!ok) Logger::instance().error("ScheduledOrder UPDATE failed: " + q.lastError().text());
    } else {
        q.prepare("INSERT INTO ScheduledOrders (CustomerId,Weekdays,Time,"
                  "RemindMinutesBefore,RemindRepeatInterval,AutoCreateOrder,"
                  "IsRecurring,OneTimeDate) VALUES (?,?,?,?,?,?,?,?)");
        q.addBindValue(so.customerId); q.addBindValue(weekdaysVal);
        q.addBindValue(so.time.toString("HH:mm"));
        q.addBindValue(so.remindMinutesBefore);
        q.addBindValue(so.remindRepeatInterval);
        q.addBindValue(so.autoCreateOrder ? 1 : 0);
        q.addBindValue(so.isRecurring ? 1 : 0);
        q.addBindValue(otdVal);
        if (q.exec()) { so.id=q.lastInsertId().toInt(); ok=true; }
        else Logger::instance().error("ScheduledOrder INSERT failed: " + q.lastError().text());
    }
    if (ok) ok = saveItems(so);
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}
bool SQLiteScheduledOrderRepository::remove(int id) {
    QSqlDatabase dbConn=db(); dbConn.transaction(); QSqlQuery q(dbConn);
    q.prepare("DELETE FROM ScheduledOrderItems WHERE ScheduledOrderId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM ScheduledOrders WHERE Id=?"); q.addBindValue(id); bool ok=q.exec();
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}
QList<ScheduledOrder> SQLiteScheduledOrderRepository::getByCustomerId(int cid) {
    QSqlDatabase dbConn = db();
    ensureScheduledOrderColumns(dbConn);
    QList<ScheduledOrder> list; QSqlQuery q(dbConn);
    q.prepare("SELECT Id,CustomerId,Weekdays,Time,"
              "COALESCE(RemindMinutesBefore,60),"
              "COALESCE(RemindRepeatInterval,0),"
              "COALESCE(AutoCreateOrder,0),"
              "COALESCE(IsRecurring,1),"
              "OneTimeDate "
              "FROM ScheduledOrders WHERE CustomerId=?");
    q.addBindValue(cid);
    if (q.exec()) while (q.next()) {
        ScheduledOrder s;
        s.id=q.value(0).toInt(); s.customerId=q.value(1).toInt();
        s.weekdays=q.value(2).toString(); s.time=QTime::fromString(q.value(3).toString(),"HH:mm");
        s.remindMinutesBefore=q.value(4).toInt();
        s.remindRepeatInterval=q.value(5).toInt();
        s.autoCreateOrder=q.value(6).toBool();
        s.isRecurring=q.value(7).toBool();
        QString otd=q.value(8).toString();
        if (!otd.isEmpty()) s.oneTimeDate=QDate::fromString(otd,Qt::ISODate);
        loadItems(s); list.append(s);
    }
    return list;
}

// ==========================================
// SQLiteMissingProductRepository
// ==========================================
static MissingProduct mpFromQuery(const QSqlQuery& q) {
    MissingProduct m;
    m.id=q.value(0).toInt(); m.productId=q.value(1).toInt();
    // productName: prefer the stored Name column (index 2), fall back to joined p.Name (index 3)
    QString storedName = q.value(2).toString();
    QString joinedName = q.value(3).toString();
    m.productName = storedName.isEmpty() ? joinedName : storedName;
    m.barcode=q.value(4).toString(); m.quantityNeeded=q.value(5).toInt(); m.branchId=q.value(6).toInt();
    m.dateAdded=QDate::fromString(q.value(7).toString(),Qt::ISODate); m.purchased=q.value(8).toBool();
    m.source = q.value(9).toString();
    if (m.source.isEmpty()) m.source = "manual";
    m.currentQty = q.value(10).toInt();
    m.unitLabel  = q.value(11).toString();   // Issues 4&5
    return m;
}
QList<MissingProduct> SQLiteMissingProductRepository::getAll() {
    QSqlDatabase dbConn = db();
    // Migration: add UnitLabel column if missing
    {
        QSqlRecord rec = dbConn.record("MissingProducts");
        if (!rec.contains("UnitLabel")) {
            QSqlQuery q(dbConn);
            q.exec("ALTER TABLE MissingProducts ADD COLUMN UnitLabel TEXT DEFAULT ''");
        }
    }
    QList<MissingProduct> list;
    QSqlQuery q("SELECT m.Id, m.ProductId, m.Name, COALESCE(p.Name,''), m.Barcode, "
                "m.QuantityNeeded, m.BranchId, m.DateAdded, m.Purchased, "
                "COALESCE(m.Source,'manual'), COALESCE(m.CurrentQty,0), COALESCE(m.UnitLabel,'') "
                "FROM MissingProducts m LEFT JOIN Products p ON m.ProductId=p.Id "
                "ORDER BY m.DateAdded DESC", dbConn);
    while (q.next()) list.append(mpFromQuery(q));
    return list;
}
MissingProduct SQLiteMissingProductRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare("SELECT m.Id, m.ProductId, m.Name, COALESCE(p.Name,''), m.Barcode, "
              "m.QuantityNeeded, m.BranchId, m.DateAdded, m.Purchased, "
              "COALESCE(m.Source,'manual'), COALESCE(m.CurrentQty,0), COALESCE(m.UnitLabel,'') "
              "FROM MissingProducts m LEFT JOIN Products p ON m.ProductId=p.Id WHERE m.Id=?");
    q.addBindValue(id); if (q.exec() && q.next()) return mpFromQuery(q); return {};
}
MissingProduct SQLiteMissingProductRepository::getActiveAutoFlagByProductId(int productId) {
    QSqlQuery q(db());
    q.prepare("SELECT m.Id, m.ProductId, m.Name, COALESCE(p.Name,''), m.Barcode, "
              "m.QuantityNeeded, m.BranchId, m.DateAdded, m.Purchased, "
              "COALESCE(m.Source,'manual'), COALESCE(m.CurrentQty,0), COALESCE(m.UnitLabel,'') "
              "FROM MissingProducts m LEFT JOIN Products p ON m.ProductId=p.Id "
              "WHERE m.ProductId=? AND COALESCE(m.Source,'manual')='auto_low_stock' AND m.Purchased=0 "
              "LIMIT 1");
    q.addBindValue(productId);
    if (q.exec() && q.next()) return mpFromQuery(q);
    return {};
}
bool SQLiteMissingProductRepository::save(MissingProduct& m) {
    QSqlQuery q(db());
    if (m.id > 0) {
        q.prepare("UPDATE MissingProducts SET ProductId=?,Name=?,Barcode=?,QuantityNeeded=?,BranchId=?,DateAdded=?,Purchased=?,Source=?,CurrentQty=?,UnitLabel=? WHERE Id=?");
        q.addBindValue(m.productId); q.addBindValue(m.productName); q.addBindValue(m.barcode);
        q.addBindValue(m.quantityNeeded); q.addBindValue(m.branchId);
        q.addBindValue(m.dateAdded.toString(Qt::ISODate));
        q.addBindValue(m.purchased?1:0);
        q.addBindValue(m.source.isEmpty() ? "manual" : m.source);
        q.addBindValue(m.currentQty);
        q.addBindValue(m.unitLabel);
        q.addBindValue(m.id); return q.exec();
    } else {
        q.prepare("INSERT INTO MissingProducts (ProductId,Name,Barcode,QuantityNeeded,BranchId,DateAdded,Purchased,Source,CurrentQty,UnitLabel) VALUES (?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(m.productId); q.addBindValue(m.productName); q.addBindValue(m.barcode);
        q.addBindValue(m.quantityNeeded); q.addBindValue(m.branchId);
        q.addBindValue(m.dateAdded.toString(Qt::ISODate)); q.addBindValue(m.purchased?1:0);
        q.addBindValue(m.source.isEmpty() ? "manual" : m.source);
        q.addBindValue(m.currentQty);
        q.addBindValue(m.unitLabel);
        if (q.exec()) { m.id=q.lastInsertId().toInt(); return true; }
    }
    return false;
}
bool SQLiteMissingProductRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM MissingProducts WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteUserRepository
// ==========================================
QList<User> SQLiteUserRepository::getAll() {
    QSqlDatabase dbConn = db();
    ensureUserColumns(dbConn);
    QList<User> list;
    QSqlQuery q("SELECT Id,Username,PasswordHash,PasswordSalt,RoleId,Name,"
                "COALESCE(Phone,''),COALESCE(PhotoPath,''),COALESCE(FingerprintBarcode,''),"
                "COALESCE(HourlyRate,0),COALESCE(WeeklyOffDays,0) FROM Users", dbConn);
    if (q.lastError().isValid()) {
        // A failure here returns an empty list, which the login screen reads as
        // "no users exist" and pops the first-run admin setup. Log it loudly.
        Logger::instance().error("User getAll() SQL failed: " + q.lastError().text());
        return list;
    }
    while (q.next()) {
        User u(q.value(0).toInt(), q.value(5).toString());
        u.setUsername(q.value(1).toString()); u.setPasswordHash(q.value(2).toString());
        u.setPasswordSalt(q.value(3).toString()); u.setRoleId(q.value(4).toInt());
        u.setPhone(q.value(6).toString());
        u.setPhotoPath(q.value(7).toString()); u.setFingerprintBarcode(q.value(8).toString());
        u.setHourlyRate(q.value(9).toDouble()); u.setWeeklyOffDays(q.value(10).toInt());
        list.append(u);
    }
    return list;
}
User SQLiteUserRepository::getById(int id) {
    QSqlDatabase dbConn = db();
    ensureUserColumns(dbConn);
    User u; QSqlQuery q(dbConn);
    q.prepare("SELECT Id,Username,PasswordHash,PasswordSalt,RoleId,Name,Phone,Email,PhotoPath,FingerprintBarcode,HourlyRate,WeeklyOffDays FROM Users WHERE Id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        u.setId(q.value(0).toInt()); u.setUsername(q.value(1).toString());
        u.setPasswordHash(q.value(2).toString()); u.setPasswordSalt(q.value(3).toString());
        u.setRoleId(q.value(4).toInt()); u.setName(q.value(5).toString()); u.setPhone(q.value(6).toString());
        u.setEmail(q.value(7).toString());
        u.setPhotoPath(q.value(8).toString()); u.setFingerprintBarcode(q.value(9).toString());
        u.setHourlyRate(q.value(10).toDouble()); u.setWeeklyOffDays(q.value(11).toInt());
    }
    return u;
}
User SQLiteUserRepository::getByUsername(const QString& username) {
    QSqlDatabase dbConn = db();
    ensureUserColumns(dbConn);
    User u; QSqlQuery q(dbConn);
    q.prepare("SELECT Id,Username,PasswordHash,PasswordSalt,RoleId,Name,Phone,Email,PhotoPath,FingerprintBarcode,HourlyRate,WeeklyOffDays FROM Users WHERE Username=?");
    q.addBindValue(username);
    if (q.exec() && q.next()) {
        u.setId(q.value(0).toInt()); u.setUsername(q.value(1).toString());
        u.setPasswordHash(q.value(2).toString()); u.setPasswordSalt(q.value(3).toString());
        u.setRoleId(q.value(4).toInt()); u.setName(q.value(5).toString()); u.setPhone(q.value(6).toString());
        u.setEmail(q.value(7).toString());
        u.setPhotoPath(q.value(8).toString()); u.setFingerprintBarcode(q.value(9).toString());
        u.setHourlyRate(q.value(10).toDouble()); u.setWeeklyOffDays(q.value(11).toInt());
    }
    return u;
}
User SQLiteUserRepository::getByEmail(const QString& email) {
    QSqlDatabase dbConn = db();
    ensureUserColumns(dbConn);
    User u; QSqlQuery q(dbConn);
    q.prepare("SELECT Id,Username,PasswordHash,PasswordSalt,RoleId,Name,Phone,Email,PhotoPath,FingerprintBarcode,HourlyRate,WeeklyOffDays FROM Users WHERE Email=?");
    q.addBindValue(email);
    if (q.exec() && q.next()) {
        u.setId(q.value(0).toInt()); u.setUsername(q.value(1).toString());
        u.setPasswordHash(q.value(2).toString()); u.setPasswordSalt(q.value(3).toString());
        u.setRoleId(q.value(4).toInt()); u.setName(q.value(5).toString()); u.setPhone(q.value(6).toString());
        u.setEmail(q.value(7).toString());
        u.setPhotoPath(q.value(8).toString()); u.setFingerprintBarcode(q.value(9).toString());
        u.setHourlyRate(q.value(10).toDouble()); u.setWeeklyOffDays(q.value(11).toInt());
    }
    return u;
}
bool SQLiteUserRepository::save(User& u) {
    QSqlDatabase dbConn = db();
    ensureUserColumns(dbConn);
    QSqlQuery q(dbConn);
    if (u.id() > 0) {
        q.prepare("UPDATE Users SET Username=?,PasswordHash=?,PasswordSalt=?,RoleId=?,Name=?,Phone=?,Email=?,"
                  "PhotoPath=?,FingerprintBarcode=?,HourlyRate=?,WeeklyOffDays=? WHERE Id=?");
        q.addBindValue(u.username()); q.addBindValue(u.passwordHash()); q.addBindValue(u.passwordSalt());
        q.addBindValue(u.roleId()); q.addBindValue(u.name()); q.addBindValue(u.phone());
        q.addBindValue(u.email());
        q.addBindValue(u.photoPath()); q.addBindValue(u.fingerprintBarcode());
        q.addBindValue(u.hourlyRate()); q.addBindValue(u.weeklyOffDays());
        q.addBindValue(u.id());
        bool ok = q.exec();
        if (!ok) Logger::instance().error("User UPDATE failed: " + q.lastError().text());
        return ok;
    } else {
        q.prepare("INSERT INTO Users (Username,PasswordHash,PasswordSalt,RoleId,Name,Phone,Email,"
                  "PhotoPath,FingerprintBarcode,HourlyRate,WeeklyOffDays) VALUES (?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(u.username()); q.addBindValue(u.passwordHash()); q.addBindValue(u.passwordSalt());
        q.addBindValue(u.roleId()); q.addBindValue(u.name()); q.addBindValue(u.phone());
        q.addBindValue(u.email());
        q.addBindValue(u.photoPath()); q.addBindValue(u.fingerprintBarcode());
        q.addBindValue(u.hourlyRate()); q.addBindValue(u.weeklyOffDays());
        if (q.exec()) { u.setId(q.lastInsertId().toInt()); return true; }
        Logger::instance().error("User INSERT failed: " + q.lastError().text());
    }
    return false;
}
bool SQLiteUserRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM Users WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteAuditLogRepository
// ==========================================
static AuditLogEntry auditFromQuery(const QSqlQuery& q) {
    AuditLogEntry e;
    e.id=q.value(0).toInt(); e.userId=q.value(1).toInt(); e.username=q.value(2).toString();
    e.action=q.value(3).toString(); e.entityType=q.value(4).toString(); e.entityId=q.value(5).toInt();
    e.details=q.value(6).toString(); e.timestamp=QDateTime::fromString(q.value(7).toString(),Qt::ISODate);
    return e;
}
static const char* kAuditSel =
    "SELECT l.Id,l.UserId,u.Username,l.Action,l.EntityType,l.EntityId,l.Details,l.Timestamp "
    "FROM AuditLogs l LEFT JOIN Users u ON l.UserId=u.Id";
QList<AuditLogEntry> SQLiteAuditLogRepository::getAll() {
    QList<AuditLogEntry> list;
    QSqlQuery q(QString(kAuditSel)+" ORDER BY l.Timestamp DESC", db());
    while (q.next()) list.append(auditFromQuery(q));
    return list;
}
bool SQLiteAuditLogRepository::addEntry(const AuditLogEntry& e) {
    QSqlQuery q(db());
    q.prepare("INSERT INTO AuditLogs (UserId,Action,EntityType,EntityId,Details,Timestamp) VALUES (?,?,?,?,?,?)");
    q.addBindValue(e.userId); q.addBindValue(e.action); q.addBindValue(e.entityType);
    q.addBindValue(e.entityId); q.addBindValue(e.details); q.addBindValue(e.timestamp.toString(Qt::ISODate));
    return q.exec();
}
QList<AuditLogEntry> SQLiteAuditLogRepository::getEntriesDateRange(const QDateTime& s, const QDateTime& e) {
    QList<AuditLogEntry> list; QSqlQuery q(db());
    q.prepare(QString(kAuditSel)+" WHERE l.Timestamp BETWEEN ? AND ? ORDER BY l.Timestamp ASC");
    q.addBindValue(s.toString(Qt::ISODate)); q.addBindValue(e.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) list.append(auditFromQuery(q));
    return list;
}

// ==========================================
// SQLiteDeliveryDriverRepository
// ==========================================
static DeliveryDriver driverFromQuery(const QSqlQuery& q) {
    DeliveryDriver d;
    d.setId(q.value(0).toInt()); d.setName(q.value(1).toString()); d.setPhone(q.value(2).toString());
    d.setNationalId(q.value(3).toString()); d.setActive(q.value(4).toBool());
    return d;
}
QList<DeliveryDriver> SQLiteDeliveryDriverRepository::getAll() {
    QList<DeliveryDriver> list;
    QSqlQuery q("SELECT Id,Name,Phone,NationalId,Active FROM DeliveryDrivers ORDER BY Name", db());
    while (q.next()) list.append(driverFromQuery(q)); return list;
}
QList<DeliveryDriver> SQLiteDeliveryDriverRepository::getActive() {
    QList<DeliveryDriver> list;
    QSqlQuery q("SELECT Id,Name,Phone,NationalId,Active FROM DeliveryDrivers WHERE Active=1 ORDER BY Name", db());
    while (q.next()) list.append(driverFromQuery(q)); return list;
}
DeliveryDriver SQLiteDeliveryDriverRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare("SELECT Id,Name,Phone,NationalId,Active FROM DeliveryDrivers WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) return driverFromQuery(q); return {};
}
bool SQLiteDeliveryDriverRepository::save(DeliveryDriver& d) {
    QSqlQuery q(db());
    if (d.id() > 0) {
        q.prepare("UPDATE DeliveryDrivers SET Name=?,Phone=?,NationalId=?,Active=? WHERE Id=?");
        q.addBindValue(d.name()); q.addBindValue(d.phone()); q.addBindValue(d.nationalId()); q.addBindValue(d.active()?1:0); q.addBindValue(d.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO DeliveryDrivers (Name,Phone,NationalId,Active) VALUES (?,?,?,?)");
        q.addBindValue(d.name()); q.addBindValue(d.phone()); q.addBindValue(d.nationalId()); q.addBindValue(d.active()?1:0);
        if (q.exec()) { d.setId(q.lastInsertId().toInt()); return true; }
    }
    return false;
}
bool SQLiteDeliveryDriverRepository::remove(int id) {
    QSqlQuery q(db()); q.prepare("DELETE FROM DeliveryDrivers WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteCouponRepository
// ==========================================
static Coupon couponFromQuery(const QSqlQuery& q) {
    Coupon c;
    c.setId(q.value(0).toInt()); c.setCode(q.value(1).toString());
    c.setDescription(q.value(2).toString()); c.setType(Coupon::stringToType(q.value(3).toString()));
    c.setValue(q.value(4).toDouble()); c.setExpiryDate(QDate::fromString(q.value(5).toString(), Qt::ISODate));
    c.setMaxUses(q.value(6).toInt()); c.setUsedCount(q.value(7).toInt()); c.setActive(q.value(8).toBool());
    return c;
}
static const char* kCoupSel =
    "SELECT Id,Code,Description,Type,Value,ExpiryDate,MaxUses,UsedCount,Active FROM Coupons";
QList<Coupon> SQLiteCouponRepository::getAll() {
    QList<Coupon> list; QSqlQuery q(kCoupSel, db()); while (q.next()) list.append(couponFromQuery(q)); return list;
}
Coupon SQLiteCouponRepository::getByCode(const QString& code) {
    QSqlQuery q(db()); q.prepare(QString(kCoupSel)+" WHERE UPPER(Code)=UPPER(?)"); q.addBindValue(code);
    if (q.exec() && q.next()) return couponFromQuery(q); return {};
}
Coupon SQLiteCouponRepository::getById(int id) {
    QSqlQuery q(db()); q.prepare(QString(kCoupSel)+" WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) return couponFromQuery(q); return {};
}
bool SQLiteCouponRepository::save(Coupon& c) {
    QSqlQuery q(db());
    auto exp = [&]() -> QVariant { return c.expiryDate().isValid() ? QVariant(c.expiryDate().toString(Qt::ISODate)) : QVariant(); };
    if (c.id() > 0) {
        q.prepare("UPDATE Coupons SET Code=?,Description=?,Type=?,Value=?,ExpiryDate=?,MaxUses=?,UsedCount=?,Active=? WHERE Id=?");
        q.addBindValue(c.code()); q.addBindValue(c.description()); q.addBindValue(Coupon::typeToString(c.type()));
        q.addBindValue(c.value()); q.addBindValue(exp()); q.addBindValue(c.maxUses()); q.addBindValue(c.usedCount()); q.addBindValue(c.isActive()?1:0); q.addBindValue(c.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO Coupons (Code,Description,Type,Value,ExpiryDate,MaxUses,UsedCount,Active) VALUES (?,?,?,?,?,?,?,?)");
        q.addBindValue(c.code()); q.addBindValue(c.description()); q.addBindValue(Coupon::typeToString(c.type()));
        q.addBindValue(c.value()); q.addBindValue(exp()); q.addBindValue(c.maxUses()); q.addBindValue(c.usedCount()); q.addBindValue(c.isActive()?1:0);
        if (q.exec()) { c.setId(q.lastInsertId().toInt()); return true; }
    }
    return false;
}
bool SQLiteCouponRepository::remove(int id) { QSqlQuery q(db()); q.prepare("DELETE FROM Coupons WHERE Id=?"); q.addBindValue(id); return q.exec(); }
bool SQLiteCouponRepository::incrementUsage(int id) {
    QSqlQuery q(db()); q.prepare("UPDATE Coupons SET UsedCount=UsedCount+1 WHERE Id=?"); q.addBindValue(id); return q.exec();
}

// ==========================================
// SQLiteOrderReturnRepository
// ==========================================
void SQLiteOrderReturnRepository::loadItems(OrderReturn& ret) {
    QSqlQuery q(db()); q.prepare("SELECT ProductId,ProductName,QuantityReturned,UnitPrice FROM ReturnItems WHERE ReturnId=?"); q.addBindValue(ret.id());
    if (q.exec()) {
        QList<OrderReturnItem> items;
        while (q.next()) {
            OrderReturnItem i; i.orderItemProductId=q.value(0).toInt(); i.productName=q.value(1).toString();
            i.quantityReturned=q.value(2).toInt(); i.unitPrice=q.value(3).toDouble(); items.append(i);
        }
        ret.setItems(items);
    }
}
bool SQLiteOrderReturnRepository::saveItems(const OrderReturn& ret) {
    QSqlQuery q(db()); q.prepare("DELETE FROM ReturnItems WHERE ReturnId=?"); q.addBindValue(ret.id()); if (!q.exec()) return false;
    for (const auto& i : ret.items()) {
        q.prepare("INSERT INTO ReturnItems (ReturnId,ProductId,ProductName,QuantityReturned,UnitPrice) VALUES (?,?,?,?,?)");
        q.addBindValue(ret.id()); q.addBindValue(i.orderItemProductId); q.addBindValue(i.productName);
        q.addBindValue(i.quantityReturned); q.addBindValue(i.unitPrice);
        if (!q.exec()) return false;
    }
    return true;
}
static OrderReturn returnFromQuery(const QSqlQuery& q) {
    OrderReturn r; r.setId(q.value(0).toInt()); r.setOrderId(q.value(1).toInt());
    r.setCustomerId(q.value(2).toInt()); r.setCustomerName(q.value(3).toString());
    r.setReason(q.value(4).toString()); r.setDateTime(QDateTime::fromString(q.value(5).toString(),Qt::ISODate));
    r.setFullReturn(q.value(6).toBool()); return r;
}
static const char* kRetSel = "SELECT Id,OrderId,CustomerId,CustomerName,Reason,DateTime,IsFullReturn FROM OrderReturns";
QList<OrderReturn> SQLiteOrderReturnRepository::getAll() {
    QList<OrderReturn> list; QSqlQuery q(QString(kRetSel)+" ORDER BY DateTime DESC", db());
    while (q.next()) { auto r=returnFromQuery(q); loadItems(r); list.append(r); } return list;
}
QList<OrderReturn> SQLiteOrderReturnRepository::getByOrderId(int oid) {
    QList<OrderReturn> list; QSqlQuery q(db()); q.prepare(QString(kRetSel)+" WHERE OrderId=?"); q.addBindValue(oid);
    if (q.exec()) while (q.next()) { auto r=returnFromQuery(q); loadItems(r); list.append(r); } return list;
}
OrderReturn SQLiteOrderReturnRepository::getById(int id) {
    QSqlQuery q(db()); q.prepare(QString(kRetSel)+" WHERE Id=?"); q.addBindValue(id);
    if (q.exec() && q.next()) { auto r=returnFromQuery(q); loadItems(r); return r; } return {};
}
bool SQLiteOrderReturnRepository::save(OrderReturn& ret) {
    QSqlDatabase dbConn=db(); dbConn.transaction(); QSqlQuery q(dbConn); bool ok=false;
    if (ret.id() > 0) {
        q.prepare("UPDATE OrderReturns SET OrderId=?,CustomerId=?,CustomerName=?,Reason=?,DateTime=?,IsFullReturn=? WHERE Id=?");
        q.addBindValue(ret.orderId()); q.addBindValue(ret.customerId()); q.addBindValue(ret.customerName());
        q.addBindValue(ret.reason()); q.addBindValue(ret.dateTime().toString(Qt::ISODate)); q.addBindValue(ret.isFullReturn()?1:0); q.addBindValue(ret.id()); ok=q.exec();
    } else {
        q.prepare("INSERT INTO OrderReturns (OrderId,CustomerId,CustomerName,Reason,DateTime,IsFullReturn) VALUES (?,?,?,?,?,?)");
        q.addBindValue(ret.orderId()); q.addBindValue(ret.customerId()); q.addBindValue(ret.customerName());
        q.addBindValue(ret.reason()); q.addBindValue(ret.dateTime().toString(Qt::ISODate)); q.addBindValue(ret.isFullReturn()?1:0);
        if (q.exec()) { ret.setId(q.lastInsertId().toInt()); ok=true; }
    }
    if (ok) ok=saveItems(ret);
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}
bool SQLiteOrderReturnRepository::remove(int id) {
    QSqlDatabase dbConn=db(); dbConn.transaction(); QSqlQuery q(dbConn);
    q.prepare("DELETE FROM ReturnItems WHERE ReturnId=?"); q.addBindValue(id); q.exec();
    q.prepare("DELETE FROM OrderReturns WHERE Id=?"); q.addBindValue(id); bool ok=q.exec();
    ok ? dbConn.commit() : dbConn.rollback(); return ok;
}

// ==========================================
// SQLiteNoteRepository
// ==========================================
static void ensureNotesTable(const QSqlDatabase& dbConn) {
    QSqlQuery q(dbConn);
    // Base table (created on first run)
    q.exec(
        "CREATE TABLE IF NOT EXISTS Notes ("
        "  Id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  Title       TEXT    NOT NULL DEFAULT '',"
        "  Content     TEXT    NOT NULL DEFAULT '',"
        "  Color       TEXT    NOT NULL DEFAULT 'yellow',"
        "  Pinned      INTEGER NOT NULL DEFAULT 0,"
        "  Attachments TEXT             DEFAULT '',"
        "  CreatedAt   TEXT    NOT NULL DEFAULT '',"
        "  UpdatedAt   TEXT    NOT NULL DEFAULT ''"
        ")"
    );
    // Migration: add new columns to existing installations
    QSqlRecord rec = dbConn.record("Notes");
    if (!rec.contains("Pinned")) {
        q.exec("ALTER TABLE Notes ADD COLUMN Pinned INTEGER NOT NULL DEFAULT 0");
        q.exec("UPDATE Notes SET Pinned=0 WHERE Pinned IS NULL");
    }
    if (!rec.contains("Attachments")) {
        // Use nullable (no NOT NULL) to avoid constraint failures on old rows
        q.exec("ALTER TABLE Notes ADD COLUMN Attachments TEXT DEFAULT ''");
    }
    // Fix any existing NULL values
    q.exec("UPDATE Notes SET Attachments='' WHERE Attachments IS NULL");
    // Migrate old hex color values to palette keys
    q.exec("UPDATE Notes SET Color='yellow' WHERE Color LIKE '#%' OR Color=''");
}

static Note noteFromQuery(const QSqlQuery& q) {
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

static const char* kNoteSelect =
    "SELECT Id,Title,Content,Color,Pinned,Attachments,CreatedAt,UpdatedAt FROM Notes";

QList<Note> SQLiteNoteRepository::getAll() {
    QSqlDatabase dbConn = db();
    ensureNotesTable(dbConn);
    QList<Note> list;
    // Pinned notes first, then by most-recently updated
    QSqlQuery q(QString("%1 ORDER BY Pinned DESC, UpdatedAt DESC").arg(kNoteSelect), dbConn);
    while (q.next()) list.append(noteFromQuery(q));
    return list;
}

Note SQLiteNoteRepository::getById(int id) {
    QSqlDatabase dbConn = db();
    ensureNotesTable(dbConn);
    QSqlQuery q(dbConn);
    q.prepare(QString("%1 WHERE Id=?").arg(kNoteSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return noteFromQuery(q);
    return {};
}

bool SQLiteNoteRepository::save(Note& n) {
    QSqlDatabase dbConn = db();
    ensureNotesTable(dbConn);
    QSqlQuery q(dbConn);
    if (n.id > 0) {
        n.updatedAt = QDateTime::currentDateTime();
        q.prepare("UPDATE Notes SET Title=?,Content=?,Color=?,Pinned=?,Attachments=?,UpdatedAt=? WHERE Id=?");
        q.addBindValue(n.title);
        q.addBindValue(n.content);
        q.addBindValue(n.color.isEmpty() ? "yellow" : n.color);
        q.addBindValue(n.pinned ? 1 : 0);
        q.addBindValue(n.attachments.isNull() ? QString("") : n.attachments);  // never NULL in UPDATE either
        q.addBindValue(n.updatedAt.toString(Qt::ISODate));
        q.addBindValue(n.id);
        bool ok = q.exec();
        if (!ok) Logger::instance().error("Note UPDATE failed: " + q.lastError().text());
        return ok;
    } else {
        n.createdAt = QDateTime::currentDateTime();
        n.updatedAt = n.createdAt;
        q.prepare(
            "INSERT INTO Notes (Title,Content,Color,Pinned,Attachments,CreatedAt,UpdatedAt)"
            " VALUES (?,?,?,?,?,?,?)");
        q.addBindValue(n.title);
        q.addBindValue(n.content);
        q.addBindValue(n.color.isEmpty() ? "yellow" : n.color);
        q.addBindValue(n.pinned ? 1 : 0);
        q.addBindValue(n.attachments.isNull() ? QString("") : n.attachments);  // never NULL
        q.addBindValue(n.createdAt.toString(Qt::ISODate));
        q.addBindValue(n.updatedAt.toString(Qt::ISODate));
        if (q.exec()) { n.id = q.lastInsertId().toInt(); return true; }
        Logger::instance().error("Note INSERT failed: " + q.lastError().text()
            + " | DB: " + dbConn.databaseName());
        return false;
    }
}

bool SQLiteNoteRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM Notes WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ══════════════════════════════════════════════════════════════════════════════
// Phase 1: New SQLite Repository Implementations
// ══════════════════════════════════════════════════════════════════════════════

// ==========================================
// SQLiteSupplierRepository
// ==========================================
static Supplier supplierFromQuery(const QSqlQuery& q) {
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

static const char* kSupplierSelect = 
    "SELECT Id,Name,ContactName,Phone,Email,Address,Notes,Active FROM Suppliers";

QList<Supplier> SQLiteSupplierRepository::getAll() {
    QList<Supplier> list;
    QSqlQuery q(QString("%1 ORDER BY Name").arg(kSupplierSelect), db());
    while (q.next()) list.append(supplierFromQuery(q));
    return list;
}

QList<Supplier> SQLiteSupplierRepository::getActive() {
    QList<Supplier> list;
    QSqlQuery q(QString("%1 WHERE Active=1 ORDER BY Name").arg(kSupplierSelect), db());
    while (q.next()) list.append(supplierFromQuery(q));
    return list;
}

Supplier SQLiteSupplierRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kSupplierSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return supplierFromQuery(q);
    return {};
}

bool SQLiteSupplierRepository::save(Supplier& s) {
    QSqlQuery q(db());
    if (s.id() > 0) {
        q.prepare("UPDATE Suppliers SET Name=?,ContactName=?,Phone=?,Email=?,Address=?,Notes=?,Active=? WHERE Id=?");
        q.addBindValue(s.name());
        q.addBindValue(s.contactName());
        q.addBindValue(s.phone());
        q.addBindValue(s.email());
        q.addBindValue(s.address());
        q.addBindValue(s.notes());
        q.addBindValue(s.active() ? 1 : 0);
        q.addBindValue(s.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO Suppliers (Name,ContactName,Phone,Email,Address,Notes,Active) VALUES (?,?,?,?,?,?,?)");
        q.addBindValue(s.name());
        q.addBindValue(s.contactName());
        q.addBindValue(s.phone());
        q.addBindValue(s.email());
        q.addBindValue(s.address());
        q.addBindValue(s.notes());
        q.addBindValue(s.active() ? 1 : 0);
        if (q.exec()) {
            s.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteSupplierRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM Suppliers WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteStockMovementRepository
// ==========================================
static StockMovement stockMovementFromQuery(const QSqlQuery& q) {
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

static const char* kStockMovementSelect =
    "SELECT Id,ProductId,MovementType,Quantity,ReferenceType,ReferenceId,DateTime,UserId,Notes FROM StockMovements";

QList<StockMovement> SQLiteStockMovementRepository::getAll() {
    QList<StockMovement> list;
    QSqlQuery q(QString("%1 ORDER BY DateTime DESC").arg(kStockMovementSelect), db());
    while (q.next()) list.append(stockMovementFromQuery(q));
    return list;
}

QList<StockMovement> SQLiteStockMovementRepository::getByProductId(int productId) {
    QList<StockMovement> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE ProductId=? ORDER BY DateTime DESC").arg(kStockMovementSelect));
    q.addBindValue(productId);
    if (q.exec()) while (q.next()) list.append(stockMovementFromQuery(q));
    return list;
}

QList<StockMovement> SQLiteStockMovementRepository::getByDateRange(const QDateTime& start, const QDateTime& end) {
    QList<StockMovement> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE DateTime>=? AND DateTime<=? ORDER BY DateTime DESC").arg(kStockMovementSelect));
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) list.append(stockMovementFromQuery(q));
    return list;
}

StockMovement SQLiteStockMovementRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kStockMovementSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return stockMovementFromQuery(q);
    return {};
}

bool SQLiteStockMovementRepository::save(StockMovement& sm) {
    QSqlQuery q(db());
    if (sm.id > 0) {
        q.prepare("UPDATE StockMovements SET ProductId=?,MovementType=?,Quantity=?,ReferenceType=?,ReferenceId=?,DateTime=?,UserId=?,Notes=? WHERE Id=?");
        q.addBindValue(sm.productId);
        q.addBindValue(sm.movementType);
        q.addBindValue(sm.quantity);
        q.addBindValue(sm.referenceType);
        q.addBindValue(sm.referenceId);
        q.addBindValue(sm.dateTime.toString(Qt::ISODate));
        q.addBindValue(sm.userId);
        q.addBindValue(sm.notes);
        q.addBindValue(sm.id);
        return q.exec();
    } else {
        q.prepare("INSERT INTO StockMovements (ProductId,MovementType,Quantity,ReferenceType,ReferenceId,DateTime,UserId,Notes) VALUES (?,?,?,?,?,?,?,?)");
        q.addBindValue(sm.productId);
        q.addBindValue(sm.movementType);
        q.addBindValue(sm.quantity);
        q.addBindValue(sm.referenceType);
        q.addBindValue(sm.referenceId);
        q.addBindValue(sm.dateTime.toString(Qt::ISODate));
        q.addBindValue(sm.userId);
        q.addBindValue(sm.notes);
        if (q.exec()) {
            sm.id = q.lastInsertId().toInt();
            return true;
        }
    }
    return false;
}

bool SQLiteStockMovementRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM StockMovements WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteRegisterSessionRepository
// ==========================================
static RegisterSession registerSessionFromQuery(const QSqlQuery& q) {
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

static const char* kRegisterSessionSelect =
    "SELECT Id,UserId,OpeningCash,ClosingCash,OpenedAt,ClosedAt,Status FROM RegisterSessions";

QList<RegisterSession> SQLiteRegisterSessionRepository::getAll() {
    QList<RegisterSession> list;
    QSqlQuery q(QString("%1 ORDER BY OpenedAt DESC").arg(kRegisterSessionSelect), db());
    while (q.next()) list.append(registerSessionFromQuery(q));
    return list;
}

QList<RegisterSession> SQLiteRegisterSessionRepository::getByUserId(int userId) {
    QList<RegisterSession> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE UserId=? ORDER BY OpenedAt DESC").arg(kRegisterSessionSelect));
    q.addBindValue(userId);
    if (q.exec()) while (q.next()) list.append(registerSessionFromQuery(q));
    return list;
}

RegisterSession SQLiteRegisterSessionRepository::getOpenSessionByUserId(int userId) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE UserId=? AND Status='Open' ORDER BY OpenedAt DESC LIMIT 1").arg(kRegisterSessionSelect));
    q.addBindValue(userId);
    if (q.exec() && q.next()) return registerSessionFromQuery(q);
    return {};
}

RegisterSession SQLiteRegisterSessionRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kRegisterSessionSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return registerSessionFromQuery(q);
    return {};
}

bool SQLiteRegisterSessionRepository::save(RegisterSession& rs) {
    QSqlQuery q(db());
    if (rs.id() > 0) {
        q.prepare("UPDATE RegisterSessions SET UserId=?,OpeningCash=?,ClosingCash=?,OpenedAt=?,ClosedAt=?,Status=? WHERE Id=?");
        q.addBindValue(rs.userId());
        q.addBindValue(rs.openingCash());
        q.addBindValue(rs.closingCash());
        q.addBindValue(rs.openedAt().toString(Qt::ISODate));
        q.addBindValue(rs.closedAt().isValid() ? rs.closedAt().toString(Qt::ISODate) : QVariant());
        q.addBindValue(RegisterSession::statusToString(rs.status()));
        q.addBindValue(rs.id());
        if (!q.exec()) {
            Logger::instance().error(QString("Failed to update RegisterSession #%1: %2").arg(rs.id()).arg(q.lastError().text()));
            return false;
        }
        return true;
    } else {
        q.prepare("INSERT INTO RegisterSessions (UserId,OpeningCash,ClosingCash,OpenedAt,ClosedAt,Status) VALUES (?,?,?,?,?,?)");
        q.addBindValue(rs.userId());
        q.addBindValue(rs.openingCash());
        q.addBindValue(rs.closingCash());
        q.addBindValue(rs.openedAt().toString(Qt::ISODate));
        q.addBindValue(rs.closedAt().isValid() ? rs.closedAt().toString(Qt::ISODate) : QVariant());
        q.addBindValue(RegisterSession::statusToString(rs.status()));
        if (q.exec()) {
            rs.setId(q.lastInsertId().toInt());
            Logger::instance().info(QString("Created RegisterSession #%1 for userId=%2").arg(rs.id()).arg(rs.userId()));
            return true;
        } else {
            Logger::instance().error(QString("Failed to insert RegisterSession: %1").arg(q.lastError().text()));
            return false;
        }
    }
}

bool SQLiteRegisterSessionRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM RegisterSessions WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteCashMovementRepository
// ==========================================
static CashMovement cashMovementFromQuery(const QSqlQuery& q) {
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

static const char* kCashMovementSelect =
    "SELECT Id,RegisterSessionId,Type,Amount,Reason,DateTime,UserId FROM CashMovements";

QList<CashMovement> SQLiteCashMovementRepository::getAll() {
    QList<CashMovement> list;
    QSqlQuery q(QString("%1 ORDER BY DateTime DESC").arg(kCashMovementSelect), db());
    while (q.next()) list.append(cashMovementFromQuery(q));
    return list;
}

QList<CashMovement> SQLiteCashMovementRepository::getByRegisterSessionId(int sessionId) {
    QList<CashMovement> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE RegisterSessionId=? ORDER BY DateTime DESC").arg(kCashMovementSelect));
    q.addBindValue(sessionId);
    if (q.exec()) while (q.next()) list.append(cashMovementFromQuery(q));
    return list;
}

CashMovement SQLiteCashMovementRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kCashMovementSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return cashMovementFromQuery(q);
    return {};
}

bool SQLiteCashMovementRepository::save(CashMovement& cm) {
    QSqlQuery q(db());
    if (cm.id() > 0) {
        q.prepare("UPDATE CashMovements SET RegisterSessionId=?,Type=?,Amount=?,Reason=?,DateTime=?,UserId=? WHERE Id=?");
        q.addBindValue(cm.registerSessionId());
        q.addBindValue(CashMovement::typeToString(cm.type()));
        q.addBindValue(cm.amount());
        q.addBindValue(cm.reason());
        q.addBindValue(cm.dateTime().toString(Qt::ISODate));
        q.addBindValue(cm.userId());
        q.addBindValue(cm.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO CashMovements (RegisterSessionId,Type,Amount,Reason,DateTime,UserId) VALUES (?,?,?,?,?,?)");
        q.addBindValue(cm.registerSessionId());
        q.addBindValue(CashMovement::typeToString(cm.type()));
        q.addBindValue(cm.amount());
        q.addBindValue(cm.reason());
        q.addBindValue(cm.dateTime().toString(Qt::ISODate));
        q.addBindValue(cm.userId());
        if (q.exec()) {
            cm.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteCashMovementRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM CashMovements WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteExpenseCategoryRepository
// ==========================================
static ExpenseCategory expenseCategoryFromQuery(const QSqlQuery& q) {
    ExpenseCategory ec;
    ec.setId(q.value(0).toInt());
    ec.setName(q.value(1).toString());
    ec.setDescription(q.value(2).toString());
    return ec;
}

static const char* kExpenseCategorySelect =
    "SELECT Id,Name,Description FROM ExpenseCategories";

QList<ExpenseCategory> SQLiteExpenseCategoryRepository::getAll() {
    QList<ExpenseCategory> list;
    QSqlQuery q(QString("%1 ORDER BY Name").arg(kExpenseCategorySelect), db());
    while (q.next()) list.append(expenseCategoryFromQuery(q));
    return list;
}

ExpenseCategory SQLiteExpenseCategoryRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kExpenseCategorySelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return expenseCategoryFromQuery(q);
    return {};
}

bool SQLiteExpenseCategoryRepository::save(ExpenseCategory& ec) {
    QSqlQuery q(db());
    if (ec.id() > 0) {
        q.prepare("UPDATE ExpenseCategories SET Name=?,Description=? WHERE Id=?");
        q.addBindValue(ec.name());
        q.addBindValue(ec.description());
        q.addBindValue(ec.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO ExpenseCategories (Name,Description) VALUES (?,?)");
        q.addBindValue(ec.name());
        q.addBindValue(ec.description());
        if (q.exec()) {
            ec.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteExpenseCategoryRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM ExpenseCategories WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteExpenseRepository
// ==========================================
static Expense expenseFromQuery(const QSqlQuery& q) {
    Expense e;
    e.setId(q.value(0).toInt());
    e.setCategoryId(q.value(1).toInt());
    e.setAmount(q.value(2).toDouble());
    e.setDescription(q.value(3).toString());
    e.setDate(QDate::fromString(q.value(4).toString(), Qt::ISODate));
    e.setUserId(q.value(5).toInt());
    e.setRegisterSessionId(q.value(6).toInt());
    e.setPaymentMethod(q.value(7).toString());
    return e;
}

static const char* kExpenseSelect =
    "SELECT Id,CategoryId,Amount,Description,Date,UserId,RegisterSessionId,PaymentMethod FROM Expenses";

QList<Expense> SQLiteExpenseRepository::getAll() {
    QList<Expense> list;
    QSqlQuery q(QString("%1 ORDER BY Date DESC").arg(kExpenseSelect), db());
    while (q.next()) list.append(expenseFromQuery(q));
    return list;
}

QList<Expense> SQLiteExpenseRepository::getByDateRange(const QDate& start, const QDate& end) {
    QList<Expense> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Date>=? AND Date<=? ORDER BY Date DESC").arg(kExpenseSelect));
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) list.append(expenseFromQuery(q));
    return list;
}

QList<Expense> SQLiteExpenseRepository::getByRegisterSessionId(int sessionId) {
    QList<Expense> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE RegisterSessionId=? ORDER BY Date DESC").arg(kExpenseSelect));
    q.addBindValue(sessionId);
    if (q.exec()) while (q.next()) list.append(expenseFromQuery(q));
    return list;
}

Expense SQLiteExpenseRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kExpenseSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return expenseFromQuery(q);
    return {};
}

bool SQLiteExpenseRepository::save(Expense& e) {
    QSqlQuery q(db());
    if (e.id() > 0) {
        q.prepare("UPDATE Expenses SET CategoryId=?,Amount=?,Description=?,Date=?,UserId=?,RegisterSessionId=?,PaymentMethod=? WHERE Id=?");
        q.addBindValue(e.categoryId());
        q.addBindValue(e.amount());
        q.addBindValue(e.description());
        q.addBindValue(e.date().toString(Qt::ISODate));
        q.addBindValue(e.userId());
        q.addBindValue(e.registerSessionId());
        q.addBindValue(e.paymentMethod());
        q.addBindValue(e.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO Expenses (CategoryId,Amount,Description,Date,UserId,RegisterSessionId,PaymentMethod) VALUES (?,?,?,?,?,?,?)");
        q.addBindValue(e.categoryId());
        q.addBindValue(e.amount());
        q.addBindValue(e.description());
        q.addBindValue(e.date().toString(Qt::ISODate));
        q.addBindValue(e.userId());
        q.addBindValue(e.registerSessionId());
        q.addBindValue(e.paymentMethod());
        if (q.exec()) {
            e.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteExpenseRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM Expenses WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLitePurchaseInvoiceRepository
// ==========================================
static PurchaseInvoice purchaseInvoiceFromQuery(const QSqlQuery& q) {
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

static const char* kPurchaseInvoiceSelect =
    "SELECT Id,SupplierId,InvoiceNumber,Date,TotalAmount,Notes,UserId,Status FROM PurchaseInvoices";

QList<PurchaseInvoice> SQLitePurchaseInvoiceRepository::getAll() {
    QList<PurchaseInvoice> list;
    QSqlQuery q(QString("%1 ORDER BY Date DESC").arg(kPurchaseInvoiceSelect), db());
    while (q.next()) list.append(purchaseInvoiceFromQuery(q));
    return list;
}

QList<PurchaseInvoice> SQLitePurchaseInvoiceRepository::getBySupplierId(int supplierId) {
    QList<PurchaseInvoice> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE SupplierId=? ORDER BY Date DESC").arg(kPurchaseInvoiceSelect));
    q.addBindValue(supplierId);
    if (q.exec()) while (q.next()) list.append(purchaseInvoiceFromQuery(q));
    return list;
}

QList<PurchaseInvoice> SQLitePurchaseInvoiceRepository::getByDateRange(const QDate& start, const QDate& end) {
    QList<PurchaseInvoice> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Date>=? AND Date<=? ORDER BY Date DESC").arg(kPurchaseInvoiceSelect));
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) list.append(purchaseInvoiceFromQuery(q));
    return list;
}

PurchaseInvoice SQLitePurchaseInvoiceRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kPurchaseInvoiceSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return purchaseInvoiceFromQuery(q);
    return {};
}

bool SQLitePurchaseInvoiceRepository::save(PurchaseInvoice& pi) {
    QSqlQuery q(db());
    if (pi.id() > 0) {
        q.prepare("UPDATE PurchaseInvoices SET SupplierId=?,InvoiceNumber=?,Date=?,TotalAmount=?,Notes=?,UserId=?,Status=? WHERE Id=?");
        q.addBindValue(pi.supplierId());
        q.addBindValue(pi.invoiceNumber());
        q.addBindValue(pi.date().toString(Qt::ISODate));
        q.addBindValue(pi.totalAmount());
        q.addBindValue(pi.notes());
        q.addBindValue(pi.userId());
        q.addBindValue(PurchaseInvoice::statusToString(pi.status()));
        q.addBindValue(pi.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO PurchaseInvoices (SupplierId,InvoiceNumber,Date,TotalAmount,Notes,UserId,Status) VALUES (?,?,?,?,?,?,?)");
        q.addBindValue(pi.supplierId());
        q.addBindValue(pi.invoiceNumber());
        q.addBindValue(pi.date().toString(Qt::ISODate));
        q.addBindValue(pi.totalAmount());
        q.addBindValue(pi.notes());
        q.addBindValue(pi.userId());
        q.addBindValue(PurchaseInvoice::statusToString(pi.status()));
        if (q.exec()) {
            pi.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLitePurchaseInvoiceRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM PurchaseInvoices WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

QList<PurchaseInvoiceItem> SQLitePurchaseInvoiceRepository::getItems(int invoiceId) {
    QList<PurchaseInvoiceItem> list;
    QSqlQuery q(db());
    q.prepare("SELECT Id,InvoiceId,ProductId,Quantity,UnitCost,TotalCost FROM PurchaseInvoiceItems WHERE InvoiceId=?");
    q.addBindValue(invoiceId);
    if (q.exec()) {
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
    }
    return list;
}

bool SQLitePurchaseInvoiceRepository::saveItems(int invoiceId, const QList<PurchaseInvoiceItem>& items) {
    QSqlQuery q(db());
    // Delete existing items
    q.prepare("DELETE FROM PurchaseInvoiceItems WHERE InvoiceId=?");
    q.addBindValue(invoiceId);
    if (!q.exec()) return false;
    
    // Insert new items
    for (const auto& item : items) {
        q.prepare("INSERT INTO PurchaseInvoiceItems (InvoiceId,ProductId,Quantity,UnitCost,TotalCost) VALUES (?,?,?,?,?)");
        q.addBindValue(invoiceId);
        q.addBindValue(item.productId());
        q.addBindValue(item.quantity());
        q.addBindValue(item.unitCost());
        q.addBindValue(item.totalCost());
        if (!q.exec()) return false;
    }
    return true;
}

// ==========================================
// SQLiteStockCountRepository
// ==========================================
static StockCount stockCountFromQuery(const QSqlQuery& q) {
    StockCount sc;
    sc.setId(q.value(0).toInt());
    sc.setDate(QDate::fromString(q.value(1).toString(), Qt::ISODate));
    sc.setUserId(q.value(2).toInt());
    sc.setStatus(StockCount::stringToStatus(q.value(3).toString()));
    sc.setNotes(q.value(4).toString());
    return sc;
}

static const char* kStockCountSelect =
    "SELECT Id,Date,UserId,Status,Notes FROM StockCounts";

QList<StockCount> SQLiteStockCountRepository::getAll() {
    QList<StockCount> list;
    QSqlQuery q(QString("%1 ORDER BY Date DESC").arg(kStockCountSelect), db());
    while (q.next()) list.append(stockCountFromQuery(q));
    return list;
}

QList<StockCount> SQLiteStockCountRepository::getByDateRange(const QDate& start, const QDate& end) {
    QList<StockCount> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Date>=? AND Date<=? ORDER BY Date DESC").arg(kStockCountSelect));
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) while (q.next()) list.append(stockCountFromQuery(q));
    return list;
}

StockCount SQLiteStockCountRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kStockCountSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return stockCountFromQuery(q);
    return {};
}

bool SQLiteStockCountRepository::save(StockCount& sc) {
    QSqlQuery q(db());
    if (sc.id() > 0) {
        q.prepare("UPDATE StockCounts SET Date=?,UserId=?,Status=?,Notes=? WHERE Id=?");
        q.addBindValue(sc.date().toString(Qt::ISODate));
        q.addBindValue(sc.userId());
        q.addBindValue(StockCount::statusToString(sc.status()));
        q.addBindValue(sc.notes());
        q.addBindValue(sc.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO StockCounts (Date,UserId,Status,Notes) VALUES (?,?,?,?)");
        q.addBindValue(sc.date().toString(Qt::ISODate));
        q.addBindValue(sc.userId());
        q.addBindValue(StockCount::statusToString(sc.status()));
        q.addBindValue(sc.notes());
        if (q.exec()) {
            sc.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteStockCountRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM StockCounts WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

QList<StockCountItem> SQLiteStockCountRepository::getItems(int countId) {
    QList<StockCountItem> list;
    QSqlQuery q(db());
    q.prepare("SELECT Id,StockCountId,ProductId,SystemQty,ActualQty,Difference FROM StockCountItems WHERE StockCountId=?");
    q.addBindValue(countId);
    if (q.exec()) {
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
    }
    return list;
}

bool SQLiteStockCountRepository::saveItems(int countId, const QList<StockCountItem>& items) {
    QSqlQuery q(db());
    // Delete existing items
    q.prepare("DELETE FROM StockCountItems WHERE StockCountId=?");
    q.addBindValue(countId);
    if (!q.exec()) return false;
    
    // Insert new items
    for (const auto& item : items) {
        q.prepare("INSERT INTO StockCountItems (StockCountId,ProductId,SystemQty,ActualQty,Difference) VALUES (?,?,?,?,?)");
        q.addBindValue(countId);
        q.addBindValue(item.productId());
        q.addBindValue(item.systemQty());
        q.addBindValue(item.actualQty());
        q.addBindValue(item.difference());
        if (!q.exec()) return false;
    }
    return true;
}

// ==========================================
// SQLiteProductCostHistoryRepository
// ==========================================
static ProductCostHistory productCostHistoryFromQuery(const QSqlQuery& q) {
    ProductCostHistory pch;
    pch.setId(q.value(0).toInt());
    pch.setProductId(q.value(1).toInt());
    pch.setCostPrice(q.value(2).toDouble());
    pch.setEffectiveDate(QDate::fromString(q.value(3).toString(), Qt::ISODate));
    pch.setPurchaseInvoiceId(q.value(4).toInt());
    return pch;
}

static const char* kProductCostHistorySelect =
    "SELECT Id,ProductId,CostPrice,EffectiveDate,PurchaseInvoiceId FROM ProductCostHistory";

QList<ProductCostHistory> SQLiteProductCostHistoryRepository::getByProductId(int productId) {
    QList<ProductCostHistory> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE ProductId=? ORDER BY EffectiveDate DESC").arg(kProductCostHistorySelect));
    q.addBindValue(productId);
    if (q.exec()) while (q.next()) list.append(productCostHistoryFromQuery(q));
    return list;
}

ProductCostHistory SQLiteProductCostHistoryRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kProductCostHistorySelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return productCostHistoryFromQuery(q);
    return {};
}

bool SQLiteProductCostHistoryRepository::save(ProductCostHistory& pch) {
    QSqlQuery q(db());
    if (pch.id() > 0) {
        q.prepare("UPDATE ProductCostHistory SET ProductId=?,CostPrice=?,EffectiveDate=?,PurchaseInvoiceId=? WHERE Id=?");
        q.addBindValue(pch.productId());
        q.addBindValue(pch.costPrice());
        q.addBindValue(pch.effectiveDate().toString(Qt::ISODate));
        q.addBindValue(pch.purchaseInvoiceId());
        q.addBindValue(pch.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO ProductCostHistory (ProductId,CostPrice,EffectiveDate,PurchaseInvoiceId) VALUES (?,?,?,?)");
        q.addBindValue(pch.productId());
        q.addBindValue(pch.costPrice());
        q.addBindValue(pch.effectiveDate().toString(Qt::ISODate));
        q.addBindValue(pch.purchaseInvoiceId());
        if (q.exec()) {
            pch.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteProductCostHistoryRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM ProductCostHistory WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ════════════════════════════════════════════════════════════════════════════
// ProductSubUnit Repository
// ════════════════════════════════════════════════════════════════════════════

static ProductSubUnit subUnitFromQuery(const QSqlQuery& q) {
    ProductSubUnit su;
    su.setId(q.value(0).toInt());
    su.setProductId(q.value(1).toInt());
    su.setName(q.value(2).toString());
    su.setBarcode(q.value(3).toString());
    su.setQuantityPerUnit(q.value(4).toDouble());
    su.setCostPrice(q.value(5).toDouble());
    su.setSalePrice(q.value(6).toDouble());
    return su;
}

static const char* kSubUnitSelect =
    "SELECT Id,ProductId,Name,Barcode,QuantityPerUnit,CostPrice,SalePrice FROM ProductSubUnits";

QList<ProductSubUnit> SQLiteProductSubUnitRepository::getByProductId(int productId) {
    QList<ProductSubUnit> list;
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE ProductId=?").arg(kSubUnitSelect));
    q.addBindValue(productId);
    if (q.exec()) while (q.next()) list.append(subUnitFromQuery(q));
    return list;
}

ProductSubUnit SQLiteProductSubUnitRepository::getById(int id) {
    QSqlQuery q(db());
    q.prepare(QString("%1 WHERE Id=?").arg(kSubUnitSelect));
    q.addBindValue(id);
    if (q.exec() && q.next()) return subUnitFromQuery(q);
    return {};
}

bool SQLiteProductSubUnitRepository::save(ProductSubUnit& su) {
    QSqlQuery q(db());
    if (su.id() > 0) {
        q.prepare("UPDATE ProductSubUnits SET ProductId=?,Name=?,Barcode=?,QuantityPerUnit=?,CostPrice=?,SalePrice=? WHERE Id=?");
        q.addBindValue(su.productId());
        q.addBindValue(su.name());
        q.addBindValue(su.barcode());
        q.addBindValue(su.quantityPerUnit());
        q.addBindValue(su.costPrice());
        q.addBindValue(su.salePrice());
        q.addBindValue(su.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO ProductSubUnits (ProductId,Name,Barcode,QuantityPerUnit,CostPrice,SalePrice) VALUES (?,?,?,?,?,?)");
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
    return false;
}

bool SQLiteProductSubUnitRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM ProductSubUnits WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ==========================================
// SQLiteAttendanceRepository
// ==========================================
QList<Attendance> SQLiteAttendanceRepository::getAll() {
    QList<Attendance> list;
    QSqlQuery q("SELECT a.Id, a.UserId, u.Name, a.Timestamp, a.Type FROM Attendance a "
                "LEFT JOIN Users u ON a.UserId = u.Id ORDER BY a.Timestamp DESC", db());
    while (q.next()) {
        Attendance att;
        att.setId(q.value(0).toInt());
        att.setUserId(q.value(1).toInt());
        att.setUserName(q.value(2).toString());
        att.setTimestamp(QDateTime::fromString(q.value(3).toString(), Qt::ISODate));
        att.setTypeFromString(q.value(4).toString());
        list.append(att);
    }
    return list;
}

QList<Attendance> SQLiteAttendanceRepository::getByUserId(int userId) {
    QList<Attendance> list;
    QSqlQuery q(db());
    q.prepare("SELECT a.Id, a.UserId, u.Name, a.Timestamp, a.Type FROM Attendance a "
              "LEFT JOIN Users u ON a.UserId = u.Id WHERE a.UserId=? ORDER BY a.Timestamp DESC");
    q.addBindValue(userId);
    if (q.exec()) {
        while (q.next()) {
            Attendance att;
            att.setId(q.value(0).toInt());
            att.setUserId(q.value(1).toInt());
            att.setUserName(q.value(2).toString());
            att.setTimestamp(QDateTime::fromString(q.value(3).toString(), Qt::ISODate));
            att.setTypeFromString(q.value(4).toString());
            list.append(att);
        }
    }
    return list;
}

QList<Attendance> SQLiteAttendanceRepository::getByDateRange(const QDateTime& start, const QDateTime& end) {
    QList<Attendance> list;
    QSqlQuery q(db());
    q.prepare("SELECT a.Id, a.UserId, u.Name, a.Timestamp, a.Type FROM Attendance a "
              "LEFT JOIN Users u ON a.UserId = u.Id WHERE a.Timestamp BETWEEN ? AND ? ORDER BY a.Timestamp DESC");
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next()) {
            Attendance att;
            att.setId(q.value(0).toInt());
            att.setUserId(q.value(1).toInt());
            att.setUserName(q.value(2).toString());
            att.setTimestamp(QDateTime::fromString(q.value(3).toString(), Qt::ISODate));
            att.setTypeFromString(q.value(4).toString());
            list.append(att);
        }
    }
    return list;
}

QList<Attendance> SQLiteAttendanceRepository::getByUserAndDateRange(int userId, const QDateTime& start, const QDateTime& end) {
    QList<Attendance> list;
    QSqlQuery q(db());
    q.prepare("SELECT a.Id, a.UserId, u.Name, a.Timestamp, a.Type FROM Attendance a "
              "LEFT JOIN Users u ON a.UserId = u.Id WHERE a.UserId=? AND a.Timestamp BETWEEN ? AND ? ORDER BY a.Timestamp ASC");
    q.addBindValue(userId);
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next()) {
            Attendance att;
            att.setId(q.value(0).toInt());
            att.setUserId(q.value(1).toInt());
            att.setUserName(q.value(2).toString());
            att.setTimestamp(QDateTime::fromString(q.value(3).toString(), Qt::ISODate));
            att.setTypeFromString(q.value(4).toString());
            list.append(att);
        }
    }
    return list;
}

Attendance SQLiteAttendanceRepository::getLastByUserId(int userId) {
    Attendance att;
    QSqlQuery q(db());
    q.prepare("SELECT a.Id, a.UserId, u.Name, a.Timestamp, a.Type FROM Attendance a "
              "LEFT JOIN Users u ON a.UserId = u.Id WHERE a.UserId=? ORDER BY a.Timestamp DESC LIMIT 1");
    q.addBindValue(userId);
    if (q.exec() && q.next()) {
        att.setId(q.value(0).toInt());
        att.setUserId(q.value(1).toInt());
        att.setUserName(q.value(2).toString());
        att.setTimestamp(QDateTime::fromString(q.value(3).toString(), Qt::ISODate));
        att.setTypeFromString(q.value(4).toString());
    }
    return att;
}

bool SQLiteAttendanceRepository::save(Attendance& attendance) {
    QSqlQuery q(db());
    if (attendance.id() > 0) {
        q.prepare("UPDATE Attendance SET UserId=?, Timestamp=?, Type=? WHERE Id=?");
        q.addBindValue(attendance.userId());
        q.addBindValue(attendance.timestamp().toString(Qt::ISODate));
        q.addBindValue(attendance.typeString());
        q.addBindValue(attendance.id());
        return q.exec();
    } else {
        q.prepare("INSERT INTO Attendance (UserId, Timestamp, Type) VALUES (?,?,?)");
        q.addBindValue(attendance.userId());
        q.addBindValue(attendance.timestamp().toString(Qt::ISODate));
        q.addBindValue(attendance.typeString());
        if (q.exec()) {
            attendance.setId(q.lastInsertId().toInt());
            return true;
        }
    }
    return false;
}

bool SQLiteAttendanceRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM Attendance WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

// ══════════════════════════════════════════════════════════════════════════════
// SQLiteProductSupplierRepository Implementation
// ══════════════════════════════════════════════════════════════════════════════

bool SQLiteProductSupplierRepository::save(const ProductSupplier& ps) {
    QSqlQuery q(db());
    
    // Check if exists
    q.prepare("SELECT COUNT(*) FROM ProductSuppliers WHERE ProductId=? AND SupplierId=?");
    q.addBindValue(ps.productId);
    q.addBindValue(ps.supplierId);
    q.exec();
    bool exists = false;
    if (q.next()) {
        exists = q.value(0).toInt() > 0;
    }
    
    if (exists) {
        q.prepare("UPDATE ProductSuppliers SET PurchasePrice=?, IsPreferred=?, LastPurchaseDate=? "
                  "WHERE ProductId=? AND SupplierId=?");
        q.addBindValue(ps.purchasePrice);
        q.addBindValue(ps.isPreferred ? 1 : 0);
        q.addBindValue(ps.lastPurchaseDate);
        q.addBindValue(ps.productId);
        q.addBindValue(ps.supplierId);
    } else {
        q.prepare("INSERT INTO ProductSuppliers (ProductId, SupplierId, PurchasePrice, IsPreferred, LastPurchaseDate) "
                  "VALUES (?,?,?,?,?)");
        q.addBindValue(ps.productId);
        q.addBindValue(ps.supplierId);
        q.addBindValue(ps.purchasePrice);
        q.addBindValue(ps.isPreferred ? 1 : 0);
        q.addBindValue(ps.lastPurchaseDate);
    }
    
    return q.exec();
}

bool SQLiteProductSupplierRepository::remove(int productId, int supplierId) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM ProductSuppliers WHERE ProductId=? AND SupplierId=?");
    q.addBindValue(productId);
    q.addBindValue(supplierId);
    return q.exec();
}

std::vector<ProductSupplier> SQLiteProductSupplierRepository::getByProductId(int productId) {
    std::vector<ProductSupplier> result;
    QSqlQuery q(db());
    q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
              "s.Name as SupplierName "
              "FROM ProductSuppliers ps "
              "LEFT JOIN Suppliers s ON ps.SupplierId = s.Id "
              "WHERE ps.ProductId=? "
              "ORDER BY ps.IsPreferred DESC, ps.PurchasePrice ASC");
    q.addBindValue(productId);
    
    if (q.exec()) {
        while (q.next()) {
            ProductSupplier ps;
            ps.productId = q.value(0).toInt();
            ps.supplierId = q.value(1).toInt();
            ps.purchasePrice = q.value(2).toDouble();
            ps.isPreferred = q.value(3).toInt() == 1;
            ps.lastPurchaseDate = q.value(4).toString();
            ps.supplierName = q.value(5).toString();
            result.push_back(ps);
        }
    }
    
    return result;
}

std::vector<ProductSupplier> SQLiteProductSupplierRepository::getBySupplierId(int supplierId) {
    std::vector<ProductSupplier> result;
    QSqlQuery q(db());
    q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
              "p.Name as ProductName "
              "FROM ProductSuppliers ps "
              "LEFT JOIN Products p ON ps.ProductId = p.Id "
              "WHERE ps.SupplierId=? "
              "ORDER BY p.Name ASC");
    q.addBindValue(supplierId);
    
    if (q.exec()) {
        while (q.next()) {
            ProductSupplier ps;
            ps.productId = q.value(0).toInt();
            ps.supplierId = q.value(1).toInt();
            ps.purchasePrice = q.value(2).toDouble();
            ps.isPreferred = q.value(3).toInt() == 1;
            ps.lastPurchaseDate = q.value(4).toString();
            ps.productName = q.value(5).toString();
            result.push_back(ps);
        }
    }
    
    return result;
}

ProductSupplier SQLiteProductSupplierRepository::getPreferredSupplier(int productId) {
    ProductSupplier ps;
    QSqlQuery q(db());
    q.prepare("SELECT ps.ProductId, ps.SupplierId, ps.PurchasePrice, ps.IsPreferred, ps.LastPurchaseDate, "
              "s.Name as SupplierName "
              "FROM ProductSuppliers ps "
              "LEFT JOIN Suppliers s ON ps.SupplierId = s.Id "
              "WHERE ps.ProductId=? AND ps.IsPreferred=1 "
              "LIMIT 1");
    q.addBindValue(productId);
    
    if (q.exec() && q.next()) {
        ps.productId = q.value(0).toInt();
        ps.supplierId = q.value(1).toInt();
        ps.purchasePrice = q.value(2).toDouble();
        ps.isPreferred = q.value(3).toInt() == 1;
        ps.lastPurchaseDate = q.value(4).toString();
        ps.supplierName = q.value(5).toString();
    }
    
    return ps;
}

bool SQLiteProductSupplierRepository::setPreferred(int productId, int supplierId) {
    QSqlDatabase database = db();
    database.transaction();
    
    // First, unset all preferred flags for this product
    QSqlQuery q(database);
    q.prepare("UPDATE ProductSuppliers SET IsPreferred=0 WHERE ProductId=?");
    q.addBindValue(productId);
    if (!q.exec()) {
        database.rollback();
        return false;
    }
    
    // Then set the preferred flag for the specified supplier
    q.prepare("UPDATE ProductSuppliers SET IsPreferred=1 WHERE ProductId=? AND SupplierId=?");
    q.addBindValue(productId);
    q.addBindValue(supplierId);
    if (!q.exec()) {
        database.rollback();
        return false;
    }
    
    database.commit();
    return true;
}

// SQLitePromotionRepository Implementation
// ══════════════════════════════════════════════════════════════════════════════

QList<Promotion> SQLitePromotionRepository::getAll() {
    QList<Promotion> list;
    QSqlQuery q(db());
    if (!q.exec("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
           "ImagePath, ProductId, CategoryId, MinPurchaseAmount FROM Promotions ORDER BY Id DESC")) {
        qDebug() << "SQLitePromotionRepository::getAll() - Query failed:" << q.lastError().text();
        return list;
    }
    
    qDebug() << "SQLitePromotionRepository::getAll() - Query executed successfully";
    while (q.next()) {
        Promotion promo;
        promo.id = q.value(0).toInt();
        promo.title = q.value(1).toString();
        promo.description = q.value(2).toString();
        promo.type = static_cast<Promotion::Type>(q.value(3).toInt());
        promo.discountValue = q.value(4).toDouble();
        promo.startDate = q.value(5).toDateTime();
        promo.endDate = q.value(6).toDateTime();
        promo.status = static_cast<Promotion::Status>(q.value(7).toInt());
        promo.imagePath = q.value(8).toString();
        promo.productId = q.value(9).toInt();
        promo.categoryId = q.value(10).toInt();
        promo.minPurchaseAmount = q.value(11).toDouble();
        list.append(promo);
        qDebug() << "  - Loaded promotion ID:" << promo.id << "Title:" << promo.title;
    }
    
    qDebug() << "SQLitePromotionRepository::getAll() - Total promotions loaded:" << list.size();
    return list;
}

QList<Promotion> SQLitePromotionRepository::getActive() {
    QList<Promotion> list;
    QDateTime now = QDateTime::currentDateTime();
    
    QSqlQuery q(db());
    
    // Convert QDateTime to ISO string format for SQLite
    QString nowStr = now.toString(Qt::ISODate);
    
    q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
              "ImagePath, ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
              "WHERE Status=? AND datetime(StartDate) <= datetime(?) AND datetime(EndDate) >= datetime(?) "
              "ORDER BY Id DESC");
    // NOTE: Promotion::Status enum is { Active=0, Inactive=1, Scheduled=2, Expired=3 }.
    // This query used to hardcode "WHERE Status=1", which is actually Inactive, not
    // Active - so a promotion saved as Active (0, the default selection in the combo
    // box) could NEVER match this query, no matter how correct its dates were. That's
    // why no promotion ever showed up as active in POS, in any scenario, from the start.
    q.addBindValue(static_cast<int>(Promotion::Status::Active));
    q.addBindValue(nowStr);
    q.addBindValue(nowStr);
    
    if (!q.exec()) {
        qDebug() << "SQLitePromotionRepository::getActive() - Query failed:" << q.lastError().text();
        return list;
    }
    
    qDebug() << "SQLitePromotionRepository::getActive() - Query executed, checking results...";
    qDebug() << "   Current time:" << nowStr;
    
    while (q.next()) {
        Promotion promo;
        promo.id = q.value(0).toInt();
        promo.title = q.value(1).toString();
        promo.description = q.value(2).toString();
        promo.type = static_cast<Promotion::Type>(q.value(3).toInt());
        promo.discountValue = q.value(4).toDouble();
        promo.startDate = q.value(5).toDateTime();
        promo.endDate = q.value(6).toDateTime();
        promo.status = static_cast<Promotion::Status>(q.value(7).toInt());
        promo.imagePath = q.value(8).toString();
        promo.productId = q.value(9).toInt();
        promo.categoryId = q.value(10).toInt();
        promo.minPurchaseAmount = q.value(11).toDouble();
        
        qDebug() << "   Found active promotion:" << promo.id << promo.title 
                 << "from" << promo.startDate.toString(Qt::ISODate) 
                 << "to" << promo.endDate.toString(Qt::ISODate);
        
        list.append(promo);
    }
    
    qDebug() << "SQLitePromotionRepository::getActive() - Total active promotions:" << list.size();
    return list;
}

QList<Promotion> SQLitePromotionRepository::getByDateRange(const QDate& start, const QDate& end) {
    QList<Promotion> list;
    QSqlQuery q(db());
    q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
              "ImagePath, ProductId, CategoryId, MinPurchaseAmount FROM Promotions "
              "WHERE date(StartDate) >= date(?) AND date(EndDate) <= date(?) "
              "ORDER BY StartDate DESC");
    // Same ISO-8601 binding fix as save() - a raw QDate would otherwise be
    // bound via its default toString() format, which date() can't parse.
    q.addBindValue(start.toString(Qt::ISODate));
    q.addBindValue(end.toString(Qt::ISODate));
    q.exec();
    
    while (q.next()) {
        Promotion promo;
        promo.id = q.value(0).toInt();
        promo.title = q.value(1).toString();
        promo.description = q.value(2).toString();
        promo.type = static_cast<Promotion::Type>(q.value(3).toInt());
        promo.discountValue = q.value(4).toDouble();
        promo.startDate = q.value(5).toDateTime();
        promo.endDate = q.value(6).toDateTime();
        promo.status = static_cast<Promotion::Status>(q.value(7).toInt());
        promo.imagePath = q.value(8).toString();
        promo.productId = q.value(9).toInt();
        promo.categoryId = q.value(10).toInt();
        promo.minPurchaseAmount = q.value(11).toDouble();
        list.append(promo);
    }
    
    return list;
}

Promotion SQLitePromotionRepository::getById(int id) {
    Promotion promo;
    QSqlQuery q(db());
    q.prepare("SELECT Id, Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
              "ImagePath, ProductId, CategoryId, MinPurchaseAmount FROM Promotions WHERE Id=?");
    q.addBindValue(id);
    q.exec();
    
    if (q.next()) {
        promo.id = q.value(0).toInt();
        promo.title = q.value(1).toString();
        promo.description = q.value(2).toString();
        promo.type = static_cast<Promotion::Type>(q.value(3).toInt());
        promo.discountValue = q.value(4).toDouble();
        promo.startDate = q.value(5).toDateTime();
        promo.endDate = q.value(6).toDateTime();
        promo.status = static_cast<Promotion::Status>(q.value(7).toInt());
        promo.imagePath = q.value(8).toString();
        promo.productId = q.value(9).toInt();
        promo.categoryId = q.value(10).toInt();
        promo.minPurchaseAmount = q.value(11).toDouble();
    }
    
    return promo;
}

bool SQLitePromotionRepository::save(Promotion& promo) {
    // Auto-create table if it doesn't exist
    QSqlQuery createTable(db());
    createTable.exec(
        "CREATE TABLE IF NOT EXISTS Promotions ("
        "Id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Title TEXT NOT NULL, "
        "Description TEXT, "
        "Type INTEGER DEFAULT 0, "
        "DiscountValue REAL DEFAULT 0, "
        "StartDate DATETIME, "
        "EndDate DATETIME, "
        "Status INTEGER DEFAULT 0, "
        "ImagePath TEXT, "
        "ProductId INTEGER, "
        "CategoryId INTEGER, "
        "MinPurchaseAmount REAL DEFAULT 0)"
    );
    
    QSqlQuery q(db());
    
    if (promo.id == 0) {
        // Insert
        q.prepare("INSERT INTO Promotions (Title, Description, Type, DiscountValue, StartDate, EndDate, Status, "
                  "ImagePath, ProductId, CategoryId, MinPurchaseAmount) "
                  "VALUES (?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(promo.title);
        q.addBindValue(promo.description);
        q.addBindValue(static_cast<int>(promo.type));
        q.addBindValue(promo.discountValue);
        // IMPORTANT: bind as an ISO-8601 string, not a raw QDateTime.
        // Qt's SQLite driver has no special case for QDateTime in bound
        // parameters - it falls back to QVariant::toString(), which uses
        // the default Qt::TextDate format ("Fri Sep 18 00:39:00 2026").
        // SQLite's own datetime()/date() SQL functions cannot parse that
        // format, so any WHERE clause using datetime(StartDate) (like in
        // getActive()) silently matches nothing, for every row, forever -
        // even though QDateTime::toDateTime() when reading it back in C++
        // parses it back fine, which is why it looked fine everywhere else.
        q.addBindValue(promo.startDate.toString(Qt::ISODate));
        q.addBindValue(promo.endDate.toString(Qt::ISODate));
        q.addBindValue(static_cast<int>(promo.status));
        q.addBindValue(promo.imagePath);
        q.addBindValue(promo.productId == 0 ? QVariant() : promo.productId);
        q.addBindValue(promo.categoryId == 0 ? QVariant() : promo.categoryId);
        q.addBindValue(promo.minPurchaseAmount);
        
        if (q.exec()) {
            promo.id = q.lastInsertId().toInt();
            return true;
        } else {
            qDebug() << "Failed to insert promotion:" << q.lastError().text();
            return false;
        }
    } else {
        // Update
        q.prepare("UPDATE Promotions SET Title=?, Description=?, Type=?, DiscountValue=?, StartDate=?, EndDate=?, "
                  "Status=?, ImagePath=?, ProductId=?, CategoryId=?, MinPurchaseAmount=? WHERE Id=?");
        q.addBindValue(promo.title);
        q.addBindValue(promo.description);
        q.addBindValue(static_cast<int>(promo.type));
        q.addBindValue(promo.discountValue);
        // Same ISO-8601 fix as the INSERT branch above.
        q.addBindValue(promo.startDate.toString(Qt::ISODate));
        q.addBindValue(promo.endDate.toString(Qt::ISODate));
        q.addBindValue(static_cast<int>(promo.status));
        q.addBindValue(promo.imagePath);
        q.addBindValue(promo.productId == 0 ? QVariant() : promo.productId);
        q.addBindValue(promo.categoryId == 0 ? QVariant() : promo.categoryId);
        q.addBindValue(promo.minPurchaseAmount);
        q.addBindValue(promo.id);
        
        if (!q.exec()) {
            qDebug() << "Failed to update promotion:" << q.lastError().text();
            return false;
        }
        return true;
    }
}

bool SQLitePromotionRepository::remove(int id) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM Promotions WHERE Id=?");
    q.addBindValue(id);
    return q.exec();
}

bool SQLitePromotionRepository::addProductToPromotion(int promotionId, int productId) {
    // Auto-create PromotionProducts table if it doesn't exist
    QSqlQuery createTable(db());
    createTable.exec(
        "CREATE TABLE IF NOT EXISTS PromotionProducts ("
        "PromotionId INTEGER NOT NULL, "
        "ProductId INTEGER NOT NULL, "
        "MagazinePrice REAL DEFAULT 0, "
        "PRIMARY KEY (PromotionId, ProductId), "
        "FOREIGN KEY (PromotionId) REFERENCES Promotions(Id) ON DELETE CASCADE, "
        "FOREIGN KEY (ProductId) REFERENCES Products(Id) ON DELETE CASCADE)"
    );
    
    // Use INSERT OR IGNORE to avoid errors if the row already exists
    QSqlQuery q(db());
    q.prepare("INSERT OR IGNORE INTO PromotionProducts (PromotionId, ProductId, MagazinePrice) VALUES (?, ?, 0)");
    q.addBindValue(promotionId);
    q.addBindValue(productId);
    return q.exec();
}

bool SQLitePromotionRepository::removeProductFromPromotion(int promotionId, int productId) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM PromotionProducts WHERE PromotionId=? AND ProductId=?");
    q.addBindValue(promotionId);
    q.addBindValue(productId);
    return q.exec();
}

bool SQLitePromotionRepository::removeAllProductsFromPromotion(int promotionId) {
    QSqlQuery q(db());
    q.prepare("DELETE FROM PromotionProducts WHERE PromotionId=?");
    q.addBindValue(promotionId);
    return q.exec();
}

QList<int> SQLitePromotionRepository::getProductsForPromotion(int promotionId) {
    QList<int> productIds;
    QSqlQuery q(db());
    q.prepare("SELECT ProductId FROM PromotionProducts WHERE PromotionId=?");
    q.addBindValue(promotionId);
    
    if (q.exec()) {
        while (q.next()) {
            productIds.append(q.value(0).toInt());
        }
    }
    
    return productIds;
}

bool SQLitePromotionRepository::setProductMagazinePrice(int promotionId, int productId, double magazinePrice) {
    QSqlQuery q(db());
    // Use INSERT OR REPLACE to ensure the price is set even if the row doesn't exist yet
    q.prepare("INSERT OR REPLACE INTO PromotionProducts (PromotionId, ProductId, MagazinePrice) VALUES (?, ?, ?)");
    q.addBindValue(promotionId);
    q.addBindValue(productId);
    q.addBindValue(magazinePrice);
    
    bool success = q.exec();
    if (success) {
        qDebug() << "✅ Set magazine price for promotion" << promotionId << "product" << productId << "=" << magazinePrice;
    } else {
        qDebug() << "❌ Failed to set magazine price:" << q.lastError().text();
    }
    return success;
}

double SQLitePromotionRepository::getProductMagazinePrice(int promotionId, int productId) {
    QSqlQuery q(db());
    q.prepare("SELECT MagazinePrice FROM PromotionProducts WHERE PromotionId=? AND ProductId=?");
    q.addBindValue(promotionId);
    q.addBindValue(productId);
    
    if (q.exec() && q.next()) {
        return q.value(0).toDouble();
    }
    
    return 0.0;
}
