#include "database_connection_manager.h"
#include "config_manager.h"
#include "logger.h"
#include "schema_deployer.h"
#include "../services/branch_manager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTimer>
#include <QObject>

DatabaseConnectionManager& DatabaseConnectionManager::instance() {
    static DatabaseConnectionManager inst;
    return inst;
}

bool DatabaseConnectionManager::openConnection() {
    m_sqliteFallbackActive = false;
    m_lastError = "";

    // ── Multi-branch: use the active branch's DB path ──────────────────────
    BranchInfo activeBranch = BranchManager::instance().activeBranch();
    QString branchDbPath    = activeBranch.dbPath;
    bool    isCloud         = activeBranch.isCloud
                              || branchDbPath.startsWith("postgresql://")
                              || branchDbPath.startsWith("postgres://");

    if (isCloud) {
        Logger::instance().info("Connecting to Supabase/PostgreSQL: " + activeBranch.name);
        if (openPostgresConnection(branchDbPath)) {
            Logger::instance().info("PostgreSQL connected for branch: " + activeBranch.name);
            m_isCloudConnection = true;
            startConnectionMonitor();  // Start health monitoring for cloud connections
            return initializeSchema();
        } else {
            Logger::instance().error("PostgreSQL connection failed: " + m_lastError);
            Logger::instance().warn("Falling back to SQLite...");
            m_sqliteFallbackActive = true;
            m_isCloudConnection = false;
            if (openSqliteConnection()) {
                Logger::instance().info("SQLite fallback connected.");
                return initializeSchema();
            }
            return false;
        }
    }

    // Override SQLite path to branch-specific file if provided.
    // Only overwrite when branchDbPath is a non-empty, non-postgres path.
    // If branchDbPath is a bare relative filename (e.g. "delivery_system.db")
    // that means the branch was created before a writable data dir was set —
    // in that case we leave ConfigManager's current (already-resolved absolute)
    // path untouched so the Program Files fix is not clobbered.
    if (!branchDbPath.isEmpty() && !branchDbPath.startsWith("postgresql://")) {
        QFileInfo fi(branchDbPath);
        if (fi.isAbsolute()) {
            // Explicit absolute path from config — honour it as-is
            ConfigManager::instance().setSqlitePath(branchDbPath);
        } else {
            // Relative/bare filename: do NOT overwrite the resolved absolute
            // path that main() already set.  Just log and move on.
            Logger::instance().info(
                "Branch dbPath is relative (\"" + branchDbPath +
                "\") — keeping resolved path: " +
                ConfigManager::instance().sqlitePath());
        }
    }

    QString configType = ConfigManager::instance().databaseType();

    if (configType.compare("ODBC", Qt::CaseInsensitive) == 0) {
        Logger::instance().info("Attempting ODBC connection...");
        if (openOdbcConnection()) {
            Logger::instance().info("ODBC connected.");
            return initializeSchema();
        } else {
            Logger::instance().error("ODBC failed: " + m_lastError);
            m_sqliteFallbackActive = true;
            if (openSqliteConnection()) {
                Logger::instance().info("SQLite fallback connected.");
                return initializeSchema();
            }
            return false;
        }
    } else {
        Logger::instance().info("Connecting to SQLite for branch: " + activeBranch.name);
        if (openSqliteConnection()) {
            Logger::instance().info("SQLite connected.");
            return initializeSchema();
        } else {
            Logger::instance().error("SQLite failed: " + m_lastError);
            return false;
        }
    }
}

void DatabaseConnectionManager::closeConnection() {
    stopConnectionMonitor();  // Stop health checks
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) {
            db.close();
            Logger::instance().info("Database connection closed.");
        }
    }
    m_isCloudConnection = false;
}

bool DatabaseConnectionManager::isSqliteFallbackActive() const {
    return m_sqliteFallbackActive;
}

QString DatabaseConnectionManager::lastError() const {
    return m_lastError;
}

QString DatabaseConnectionManager::connectionType() const {
    if (QSqlDatabase::contains(m_connectionName)) {
        return QSqlDatabase::database(m_connectionName).driverName();
    }
    return "";
}

bool DatabaseConnectionManager::openPostgresConnection(const QString& connStr) {
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", m_connectionName);

    // Parse postgresql://user:password@host:port/dbname
    // Qt QPSQL accepts individual fields
    QRegularExpression re(R"(postgres(?:ql)?://([^:]+):([^@]+)@([^:/]+):?(\d*)/(.+))");
    auto match = re.match(connStr);
    if (match.hasMatch()) {
        db.setUserName(match.captured(1));
        db.setPassword(match.captured(2));
        db.setHostName(match.captured(3));
        db.setPort(match.captured(4).isEmpty() ? 5432 : match.captured(4).toInt());
        db.setDatabaseName(match.captured(5));
        // Supabase requires SSL. Use sslmode=require (encrypts the connection
        // without verifying the server certificate — sufficient for Supabase
        // pooler and avoids libpq cert-path issues on Windows).
        db.setConnectOptions("sslmode=require");
    } else {
        // Try passing the full URI as database name (some drivers accept it)
        db.setDatabaseName(connStr);
    }

    if (!db.open()) {
        QString rawErr = db.lastError().text();
        if (rawErr.contains("could not translate host name", Qt::CaseInsensitive)
            || rawErr.contains("Name or service not known", Qt::CaseInsensitive)
            || rawErr.contains("nodename nor servname", Qt::CaseInsensitive))
        {
            m_lastError =
                "Could not resolve Supabase host (IPv6-only host, IPv4 network). "
                "Use the Session Pooler URL from Supabase Dashboard → Connect → "
                "\"Session pooler\" tab (aws-0-<region>.pooler.supabase.com:5432).";
        } else {
            m_lastError = rawErr;
        }
        return false;
    }

    // Set search_path to the branch schema so all queries hit the right schema.
    // Use SET SESSION to make it persistent across transactions and the entire connection lifetime.
    // IMPORTANT: this is derived from the branch's *name* (via the same
    // sanitizer used at deploy time), not from activeBranchId() — the local
    // numeric id is assigned independently per device/config.ini and is
    // NOT shared across machines, so two devices for the "same" branch could
    // end up with different ids and therefore different (wrong) schemas.
    // The branch name, typed identically on every device, is the one thing
    // that's actually shared, so it's the only safe key to derive the
    // schema from.
    {
        QSqlQuery q(db);
        QString schemaName = SchemaDeployer::sanitizeSchemaName(BranchManager::instance().activeBranch().name);
        QString safe = schemaName;
        safe.replace('"', "\"\"");
        bool ok = q.exec(QString("SET SESSION search_path TO \"%1\"").arg(safe));
        if (!ok) {
            Logger::instance().error(QString("Failed to set search_path: %1").arg(q.lastError().text()));
            m_lastError = "Failed to set schema search_path: " + q.lastError().text();
            return false;
        }
        Logger::instance().info(QString("SET SESSION search_path TO \"%1\" — successful").arg(schemaName));
    }

    return true;
}

bool DatabaseConnectionManager::openOdbcConnection() {
    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connectionName)) {
        db = QSqlDatabase::database(m_connectionName);
    } else {
        db = QSqlDatabase::addDatabase("QODBC", m_connectionName);
    }

    QString connStr = ConfigManager::instance().odbcConnectionString();
    db.setDatabaseName(connStr);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseConnectionManager::openSqliteConnection() {
    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connectionName)) {
        db = QSqlDatabase::database(m_connectionName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    QString sqlitePath = ConfigManager::instance().sqlitePath();
    
    // Ensure parent directory exists
    QFileInfo fileInfo(sqlitePath);
    QDir().mkpath(fileInfo.absolutePath());

    db.setDatabaseName(sqlitePath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseConnectionManager::initializeSchema() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    QString driverName = db.driverName();
    QStringList tables = db.tables();
    QSqlQuery query(db);

    Logger::instance().info("Initializing database schema on driver: " + driverName);

    // Helper lambda to run table creation query if table does not exist
    auto createTable = [&](const QString& tableName, const QString& sqliteDDL, const QString& odbcDDL) {
        if (tables.contains(tableName, Qt::CaseInsensitive)) {
            return true;
        }
        
        QString ddl = (driverName == "QSQLITE") ? sqliteDDL : odbcDDL;
        // ODBC DDL uses MS Access types — replace with PostgreSQL equivalents
        if (driverName == "QPSQL") {
            ddl.replace("COUNTER PRIMARY KEY", "SERIAL PRIMARY KEY");
            ddl.replace(QRegularExpression("\\bDOUBLE\\b"), "DOUBLE PRECISION");
            ddl.replace(" MEMO", " TEXT");
        }
        if (!query.exec(ddl)) {
            m_lastError = QString("Failed to create table %1: %2").arg(tableName, query.lastError().text());
            Logger::instance().error(m_lastError);
            return false;
        }
        Logger::instance().info(QString("Table %1 created successfully.").arg(tableName));
        return true;
    };

    // 1. Branches
    if (!createTable("Branches",
        "CREATE TABLE Branches (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Address TEXT)",
        "CREATE TABLE Branches (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL, Address VARCHAR(255))"))
        return false;

    // 2. Categories
    if (!createTable("Categories",
        "CREATE TABLE Categories (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE Categories (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL UNIQUE)"))
        return false;

    // 3. Regions
    if (!createTable("Regions",
        "CREATE TABLE Regions (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE Regions (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL UNIQUE)"))
        return false;

    // 4. Roles
    if (!createTable("Roles",
        "CREATE TABLE Roles (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE, "
        "CanManageUsers INTEGER DEFAULT 0, CanViewReports INTEGER DEFAULT 0, "
        "CanAddCustomer INTEGER DEFAULT 0, CanEditCustomer INTEGER DEFAULT 0, CanDeleteCustomer INTEGER DEFAULT 0, "
        "CanAddProduct INTEGER DEFAULT 0, CanEditProduct INTEGER DEFAULT 0, CanDeleteProduct INTEGER DEFAULT 0, "
        "CanAddOrder INTEGER DEFAULT 0, CanEditOrder INTEGER DEFAULT 0, CanCancelOrder INTEGER DEFAULT 0, "
        "CanDeleteOrders INTEGER DEFAULT 0, CanManageRegions INTEGER DEFAULT 0, CanAccessSettings INTEGER DEFAULT 0, "
        "CanEditOrderDate INTEGER DEFAULT 0, CanAccessDashboard INTEGER DEFAULT 1, "
        "CanManagePurchases INTEGER DEFAULT 0, CanAccessPOS INTEGER DEFAULT 0, CanManageRegister INTEGER DEFAULT 0, "
        "CanManageExpenses INTEGER DEFAULT 0, CanManageInventory INTEGER DEFAULT 0)",
        "CREATE TABLE Roles (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL UNIQUE, "
        "CanManageUsers INTEGER DEFAULT 0, CanViewReports INTEGER DEFAULT 0, "
        "CanAddCustomer INTEGER DEFAULT 0, CanEditCustomer INTEGER DEFAULT 0, CanDeleteCustomer INTEGER DEFAULT 0, "
        "CanAddProduct INTEGER DEFAULT 0, CanEditProduct INTEGER DEFAULT 0, CanDeleteProduct INTEGER DEFAULT 0, "
        "CanAddOrder INTEGER DEFAULT 0, CanEditOrder INTEGER DEFAULT 0, CanCancelOrder INTEGER DEFAULT 0, "
        "CanDeleteOrders INTEGER DEFAULT 0, CanManageRegions INTEGER DEFAULT 0, CanAccessSettings INTEGER DEFAULT 0, "
        "CanEditOrderDate INTEGER DEFAULT 0, CanAccessDashboard INTEGER DEFAULT 1, "
        "CanManagePurchases INTEGER DEFAULT 0, CanAccessPOS INTEGER DEFAULT 0, CanManageRegister INTEGER DEFAULT 0, "
        "CanManageExpenses INTEGER DEFAULT 0, CanManageInventory INTEGER DEFAULT 0)"))
        return false;

    // 5. Users
    if (!createTable("Users",
        "CREATE TABLE Users (Id INTEGER PRIMARY KEY AUTOINCREMENT, Username TEXT NOT NULL UNIQUE, "
        "PasswordHash TEXT NOT NULL, PasswordSalt TEXT NOT NULL, RoleId INTEGER, Name TEXT, Phone TEXT, Email TEXT, "
        "FOREIGN KEY(RoleId) REFERENCES Roles(Id))",
        "CREATE TABLE Users (Id COUNTER PRIMARY KEY, Username VARCHAR(255) NOT NULL UNIQUE, "
        "PasswordHash VARCHAR(255) NOT NULL, PasswordSalt VARCHAR(255) NOT NULL, RoleId INTEGER, Name VARCHAR(255), Phone VARCHAR(255), Email VARCHAR(255))"))
        return false;

    // 6. Customers
    if (!createTable("Customers",
        "CREATE TABLE Customers (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, DistanceKm REAL DEFAULT 0.0, "
        "RegionId INTEGER, Notes TEXT, FirstContactDate TEXT, LastOrderDate TEXT, Status TEXT DEFAULT 'Active', "
        "PreferredPaymentMethod TEXT DEFAULT '', PreferredPaymentOther TEXT DEFAULT '', Debt REAL DEFAULT 0.0, "
        "FOREIGN KEY(RegionId) REFERENCES Regions(Id))",
        "CREATE TABLE Customers (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL, DistanceKm DOUBLE DEFAULT 0.0, "
        "RegionId INTEGER, Notes MEMO, FirstContactDate VARCHAR(255), LastOrderDate VARCHAR(255), Status VARCHAR(50) DEFAULT 'Active', "
        "PreferredPaymentMethod VARCHAR(50) DEFAULT '', PreferredPaymentOther VARCHAR(255) DEFAULT '', Debt DOUBLE DEFAULT 0.0)"))
        return false;

    // 7. CustomerPhones
    if (!createTable("CustomerPhones",
        "CREATE TABLE CustomerPhones (CustomerId INTEGER, PhoneNumber TEXT, PRIMARY KEY(CustomerId, PhoneNumber), "
        "FOREIGN KEY(CustomerId) REFERENCES Customers(Id) ON DELETE CASCADE)",
        "CREATE TABLE CustomerPhones (CustomerId INTEGER, PhoneNumber VARCHAR(255), PRIMARY KEY(CustomerId, PhoneNumber))"))
        return false;

    // 7.5. ProductSuppliers (Task #4: Multiple suppliers per product)
    if (!createTable("ProductSuppliers",
        "CREATE TABLE ProductSuppliers (ProductId INTEGER, SupplierId INTEGER, PurchasePrice REAL DEFAULT 0, "
        "IsPreferred INTEGER DEFAULT 0, LastPurchaseDate TEXT, "
        "PRIMARY KEY(ProductId, SupplierId), "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id) ON DELETE CASCADE, "
        "FOREIGN KEY(SupplierId) REFERENCES Suppliers(Id) ON DELETE CASCADE)",
        "CREATE TABLE ProductSuppliers (ProductId INTEGER, SupplierId INTEGER, PurchasePrice DOUBLE DEFAULT 0, "
        "IsPreferred INTEGER DEFAULT 0, LastPurchaseDate VARCHAR(255), "
        "PRIMARY KEY(ProductId, SupplierId))"))
        return false;

    // 8. CustomerAddresses
    if (!createTable("CustomerAddresses",
        "CREATE TABLE CustomerAddresses (CustomerId INTEGER, AddressText TEXT, PRIMARY KEY(CustomerId, AddressText), "
        "FOREIGN KEY(CustomerId) REFERENCES Customers(Id) ON DELETE CASCADE)",
        "CREATE TABLE CustomerAddresses (CustomerId INTEGER, AddressText VARCHAR(255), PRIMARY KEY(CustomerId, AddressText))"))
        return false;

    // 9. Products
    if (!createTable("Products",
        "CREATE TABLE Products (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Barcode TEXT, Price REAL NOT NULL, "
        "CategoryId INTEGER, Status TEXT DEFAULT 'Active', ManufactureDate TEXT, ExpiryDate TEXT, "
        "StockQty INTEGER DEFAULT 0, LowStockThreshold INTEGER DEFAULT 5, CostPrice REAL DEFAULT 0, UnitLabel TEXT DEFAULT 'وحدة', "
        "ImagePath TEXT, "
        "FOREIGN KEY(CategoryId) REFERENCES Categories(Id))",
        "CREATE TABLE Products (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL, Barcode VARCHAR(255), Price DOUBLE NOT NULL, "
        "CategoryId INTEGER, Status VARCHAR(50) DEFAULT 'Active', ManufactureDate VARCHAR(20), ExpiryDate VARCHAR(20), "
        "StockQty INTEGER DEFAULT 0, LowStockThreshold INTEGER DEFAULT 5, CostPrice DOUBLE DEFAULT 0, UnitLabel VARCHAR(50) DEFAULT 'وحدة', "
        "ImagePath VARCHAR(500))"))
        return false;

    // 10. CustomerFavorites
    if (!createTable("CustomerFavorites",
        "CREATE TABLE CustomerFavorites (CustomerId INTEGER, ProductId INTEGER, PRIMARY KEY(CustomerId, ProductId), "
        "FOREIGN KEY(CustomerId) REFERENCES Customers(Id) ON DELETE CASCADE, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id) ON DELETE CASCADE)",
        "CREATE TABLE CustomerFavorites (CustomerId INTEGER, ProductId INTEGER, PRIMARY KEY(CustomerId, ProductId))"))
        return false;

    // 11. ProductPriceHistory
    if (!createTable("ProductPriceHistory",
        "CREATE TABLE ProductPriceHistory (Id INTEGER PRIMARY KEY AUTOINCREMENT, ProductId INTEGER, Price REAL NOT NULL, EffectiveDate TEXT NOT NULL, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id) ON DELETE CASCADE)",
        "CREATE TABLE ProductPriceHistory (Id COUNTER PRIMARY KEY, ProductId INTEGER, Price DOUBLE NOT NULL, EffectiveDate VARCHAR(255))"))
        return false;

    // 12. Orders
    if (!createTable("Orders",
        "CREATE TABLE Orders (Id INTEGER PRIMARY KEY AUTOINCREMENT, CustomerId INTEGER, RegisterSessionId INTEGER, DateTime TEXT NOT NULL, "
        "DeliveryFee REAL DEFAULT 0.0, Subtotal REAL DEFAULT 0.0, GrandTotal REAL DEFAULT 0.0, "
        "Status TEXT DEFAULT 'Pending', CancelReason TEXT, DriverId INTEGER DEFAULT 0, DriverName TEXT, "
        "DiscountAmount REAL DEFAULT 0.0, DiscountReason TEXT, PaymentMethod TEXT DEFAULT 'Cash', "
        "FOREIGN KEY(CustomerId) REFERENCES Customers(Id), FOREIGN KEY(RegisterSessionId) REFERENCES RegisterSessions(Id))",
        "CREATE TABLE Orders (Id COUNTER PRIMARY KEY, CustomerId INTEGER, RegisterSessionId INTEGER, DateTime VARCHAR(255) NOT NULL, "
        "DeliveryFee DOUBLE DEFAULT 0.0, Subtotal DOUBLE DEFAULT 0.0, GrandTotal DOUBLE DEFAULT 0.0, "
        "Status VARCHAR(50) DEFAULT 'Pending', CancelReason MEMO, DriverId INTEGER DEFAULT 0, DriverName VARCHAR(255), "
        "DiscountAmount DOUBLE DEFAULT 0.0, DiscountReason VARCHAR(255), PaymentMethod VARCHAR(50) DEFAULT 'Cash')"))
        return false;

    // 13. OrderItems
    if (!createTable("OrderItems",
        "CREATE TABLE OrderItems (OrderId INTEGER, ProductId INTEGER, Quantity INTEGER NOT NULL, UnitPrice REAL NOT NULL, "
        "PRIMARY KEY(OrderId, ProductId), FOREIGN KEY(OrderId) REFERENCES Orders(Id) ON DELETE CASCADE, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id))",
        "CREATE TABLE OrderItems (OrderId INTEGER, ProductId INTEGER, Quantity INTEGER NOT NULL, UnitPrice DOUBLE NOT NULL, PRIMARY KEY(OrderId, ProductId))"))
        return false;

    // 14. ScheduledOrders
    if (!createTable("ScheduledOrders",
        "CREATE TABLE ScheduledOrders (Id INTEGER PRIMARY KEY AUTOINCREMENT, CustomerId INTEGER, Weekdays TEXT NOT NULL, "
        "Time TEXT NOT NULL, FOREIGN KEY(CustomerId) REFERENCES Customers(Id))",
        "CREATE TABLE ScheduledOrders (Id COUNTER PRIMARY KEY, CustomerId INTEGER, Weekdays VARCHAR(255) NOT NULL, Time VARCHAR(255) NOT NULL)"))
        return false;

    // 15. ScheduledOrderItems
    if (!createTable("ScheduledOrderItems",
        "CREATE TABLE ScheduledOrderItems (ScheduledOrderId INTEGER, ProductId INTEGER, Quantity INTEGER NOT NULL, "
        "PRIMARY KEY(ScheduledOrderId, ProductId), FOREIGN KEY(ScheduledOrderId) REFERENCES ScheduledOrders(Id) ON DELETE CASCADE, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id))",
        "CREATE TABLE ScheduledOrderItems (ScheduledOrderId INTEGER, ProductId INTEGER, Quantity INTEGER NOT NULL, PRIMARY KEY(ScheduledOrderId, ProductId))"))
        return false;

    // 16. MissingProducts
    if (!createTable("MissingProducts",
        "CREATE TABLE MissingProducts (Id INTEGER PRIMARY KEY AUTOINCREMENT, ProductId INTEGER, Barcode TEXT, "
        "QuantityNeeded INTEGER NOT NULL, BranchId INTEGER, DateAdded TEXT NOT NULL, Purchased INTEGER DEFAULT 0, "
        "Name TEXT DEFAULT '', Source TEXT DEFAULT 'manual', CurrentQty INTEGER DEFAULT 0, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id), FOREIGN KEY(BranchId) REFERENCES Branches(Id))",
        "CREATE TABLE MissingProducts (Id COUNTER PRIMARY KEY, ProductId INTEGER, Barcode VARCHAR(255), "
        "QuantityNeeded INTEGER NOT NULL, BranchId INTEGER, DateAdded VARCHAR(255) NOT NULL, Purchased INTEGER DEFAULT 0, "
        "Name VARCHAR(255) DEFAULT '', Source VARCHAR(50) DEFAULT 'manual', CurrentQty INTEGER DEFAULT 0)"))
        return false;

    // 17. AuditLogs
    if (!createTable("AuditLogs",
        "CREATE TABLE AuditLogs (Id INTEGER PRIMARY KEY AUTOINCREMENT, UserId INTEGER, Action TEXT NOT NULL, "
        "EntityType TEXT, EntityId INTEGER, Details TEXT, Timestamp TEXT NOT NULL, FOREIGN KEY(UserId) REFERENCES Users(Id))",
        "CREATE TABLE AuditLogs (Id COUNTER PRIMARY KEY, UserId INTEGER, Action VARCHAR(255) NOT NULL, EntityType VARCHAR(255), EntityId INTEGER, Details MEMO, Timestamp VARCHAR(255) NOT NULL)"))
        return false;

    // ── Schema Migrations for existing databases ──────────────────────────────
    // ALTER TABLE is safe to call even if column exists — we ignore errors
    if (driverName == "QSQLITE") {
        query.exec("ALTER TABLE Products ADD COLUMN Barcode TEXT");
        query.exec("ALTER TABLE Products ADD COLUMN ManufactureDate TEXT");
        query.exec("ALTER TABLE Products ADD COLUMN ExpiryDate TEXT");
        query.exec("ALTER TABLE Products ADD COLUMN StockQty INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Products ADD COLUMN LowStockThreshold INTEGER DEFAULT 5");
        query.exec("ALTER TABLE Products ADD COLUMN CostPrice REAL DEFAULT 0");
        query.exec("ALTER TABLE Products ADD COLUMN UnitLabel TEXT DEFAULT 'وحدة'");
        query.exec("ALTER TABLE Products ADD COLUMN ImagePath TEXT");
        query.exec("ALTER TABLE Products ADD COLUMN HasTax INTEGER DEFAULT 0");
        // Product type (Solid/Weighted) and PLU barcode
        query.exec("ALTER TABLE Products ADD COLUMN Type TEXT DEFAULT 'Solid'");
        query.exec("ALTER TABLE Products ADD COLUMN PluBarcode TEXT");
        // MissingProducts name column (added to support freehand-typed names)
        query.exec("ALTER TABLE MissingProducts ADD COLUMN Name TEXT DEFAULT ''");
        query.exec("ALTER TABLE Orders ADD COLUMN Status TEXT DEFAULT 'Pending'");
        query.exec("ALTER TABLE Orders ADD COLUMN CancelReason TEXT");
        query.exec("ALTER TABLE Orders ADD COLUMN DriverId INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Orders ADD COLUMN DriverName TEXT");
        query.exec("ALTER TABLE Orders ADD COLUMN DiscountAmount REAL DEFAULT 0.0");
        query.exec("ALTER TABLE Orders ADD COLUMN DiscountReason TEXT");
        query.exec("ALTER TABLE Orders ADD COLUMN PaymentMethod TEXT DEFAULT 'Cash'");
        query.exec("ALTER TABLE Orders ADD COLUMN RegisterSessionId INTEGER");
        query.exec("ALTER TABLE Orders ADD COLUMN InvoiceBarcode TEXT");
        // Expenses payment method
        query.exec("ALTER TABLE Expenses ADD COLUMN PaymentMethod TEXT DEFAULT 'Cash'");
        // User attendance fields
        query.exec("ALTER TABLE Users ADD COLUMN PhotoPath TEXT");
        query.exec("ALTER TABLE Users ADD COLUMN FingerprintBarcode TEXT");
        query.exec("ALTER TABLE Users ADD COLUMN HourlyRate REAL DEFAULT 0.0");
        query.exec("ALTER TABLE Users ADD COLUMN WeeklyOffDays INTEGER DEFAULT 1");
        query.exec("ALTER TABLE Users ADD COLUMN Email TEXT");
        // Granular role permissions (added in v2)
        query.exec("ALTER TABLE Roles ADD COLUMN CanAddCustomer INTEGER DEFAULT 0");        query.exec("ALTER TABLE Roles ADD COLUMN CanEditCustomer INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanDeleteCustomer INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanAddProduct INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanEditProduct INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanDeleteProduct INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanAddOrder INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanEditOrder INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanCancelOrder INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanViewReports INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanManageRegions INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanAccessSettings INTEGER DEFAULT 0");
        // Phase 1 permissions
        query.exec("ALTER TABLE Roles ADD COLUMN CanManagePurchases INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanAccessPOS INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanManageRegister INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanManageExpenses INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanManageInventory INTEGER DEFAULT 0");
        // v4 migrations — safe to re-run (errors silently ignored)
        query.exec("ALTER TABLE Customers ADD COLUMN PreferredPaymentMethod TEXT DEFAULT ''");
        query.exec("ALTER TABLE Customers ADD COLUMN PreferredPaymentOther TEXT DEFAULT ''");
        query.exec("ALTER TABLE Customers ADD COLUMN Debt REAL DEFAULT 0.0");
        query.exec("ALTER TABLE ScheduledOrders ADD COLUMN RemindMinutesBefore INTEGER DEFAULT 60");

        // Task 11 — soft-delete archive table (also created on-demand by ArchiveManager)
        // Use SERIAL for PostgreSQL, AUTOINCREMENT for SQLite
        if (driverName == "QPSQL") {
            query.exec(
                "CREATE TABLE IF NOT EXISTS \"DeletedRecordsArchive\" ("
                "  id           SERIAL PRIMARY KEY,"
                "  table_name   TEXT   NOT NULL,"
                "  record_id    TEXT   NOT NULL,"
                "  record_data  TEXT   NOT NULL,"
                "  deleted_at   TEXT   NOT NULL,"
                "  deleted_by   TEXT,"
                "  restored_at  TEXT,"
                "  restored_by  TEXT"
                ")");
        } else {
            query.exec(
                "CREATE TABLE IF NOT EXISTS DeletedRecordsArchive ("
                "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  table_name   TEXT    NOT NULL,"
                "  record_id    TEXT    NOT NULL,"
                "  record_data  TEXT    NOT NULL,"
                "  deleted_at   TEXT    NOT NULL,"
                "  deleted_by   TEXT,"
                "  restored_at  TEXT,"
                "  restored_by  TEXT"
                ")");
        }
        query.exec("ALTER TABLE ScheduledOrders ADD COLUMN RemindRepeatInterval INTEGER DEFAULT 0");
        query.exec("ALTER TABLE ScheduledOrders ADD COLUMN AutoCreateOrder INTEGER DEFAULT 0");
        // Task B migrations — MissingProducts new columns
        query.exec("ALTER TABLE MissingProducts ADD COLUMN Source TEXT DEFAULT 'manual'");
        query.exec("ALTER TABLE MissingProducts ADD COLUMN CurrentQty INTEGER DEFAULT 0");
        // v2.0.0 migrations
        query.exec("ALTER TABLE Roles ADD COLUMN CanEditOrderDate INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Roles ADD COLUMN CanAccessDashboard INTEGER DEFAULT 1");
    }

    // 18. OrderStatuses (custom statuses — Admin only)
    if (!createTable("OrderStatuses",
        "CREATE TABLE OrderStatuses (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE)",
        "CREATE TABLE OrderStatuses (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL UNIQUE)"))
        return false;

    // Seed default order statuses if table is empty
    {
        QSqlQuery cnt("SELECT COUNT(*) FROM OrderStatuses", db);
        if (cnt.next() && cnt.value(0).toInt() == 0) {
            for (const QString& s : {"Pending","Out for Delivery","Delivered","Cancelled"})
                QSqlQuery(QString("INSERT INTO OrderStatuses (Name) VALUES ('%1')").arg(s), db);
        }
    }

    // 19. DeliveryDrivers
    if (!createTable("DeliveryDrivers",
        "CREATE TABLE DeliveryDrivers (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, "
        "Phone TEXT, NationalId TEXT, Active INTEGER DEFAULT 1)",
        "CREATE TABLE DeliveryDrivers (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL, "
        "Phone VARCHAR(255), NationalId VARCHAR(255), Active INTEGER DEFAULT 1)"))
        return false;

    // Migration: add DriverId to Orders if not present
    if (driverName == "QSQLITE") {
        query.exec("ALTER TABLE Orders ADD COLUMN DriverId INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Orders ADD COLUMN DriverName TEXT");
        query.exec("ALTER TABLE Orders ADD COLUMN DiscountAmount REAL DEFAULT 0.0");
        query.exec("ALTER TABLE Orders ADD COLUMN DiscountReason TEXT");
        query.exec("ALTER TABLE Orders ADD COLUMN PaymentMethod TEXT DEFAULT 'Cash'");
        query.exec("ALTER TABLE Orders ADD COLUMN PaymentOtherDetail TEXT DEFAULT ''");
        query.exec("ALTER TABLE Orders ADD COLUMN InvoiceNumber INTEGER DEFAULT 0");
        query.exec("ALTER TABLE Orders ADD COLUMN RegisterSessionId INTEGER");
        query.exec("ALTER TABLE Orders ADD COLUMN InvoiceBarcode TEXT");
    }
    if (!createTable("Coupons",
        "CREATE TABLE Coupons (Id INTEGER PRIMARY KEY AUTOINCREMENT, Code TEXT NOT NULL UNIQUE, "
        "Description TEXT, Type TEXT DEFAULT 'FixedAmount', Value REAL DEFAULT 0.0, "
        "ExpiryDate TEXT, MaxUses INTEGER DEFAULT 0, UsedCount INTEGER DEFAULT 0, Active INTEGER DEFAULT 1)",
        "CREATE TABLE Coupons (Id COUNTER PRIMARY KEY, Code VARCHAR(50) NOT NULL, "
        "Description VARCHAR(255), Type VARCHAR(50) DEFAULT 'FixedAmount', Value DOUBLE DEFAULT 0.0, "
        "ExpiryDate VARCHAR(20), MaxUses INTEGER DEFAULT 0, UsedCount INTEGER DEFAULT 0, Active INTEGER DEFAULT 1)"))
        return false;

    // 21. OrderReturns + ReturnItems
    if (!createTable("OrderReturns",
        "CREATE TABLE OrderReturns (Id INTEGER PRIMARY KEY AUTOINCREMENT, OrderId INTEGER NOT NULL, "
        "CustomerId INTEGER, CustomerName TEXT, Reason TEXT, DateTime TEXT, IsFullReturn INTEGER DEFAULT 1, "
        "FOREIGN KEY(OrderId) REFERENCES Orders(Id))",
        "CREATE TABLE OrderReturns (Id COUNTER PRIMARY KEY, OrderId INTEGER NOT NULL, "
        "CustomerId INTEGER, CustomerName VARCHAR(255), Reason MEMO, DateTime VARCHAR(255), IsFullReturn INTEGER DEFAULT 1)"))
        return false;

    if (!createTable("ReturnItems",
        "CREATE TABLE ReturnItems (Id INTEGER PRIMARY KEY AUTOINCREMENT, ReturnId INTEGER NOT NULL, "
        "ProductId INTEGER, ProductName TEXT, QuantityReturned INTEGER, UnitPrice REAL, "
        "FOREIGN KEY(ReturnId) REFERENCES OrderReturns(Id) ON DELETE CASCADE)",
        "CREATE TABLE ReturnItems (Id COUNTER PRIMARY KEY, ReturnId INTEGER NOT NULL, "
        "ProductId INTEGER, ProductName VARCHAR(255), QuantityReturned INTEGER, UnitPrice DOUBLE)"))
        return false;

    // ══════════════════════════════════════════════════════════════════════
    // Phase 1: Purchases + POS + Register + Expenses + Inventory Tables
    // ══════════════════════════════════════════════════════════════════════

    // 23. Suppliers
    if (!createTable("Suppliers",
        "CREATE TABLE Suppliers (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, "
        "ContactName TEXT, Phone TEXT, Email TEXT, Address TEXT, Notes TEXT, Active INTEGER DEFAULT 1)",
        "CREATE TABLE Suppliers (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL, "
        "ContactName VARCHAR(255), Phone VARCHAR(255), Email VARCHAR(255), Address MEMO, Notes MEMO, Active INTEGER DEFAULT 1)"))
        return false;

    // 24. StockMovements
    if (!createTable("StockMovements",
        "CREATE TABLE StockMovements (Id INTEGER PRIMARY KEY AUTOINCREMENT, ProductId INTEGER NOT NULL, "
        "MovementType TEXT NOT NULL, Quantity INTEGER NOT NULL, ReferenceType TEXT, ReferenceId INTEGER, "
        "DateTime TEXT NOT NULL, UserId INTEGER, Notes TEXT, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id))",
        "CREATE TABLE StockMovements (Id COUNTER PRIMARY KEY, ProductId INTEGER NOT NULL, "
        "MovementType VARCHAR(50) NOT NULL, Quantity INTEGER NOT NULL, ReferenceType VARCHAR(50), ReferenceId INTEGER, "
        "DateTime VARCHAR(255) NOT NULL, UserId INTEGER, Notes MEMO)"))
        return false;

    // 25. RegisterSessions
    if (!createTable("RegisterSessions",
        "CREATE TABLE RegisterSessions (Id INTEGER PRIMARY KEY AUTOINCREMENT, UserId INTEGER NOT NULL, "
        "OpeningCash REAL DEFAULT 0, ClosingCash REAL DEFAULT 0, OpenedAt TEXT NOT NULL, ClosedAt TEXT, "
        "Status TEXT DEFAULT 'Open', FOREIGN KEY(UserId) REFERENCES Users(Id))",
        "CREATE TABLE RegisterSessions (Id COUNTER PRIMARY KEY, UserId INTEGER NOT NULL, "
        "OpeningCash DOUBLE DEFAULT 0, ClosingCash DOUBLE DEFAULT 0, OpenedAt VARCHAR(255) NOT NULL, ClosedAt VARCHAR(255), "
        "Status VARCHAR(50) DEFAULT 'Open')"))
        return false;

    // 26. CashMovements
    if (!createTable("CashMovements",
        "CREATE TABLE CashMovements (Id INTEGER PRIMARY KEY AUTOINCREMENT, RegisterSessionId INTEGER NOT NULL, "
        "Type TEXT NOT NULL, Amount REAL NOT NULL, Reason TEXT, DateTime TEXT NOT NULL, UserId INTEGER, "
        "FOREIGN KEY(RegisterSessionId) REFERENCES RegisterSessions(Id))",
        "CREATE TABLE CashMovements (Id COUNTER PRIMARY KEY, RegisterSessionId INTEGER NOT NULL, "
        "Type VARCHAR(50) NOT NULL, Amount DOUBLE NOT NULL, Reason MEMO, DateTime VARCHAR(255) NOT NULL, UserId INTEGER)"))
        return false;

    // 27. ExpenseCategories
    if (!createTable("ExpenseCategories",
        "CREATE TABLE ExpenseCategories (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE, Description TEXT)",
        "CREATE TABLE ExpenseCategories (Id COUNTER PRIMARY KEY, Name VARCHAR(255) NOT NULL UNIQUE, Description MEMO)"))
        return false;

    // 27.5 Attendance system
    if (!createTable("Attendance",
        "CREATE TABLE Attendance (Id INTEGER PRIMARY KEY AUTOINCREMENT, UserId INTEGER NOT NULL, "
        "Timestamp TEXT NOT NULL, Type TEXT NOT NULL, "
        "FOREIGN KEY(UserId) REFERENCES Users(Id))",
        "CREATE TABLE Attendance (Id COUNTER PRIMARY KEY, UserId INTEGER NOT NULL, "
        "Timestamp VARCHAR(255) NOT NULL, Type VARCHAR(10) NOT NULL)"))
        return false;

    // 28. Expenses
    if (!createTable("Expenses",
        "CREATE TABLE Expenses (Id INTEGER PRIMARY KEY AUTOINCREMENT, CategoryId INTEGER, Amount REAL NOT NULL, "
        "Description TEXT, Date TEXT NOT NULL, UserId INTEGER, RegisterSessionId INTEGER, "
        "FOREIGN KEY(CategoryId) REFERENCES ExpenseCategories(Id), FOREIGN KEY(RegisterSessionId) REFERENCES RegisterSessions(Id))",
        "CREATE TABLE Expenses (Id COUNTER PRIMARY KEY, CategoryId INTEGER, Amount DOUBLE NOT NULL, "
        "Description MEMO, Date VARCHAR(255) NOT NULL, UserId INTEGER, RegisterSessionId INTEGER)"))
        return false;

    // 29. PurchaseInvoices
    if (!createTable("PurchaseInvoices",
        "CREATE TABLE PurchaseInvoices (Id INTEGER PRIMARY KEY AUTOINCREMENT, SupplierId INTEGER, "
        "InvoiceNumber TEXT, Date TEXT NOT NULL, TotalAmount REAL DEFAULT 0, Notes TEXT, UserId INTEGER, "
        "Status TEXT DEFAULT 'Draft', FOREIGN KEY(SupplierId) REFERENCES Suppliers(Id))",
        "CREATE TABLE PurchaseInvoices (Id COUNTER PRIMARY KEY, SupplierId INTEGER, "
        "InvoiceNumber VARCHAR(100), Date VARCHAR(255) NOT NULL, TotalAmount DOUBLE DEFAULT 0, Notes MEMO, UserId INTEGER, "
        "Status VARCHAR(50) DEFAULT 'Draft')"))
        return false;

    // 30. PurchaseInvoiceItems
    if (!createTable("PurchaseInvoiceItems",
        "CREATE TABLE PurchaseInvoiceItems (Id INTEGER PRIMARY KEY AUTOINCREMENT, InvoiceId INTEGER NOT NULL, "
        "ProductId INTEGER NOT NULL, Quantity INTEGER NOT NULL, UnitCost REAL NOT NULL, TotalCost REAL NOT NULL, "
        "FOREIGN KEY(InvoiceId) REFERENCES PurchaseInvoices(Id) ON DELETE CASCADE, FOREIGN KEY(ProductId) REFERENCES Products(Id))",
        "CREATE TABLE PurchaseInvoiceItems (Id COUNTER PRIMARY KEY, InvoiceId INTEGER NOT NULL, "
        "ProductId INTEGER NOT NULL, Quantity INTEGER NOT NULL, UnitCost DOUBLE NOT NULL, TotalCost DOUBLE NOT NULL)"))
        return false;

    // 31. StockCounts
    if (!createTable("StockCounts",
        "CREATE TABLE StockCounts (Id INTEGER PRIMARY KEY AUTOINCREMENT, Date TEXT NOT NULL, UserId INTEGER NOT NULL, "
        "Status TEXT DEFAULT 'Draft', Notes TEXT, FOREIGN KEY(UserId) REFERENCES Users(Id))",
        "CREATE TABLE StockCounts (Id COUNTER PRIMARY KEY, Date VARCHAR(255) NOT NULL, UserId INTEGER NOT NULL, "
        "Status VARCHAR(50) DEFAULT 'Draft', Notes MEMO)"))
        return false;

    // 32. StockCountItems
    if (!createTable("StockCountItems",
        "CREATE TABLE StockCountItems (Id INTEGER PRIMARY KEY AUTOINCREMENT, StockCountId INTEGER NOT NULL, "
        "ProductId INTEGER NOT NULL, SystemQty INTEGER DEFAULT 0, ActualQty INTEGER NOT NULL, Difference INTEGER DEFAULT 0, "
        "FOREIGN KEY(StockCountId) REFERENCES StockCounts(Id) ON DELETE CASCADE, FOREIGN KEY(ProductId) REFERENCES Products(Id))",
        "CREATE TABLE StockCountItems (Id COUNTER PRIMARY KEY, StockCountId INTEGER NOT NULL, "
        "ProductId INTEGER NOT NULL, SystemQty INTEGER DEFAULT 0, ActualQty INTEGER NOT NULL, Difference INTEGER DEFAULT 0)"))
        return false;

    // 33. ProductCostHistory
    if (!createTable("ProductCostHistory",
        "CREATE TABLE ProductCostHistory (Id INTEGER PRIMARY KEY AUTOINCREMENT, ProductId INTEGER NOT NULL, "
        "CostPrice REAL NOT NULL, EffectiveDate TEXT NOT NULL, PurchaseInvoiceId INTEGER, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id), FOREIGN KEY(PurchaseInvoiceId) REFERENCES PurchaseInvoices(Id))",
        "CREATE TABLE ProductCostHistory (Id COUNTER PRIMARY KEY, ProductId INTEGER NOT NULL, "
        "CostPrice DOUBLE NOT NULL, EffectiveDate VARCHAR(255) NOT NULL, PurchaseInvoiceId INTEGER)"))
        return false;

    // 34. ProductSubUnits (وحدة فرعية)
    if (!createTable("ProductSubUnits",
        "CREATE TABLE ProductSubUnits (Id INTEGER PRIMARY KEY AUTOINCREMENT, ProductId INTEGER NOT NULL, "
        "Name TEXT NOT NULL, Barcode TEXT, QuantityPerUnit REAL NOT NULL, CostPrice REAL DEFAULT 0, SalePrice REAL DEFAULT 0, "
        "FOREIGN KEY(ProductId) REFERENCES Products(Id) ON DELETE CASCADE)",
        "CREATE TABLE ProductSubUnits (Id COUNTER PRIMARY KEY, ProductId INTEGER NOT NULL, "
        "Name VARCHAR(255) NOT NULL, Barcode VARCHAR(255), QuantityPerUnit DOUBLE NOT NULL, CostPrice DOUBLE DEFAULT 0, SalePrice DOUBLE DEFAULT 0)"))
        return false;

    // 35. TaxSettings
    if (!createTable("TaxSettings",
        "CREATE TABLE TaxSettings (Id INTEGER PRIMARY KEY AUTOINCREMENT, TaxRate REAL DEFAULT 0, IsEnabled INTEGER DEFAULT 0)",
        "CREATE TABLE TaxSettings (Id COUNTER PRIMARY KEY, TaxRate DOUBLE DEFAULT 0, IsEnabled INTEGER DEFAULT 0)"))
        return false;

    // 22. Indexes
    if (driverName == "QSQLITE") {
        query.exec("CREATE INDEX IF NOT EXISTS idx_customers_name ON Customers(Name)");
        query.exec("CREATE INDEX IF NOT EXISTS idx_phones_number ON CustomerPhones(PhoneNumber)");
        query.exec("CREATE INDEX IF NOT EXISTS idx_products_name ON Products(Name)");
        query.exec("CREATE INDEX IF NOT EXISTS idx_products_barcode ON Products(Barcode)");
        query.exec("CREATE INDEX IF NOT EXISTS idx_orders_status ON Orders(Status)");
    } else {
        query.exec("CREATE INDEX idx_customers_name ON Customers(Name)");
        query.exec("CREATE INDEX idx_phones_number ON CustomerPhones(PhoneNumber)");
        query.exec("CREATE INDEX idx_products_name ON Products(Name)");
    }

    Logger::instance().info("Schema validation and indexing completed successfully.");
    return true;
}


// Static helper: ensures search_path is set to the active branch schema.
// QPSQL connections can "forget" search_path after certain operations (transactions, reconnects, etc.).
// Call this before any schema-dependent query to avoid "relation does not exist" errors.
void DatabaseConnectionManager::ensureSearchPath(QSqlDatabase db) {
    if (!db.isOpen() || db.driverName() != "QPSQL") return;
    
    QString schemaName = SchemaDeployer::sanitizeSchemaName(BranchManager::instance().activeBranch().name);
    QString safe = schemaName;
    safe.replace('"', "\"\"");
    
    QSqlQuery q(db);
    // Use SET SESSION to persist across transactions
    q.exec(QString("SET SESSION search_path TO \"%1\"").arg(safe));
    
    Logger::instance().info(QString("ensureSearchPath: SET SESSION search_path TO \"%1\"").arg(schemaName));
}

// ── Connection Health Monitoring ──────────────────────────────────────────────
bool DatabaseConnectionManager::isConnectionAlive() {
    if (!QSqlDatabase::contains(m_connectionName)) return false;
    
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return false;
    
    // Quick ping query to check if connection is responsive
    QSqlQuery q(db);
    bool alive = q.exec("SELECT 1");
    return alive;
}

void DatabaseConnectionManager::startConnectionMonitor() {
    if (m_healthCheckTimer) return;  // Already running
    
    m_healthCheckTimer = new QTimer();
    connect(m_healthCheckTimer, &QTimer::timeout, this, &DatabaseConnectionManager::checkConnectionHealth);
    m_healthCheckTimer->start(30000);  // Check every 30 seconds
    
    Logger::instance().info("Connection health monitor started (30s interval)");
}

void DatabaseConnectionManager::stopConnectionMonitor() {
    if (m_healthCheckTimer) {
        m_healthCheckTimer->stop();
        delete m_healthCheckTimer;
        m_healthCheckTimer = nullptr;
        Logger::instance().info("Connection health monitor stopped");
    }
}

void DatabaseConnectionManager::checkConnectionHealth() {
    if (!m_isCloudConnection) return;
    
    if (!isConnectionAlive()) {
        Logger::instance().warn("Connection health check failed - attempting reconnect...");
        // Try to reopen the connection
        if (QSqlDatabase::contains(m_connectionName)) {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (!db.isOpen()) {
                // Connection dropped - try to reopen
                BranchInfo activeBranch = BranchManager::instance().activeBranch();
                if (openPostgresConnection(activeBranch.dbPath)) {
                    Logger::instance().info("Connection restored successfully");
                    initializeSchema();
                } else {
                    Logger::instance().error("Failed to restore connection: " + m_lastError);
                }
            }
        }
    }
}
