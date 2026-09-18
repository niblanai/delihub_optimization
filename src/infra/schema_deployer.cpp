#include "schema_deployer.h"
#include "database_connection_manager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QCoreApplication>
#include <QRegularExpression>

QString SchemaDeployer::sanitizeSchemaName(const QString& branchName) {
    QString s = branchName.trimmed().toLower();
    // Collapse any run of whitespace to a single underscore so that
    // "مدينة نصر" and "مدينة   نصر" always resolve to the same schema.
    s.replace(QRegularExpression("\\s+"), "_");

    // Postgres identifiers are limited to 63 bytes (NAMEDATALEN-1).
    // Truncate on a valid UTF-8 boundary so multi-byte characters
    // (Arabic, etc.) are never split in half.
    QByteArray utf8 = s.toUtf8();
    if (utf8.size() > 63) {
        utf8.truncate(63);
        while (!utf8.isEmpty() && (static_cast<unsigned char>(utf8.at(utf8.size() - 1)) & 0xC0) == 0x80)
            utf8.chop(1);
        s = QString::fromUtf8(utf8);
    }
    if (s.isEmpty()) s = "branch";
    return s;
}

// Escapes a schema name for safe embedding inside a double-quoted
// Postgres identifier (doubles any embedded " characters).
static QString escapeIdentifier(const QString& raw) {
    QString r = raw;
    r.replace('"', "\"\"");
    return r;
}

// ── Build CREATE TABLE SQL for a given schema name ────────────────────────────
static QStringList buildSchemaSql(const QString& schema) {
    const QString safe = escapeIdentifier(schema);
    QStringList stmts;
    stmts << QString("CREATE SCHEMA IF NOT EXISTS \"%1\"").arg(safe);
    stmts << QString("SET SESSION search_path TO \"%1\"").arg(safe);

    // Use lowercase unquoted identifiers for PostgreSQL compatibility
    // PostgreSQL converts unquoted identifiers to lowercase, matching repository expectations
    const QString prefix = QString("\"%1\".").arg(safe);
    
    stmts <<
        QString("CREATE TABLE IF NOT EXISTS %1branches (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL, address VARCHAR(255))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1categories (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL UNIQUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1regions (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL UNIQUE, distancekm DOUBLE PRECISION DEFAULT 0, deliveryfee DOUBLE PRECISION DEFAULT 0)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1roles (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL UNIQUE, "
            "canmanageusers BOOLEAN DEFAULT FALSE, canviewreports BOOLEAN DEFAULT FALSE, "
            "canaddcustomer BOOLEAN DEFAULT FALSE, caneditcustomer BOOLEAN DEFAULT FALSE, candeletecustomer BOOLEAN DEFAULT FALSE, "
            "canaddproduct BOOLEAN DEFAULT FALSE, caneditproduct BOOLEAN DEFAULT FALSE, candeleteproduct BOOLEAN DEFAULT FALSE, "
            "canaddorder BOOLEAN DEFAULT FALSE, caneditorder BOOLEAN DEFAULT FALSE, cancancelorder BOOLEAN DEFAULT FALSE, "
            "candeleteorders BOOLEAN DEFAULT FALSE, canmanageregions BOOLEAN DEFAULT FALSE, canaccesssettings BOOLEAN DEFAULT FALSE, "
            "caneditorderdate BOOLEAN DEFAULT FALSE, canaccessdashboard BOOLEAN DEFAULT TRUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1users (id SERIAL PRIMARY KEY, username VARCHAR(255) NOT NULL UNIQUE, "
            "passwordhash VARCHAR(255) NOT NULL, passwordsalt VARCHAR(255) NOT NULL, roleid INTEGER, name VARCHAR(255), phone VARCHAR(255), email VARCHAR(255))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1customers (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL, distancekm DOUBLE PRECISION DEFAULT 0, "
            "regionid INTEGER, notes TEXT, firstcontactdate VARCHAR(50), lastorderdate VARCHAR(50), status VARCHAR(50) DEFAULT 'Active', "
            "preferredpaymentmethod VARCHAR(50) DEFAULT 'Cash', preferredpaymentother VARCHAR(255), debt DOUBLE PRECISION DEFAULT 0.0)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1customerphones (customerid INTEGER, phonenumber VARCHAR(255), PRIMARY KEY(customerid, phonenumber))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1customeraddresses (customerid INTEGER, addresstext VARCHAR(255), PRIMARY KEY(customerid, addresstext))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1customerfavorites (customerid INTEGER, productid INTEGER, PRIMARY KEY(customerid, productid))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1products (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL, barcode VARCHAR(255), "
            "price DOUBLE PRECISION NOT NULL, categoryid INTEGER, status VARCHAR(50) DEFAULT 'Active', "
            "manufacturedate VARCHAR(20), expirydate VARCHAR(20), stockqty INTEGER DEFAULT 0, lowstockthreshold INTEGER DEFAULT 5, "
            "costprice DOUBLE PRECISION DEFAULT 0, unitlabel VARCHAR(50) DEFAULT 'وحدة', imagepath VARCHAR(500))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1productpricehistory (id SERIAL PRIMARY KEY, productid INTEGER, price DOUBLE PRECISION, effectivedate VARCHAR(50))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1orders (id SERIAL PRIMARY KEY, customerid INTEGER, registersessionid INTEGER, datetime VARCHAR(50) NOT NULL, "
            "deliveryfee DOUBLE PRECISION DEFAULT 0, subtotal DOUBLE PRECISION DEFAULT 0, grandtotal DOUBLE PRECISION DEFAULT 0, "
            "status VARCHAR(50) DEFAULT 'Pending', cancelreason TEXT, driverid INTEGER DEFAULT 0, drivername VARCHAR(255), "
            "discountamount DOUBLE PRECISION DEFAULT 0, discountreason VARCHAR(255), paymentmethod VARCHAR(50) DEFAULT 'Cash', "
            "paymentotherdetail VARCHAR(255), invoicenumber VARCHAR(50))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1orderitems (orderid INTEGER, productid INTEGER, quantity INTEGER, unitprice DOUBLE PRECISION, PRIMARY KEY(orderid, productid))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1scheduledorders (id SERIAL PRIMARY KEY, customerid INTEGER, weekdays VARCHAR(50), time VARCHAR(10), "
            "remindminutesbefore INTEGER DEFAULT 0, remindrepeatinterval INTEGER DEFAULT 0, autocreateorder BOOLEAN DEFAULT FALSE, "
            "isrecurring BOOLEAN DEFAULT TRUE, onetimedate VARCHAR(50))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1scheduledorderitems (scheduledorderid INTEGER, productid INTEGER, quantity INTEGER, PRIMARY KEY(scheduledorderid, productid))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1missingproducts (id SERIAL PRIMARY KEY, productid INTEGER, name VARCHAR(255), barcode VARCHAR(255), "
            "quantityneeded INTEGER, branchid INTEGER, dateadded VARCHAR(20), purchased BOOLEAN DEFAULT FALSE, "
            "source VARCHAR(50) DEFAULT 'Manual', currentqty INTEGER DEFAULT 0, unitlabel VARCHAR(50))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1deliverydrivers (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL, phone VARCHAR(255), nationalid VARCHAR(255), active BOOLEAN DEFAULT TRUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1orderstatuses (id SERIAL PRIMARY KEY, name VARCHAR(255) NOT NULL UNIQUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1auditlogs (id SERIAL PRIMARY KEY, userid INTEGER, action VARCHAR(255), entitytype VARCHAR(255), entityid INTEGER, details TEXT, timestamp VARCHAR(50))").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1coupons (id SERIAL PRIMARY KEY, code VARCHAR(50) NOT NULL UNIQUE, description VARCHAR(255), "
            "type VARCHAR(50) DEFAULT 'FixedAmount', value DOUBLE PRECISION DEFAULT 0, expirydate VARCHAR(20), maxuses INTEGER DEFAULT 0, usedcount INTEGER DEFAULT 0, active BOOLEAN DEFAULT TRUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1orderreturns (id SERIAL PRIMARY KEY, orderid INTEGER, customerid INTEGER, customername VARCHAR(255), reason TEXT, datetime VARCHAR(50), isfullreturn BOOLEAN DEFAULT TRUE)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1returnitems (id SERIAL PRIMARY KEY, returnid INTEGER, productid INTEGER, productname VARCHAR(255), quantityreturned INTEGER, unitprice DOUBLE PRECISION)").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1notes (id SERIAL PRIMARY KEY, title VARCHAR(255) NOT NULL DEFAULT '', "
            "content TEXT NOT NULL DEFAULT '', color VARCHAR(50) NOT NULL DEFAULT 'yellow', "
            "pinned BOOLEAN NOT NULL DEFAULT FALSE, attachments TEXT DEFAULT '', "
            "createdat VARCHAR(50) NOT NULL DEFAULT '', updatedat VARCHAR(50) NOT NULL DEFAULT '')").arg(prefix) <<
        QString("CREATE TABLE IF NOT EXISTS %1deletedrecordsarchive ("
            "id SERIAL PRIMARY KEY, "
            "table_name TEXT NOT NULL, "
            "record_id TEXT NOT NULL, "
            "record_data TEXT NOT NULL, "
            "deleted_at TEXT NOT NULL, "
            "deleted_by TEXT, "
            "restored_at TEXT, "
            "restored_by TEXT)").arg(prefix) <<
        QString("INSERT INTO %1orderstatuses (name) SELECT 'Pending' WHERE NOT EXISTS (SELECT 1 FROM %1orderstatuses WHERE name='Pending')").arg(prefix) <<
        QString("INSERT INTO %1orderstatuses (name) SELECT 'Out for Delivery' WHERE NOT EXISTS (SELECT 1 FROM %1orderstatuses WHERE name='Out for Delivery')").arg(prefix) <<
        QString("INSERT INTO %1orderstatuses (name) SELECT 'Delivered' WHERE NOT EXISTS (SELECT 1 FROM %1orderstatuses WHERE name='Delivered')").arg(prefix) <<
        QString("INSERT INTO %1orderstatuses (name) SELECT 'Cancelled' WHERE NOT EXISTS (SELECT 1 FROM %1orderstatuses WHERE name='Cancelled')").arg(prefix) <<
        
        // ══════════════════════════════════════════════════════════════════════
        // Phase 1: Purchases + POS + Register + Expenses + Inventory Tables
        // ══════════════════════════════════════════════════════════════════════
        
        // 1. Suppliers table
        QString("CREATE TABLE IF NOT EXISTS %1suppliers ("
            "id SERIAL PRIMARY KEY, "
            "name VARCHAR(255) NOT NULL, "
            "contactname VARCHAR(255), "
            "phone VARCHAR(255), "
            "email VARCHAR(255), "
            "address TEXT, "
            "notes TEXT, "
            "active BOOLEAN DEFAULT TRUE)").arg(prefix) <<
        
        // 2. StockMovements table (unified inventory tracking)
        QString("CREATE TABLE IF NOT EXISTS %1stockmovements ("
            "id SERIAL PRIMARY KEY, "
            "productid INTEGER NOT NULL, "
            "movementtype VARCHAR(50) NOT NULL, "
            "quantity INTEGER NOT NULL, "
            "referencetype VARCHAR(50), "
            "referenceid INTEGER, "
            "datetime VARCHAR(50) NOT NULL, "
            "userid INTEGER, "
            "notes TEXT)").arg(prefix) <<
        
        // 3. RegisterSessions table (till/cash register sessions)
        QString("CREATE TABLE IF NOT EXISTS %1registersessions ("
            "id SERIAL PRIMARY KEY, "
            "userid INTEGER NOT NULL, "
            "openingcash DOUBLE PRECISION DEFAULT 0, "
            "closingcash DOUBLE PRECISION DEFAULT 0, "
            "openedat VARCHAR(50) NOT NULL, "
            "closedat VARCHAR(50), "
            "status VARCHAR(50) DEFAULT 'Open')").arg(prefix) <<
        
        // 4. CashMovements table (cash in/out within register sessions)
        QString("CREATE TABLE IF NOT EXISTS %1cashmovements ("
            "id SERIAL PRIMARY KEY, "
            "registersessionid INTEGER NOT NULL, "
            "type VARCHAR(50) NOT NULL, "
            "amount DOUBLE PRECISION NOT NULL, "
            "reason TEXT, "
            "datetime VARCHAR(50) NOT NULL, "
            "userid INTEGER)").arg(prefix) <<
        
        // 5. ExpenseCategories table
        QString("CREATE TABLE IF NOT EXISTS %1expensecategories ("
            "id SERIAL PRIMARY KEY, "
            "name VARCHAR(255) NOT NULL UNIQUE, "
            "description TEXT)").arg(prefix) <<
        
        // 6. Expenses table
        QString("CREATE TABLE IF NOT EXISTS %1expenses ("
            "id SERIAL PRIMARY KEY, "
            "categoryid INTEGER, "
            "amount DOUBLE PRECISION NOT NULL, "
            "description TEXT, "
            "date VARCHAR(50) NOT NULL, "
            "userid INTEGER, "
            "registersessionid INTEGER)").arg(prefix) <<
        
        // 7. PurchaseInvoices table
        QString("CREATE TABLE IF NOT EXISTS %1purchaseinvoices ("
            "id SERIAL PRIMARY KEY, "
            "supplierid INTEGER, "
            "invoicenumber VARCHAR(100), "
            "date VARCHAR(50) NOT NULL, "
            "totalamount DOUBLE PRECISION DEFAULT 0, "
            "notes TEXT, "
            "userid INTEGER, "
            "status VARCHAR(50) DEFAULT 'Draft')").arg(prefix) <<
        
        // 8. PurchaseInvoiceItems table
        QString("CREATE TABLE IF NOT EXISTS %1purchaseinvoiceitems ("
            "id SERIAL PRIMARY KEY, "
            "invoiceid INTEGER NOT NULL, "
            "productid INTEGER NOT NULL, "
            "quantity INTEGER NOT NULL, "
            "unitcost DOUBLE PRECISION NOT NULL, "
            "totalcost DOUBLE PRECISION NOT NULL)").arg(prefix) <<
        
        // 9. StockCounts table (inventory counts/stocktake)
        QString("CREATE TABLE IF NOT EXISTS %1stockcounts ("
            "id SERIAL PRIMARY KEY, "
            "date VARCHAR(50) NOT NULL, "
            "userid INTEGER NOT NULL, "
            "status VARCHAR(50) DEFAULT 'Draft', "
            "notes TEXT)").arg(prefix) <<
        
        // 10. StockCountItems table
        QString("CREATE TABLE IF NOT EXISTS %1stockcountitems ("
            "id SERIAL PRIMARY KEY, "
            "stockcountid INTEGER NOT NULL, "
            "productid INTEGER NOT NULL, "
            "systemqty INTEGER DEFAULT 0, "
            "actualqty INTEGER NOT NULL, "
            "difference INTEGER DEFAULT 0)").arg(prefix) <<
        
        // 11. ProductCostHistory table (track purchase cost changes)
        QString("CREATE TABLE IF NOT EXISTS %1productcosthistory ("
            "id SERIAL PRIMARY KEY, "
            "productid INTEGER NOT NULL, "
            "costprice DOUBLE PRECISION NOT NULL, "
            "effectivedate VARCHAR(50) NOT NULL, "
            "purchaseinvoiceid INTEGER)").arg(prefix) <<
        
        // 12. TaxSettings table (global tax rate)
        QString("CREATE TABLE IF NOT EXISTS %1taxsettings ("
            "id SERIAL PRIMARY KEY, "
            "taxrate DOUBLE PRECISION DEFAULT 0, "
            "isenabled BOOLEAN DEFAULT FALSE)").arg(prefix) <<
        
        // 13. ProductSuppliers table (multiple suppliers per product)
        QString("CREATE TABLE IF NOT EXISTS %1productsuppliers ("
            "productid INTEGER NOT NULL, "
            "supplierid INTEGER NOT NULL, "
            "purchaseprice DOUBLE PRECISION DEFAULT 0, "
            "ispreferred BOOLEAN DEFAULT FALSE, "
            "lastpurchasedate VARCHAR(50), "
            "PRIMARY KEY(productid, supplierid))").arg(prefix) <<
        
        // 14. Promotions table (promotional offers)
        QString("CREATE TABLE IF NOT EXISTS %1promotions ("
            "id SERIAL PRIMARY KEY, "
            "title VARCHAR(255) NOT NULL, "
            "description TEXT, "
            "type VARCHAR(50) DEFAULT 'Percentage', "
            "discountvalue DOUBLE PRECISION DEFAULT 0, "
            "startdate VARCHAR(20) NOT NULL, "
            "enddate VARCHAR(20) NOT NULL, "
            "status VARCHAR(50) DEFAULT 'Active', "
            "imagepath VARCHAR(500), "
            "productid INTEGER DEFAULT 0, "
            "categoryid INTEGER DEFAULT 0, "
            "minpurchaseamount DOUBLE PRECISION DEFAULT 0)").arg(prefix) <<
        
        // 15. PromotionProducts junction table (Many-to-Many: Promotion - Products)
        QString("CREATE TABLE IF NOT EXISTS %1promotionproducts ("
            "promotionid INTEGER NOT NULL, "
            "productid INTEGER NOT NULL, "
            "magazineprice DOUBLE PRECISION DEFAULT 0, "
            "PRIMARY KEY (promotionid, productid))").arg(prefix);

    // ALTER TABLE statements for existing schemas (safe migrations - columns added only if not exists)
    // Use lowercase for table and column names
    stmts <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='regions' AND column_name='distancekm') THEN "
                "ALTER TABLE %2regions ADD COLUMN distancekm DOUBLE PRECISION DEFAULT 0; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='regions' AND column_name='deliveryfee') THEN "
                "ALTER TABLE %2regions ADD COLUMN deliveryfee DOUBLE PRECISION DEFAULT 0; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='missingproducts' AND column_name='unitlabel') THEN "
                "ALTER TABLE %2missingproducts ADD COLUMN unitlabel VARCHAR(50); "
                "END IF; END $$").arg(safe, prefix) <<
        
        // Phase 1 migrations: Add new permissions to Roles table
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='roles' AND column_name='canmanagepurchases') THEN "
                "ALTER TABLE %2roles ADD COLUMN canmanagepurchases BOOLEAN DEFAULT FALSE; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='roles' AND column_name='canaccesspos') THEN "
                "ALTER TABLE %2roles ADD COLUMN canaccesspos BOOLEAN DEFAULT FALSE; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='roles' AND column_name='canmanageregister') THEN "
                "ALTER TABLE %2roles ADD COLUMN canmanageregister BOOLEAN DEFAULT FALSE; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='roles' AND column_name='canmanageexpenses') THEN "
                "ALTER TABLE %2roles ADD COLUMN canmanageexpenses BOOLEAN DEFAULT FALSE; "
                "END IF; END $$").arg(safe, prefix) <<
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='roles' AND column_name='canmanageinventory') THEN "
                "ALTER TABLE %2roles ADD COLUMN canmanageinventory BOOLEAN DEFAULT FALSE; "
                "END IF; END $$").arg(safe, prefix) <<
        
        // Add CostPrice column to Products table
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='products' AND column_name='costprice') THEN "
                "ALTER TABLE %2products ADD COLUMN costprice DOUBLE PRECISION DEFAULT 0; "
                "END IF; END $$").arg(safe, prefix) <<
        
        // Add UnitLabel column to Products table (for POS display)
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='products' AND column_name='unitlabel') THEN "
                "ALTER TABLE %2products ADD COLUMN unitlabel VARCHAR(50) DEFAULT 'وحدة'; "
                "END IF; END $$").arg(safe, prefix) <<
        
        // Add MagazinePrice column to PromotionProducts table
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='promotionproducts' AND column_name='magazineprice') THEN "
                "ALTER TABLE %2promotionproducts ADD COLUMN magazineprice DOUBLE PRECISION DEFAULT 0; "
                "END IF; END $$").arg(safe, prefix) <<
        
        // Add RegisterSessionId column to Orders table (for POS tracking)
        QString("DO $$ BEGIN "
                "IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='%1' AND table_name='orders' AND column_name='registersessionid') THEN "
                "ALTER TABLE %2orders ADD COLUMN registersessionid INTEGER; "
                "END IF; END $$").arg(safe, prefix);

    // Indexes - fully qualified with lowercase
    stmts <<
        QString("CREATE INDEX IF NOT EXISTS idx_customers_name ON %1customers(name)").arg(prefix) <<
        QString("CREATE INDEX IF NOT EXISTS idx_products_name ON %1products(name)").arg(prefix) <<
        QString("CREATE INDEX IF NOT EXISTS idx_products_barcode ON %1products(barcode)").arg(prefix) <<
        QString("CREATE INDEX IF NOT EXISTS idx_orders_status ON %1orders(status)").arg(prefix);

    // Create views with PascalCase names pointing to lowercase tables (for compatibility)
    // This allows existing code using PascalCase to work without modifications
    stmts <<
        QString("CREATE OR REPLACE VIEW %1\"Branches\" AS SELECT * FROM %1branches").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Categories\" AS SELECT * FROM %1categories").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Regions\" AS SELECT * FROM %1regions").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Roles\" AS SELECT * FROM %1roles").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Users\" AS SELECT * FROM %1users").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Customers\" AS SELECT * FROM %1customers").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"CustomerPhones\" AS SELECT * FROM %1customerphones").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"CustomerAddresses\" AS SELECT * FROM %1customeraddresses").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"CustomerFavorites\" AS SELECT * FROM %1customerfavorites").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Products\" AS SELECT * FROM %1products").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ProductPriceHistory\" AS SELECT * FROM %1productpricehistory").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Orders\" AS SELECT * FROM %1orders").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"OrderItems\" AS SELECT * FROM %1orderitems").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ScheduledOrders\" AS SELECT * FROM %1scheduledorders").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ScheduledOrderItems\" AS SELECT * FROM %1scheduledorderitems").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"MissingProducts\" AS SELECT * FROM %1missingproducts").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"DeliveryDrivers\" AS SELECT * FROM %1deliverydrivers").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"OrderStatuses\" AS SELECT * FROM %1orderstatuses").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"AuditLogs\" AS SELECT * FROM %1auditlogs").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Coupons\" AS SELECT * FROM %1coupons").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"OrderReturns\" AS SELECT * FROM %1orderreturns").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ReturnItems\" AS SELECT * FROM %1returnitems").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Notes\" AS SELECT * FROM %1notes").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"DeletedRecordsArchive\" AS SELECT * FROM %1deletedrecordsarchive").arg(prefix) <<
        
        // Phase 1: Views for new tables
        QString("CREATE OR REPLACE VIEW %1\"Suppliers\" AS SELECT * FROM %1suppliers").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"StockMovements\" AS SELECT * FROM %1stockmovements").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"RegisterSessions\" AS SELECT * FROM %1registersessions").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"CashMovements\" AS SELECT * FROM %1cashmovements").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ExpenseCategories\" AS SELECT * FROM %1expensecategories").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Expenses\" AS SELECT * FROM %1expenses").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"PurchaseInvoices\" AS SELECT * FROM %1purchaseinvoices").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"PurchaseInvoiceItems\" AS SELECT * FROM %1purchaseinvoiceitems").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"StockCounts\" AS SELECT * FROM %1stockcounts").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"StockCountItems\" AS SELECT * FROM %1stockcountitems").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ProductCostHistory\" AS SELECT * FROM %1productcosthistory").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"TaxSettings\" AS SELECT * FROM %1taxsettings").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"ProductSuppliers\" AS SELECT * FROM %1productsuppliers").arg(prefix) <<
        QString("CREATE OR REPLACE VIEW %1\"Promotions\" AS SELECT * FROM %1promotions").arg(prefix);

    return stmts;
}

// ── Deploy schema to a PostgreSQL connection ──────────────────────────────────
bool SchemaDeployer::deployToPostgres(const QString& connStr,
                                       const QString& schemaName,
                                       QString& errorOut)
{
    // Parse connection string
    QRegularExpression re(R"(postgres(?:ql)?://([^:]+):([^@]+)@([^:/]+):?(\d*)/(.+))");
    auto match = re.match(connStr);
    if (!match.hasMatch()) {
        errorOut = "Invalid PostgreSQL connection string.";
        return false;
    }

    // Use a unique temp connection name
    QString connName = "schema_deploy_" + schemaName;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);
        db.setUserName(match.captured(1));
        db.setPassword(match.captured(2));
        db.setHostName(match.captured(3));
        db.setPort(match.captured(4).isEmpty() ? 5432 : match.captured(4).toInt());
        db.setDatabaseName(match.captured(5));
        QString certPath = QCoreApplication::applicationDirPath() + "/ca-certificates.crt";
        db.setConnectOptions("sslmode=require");

        if (!db.open()) {
            QString rawErr = db.lastError().text();
            if (rawErr.contains("could not translate host name", Qt::CaseInsensitive)
                || rawErr.contains("Name or service not known", Qt::CaseInsensitive)
                || rawErr.contains("nodename nor servname", Qt::CaseInsensitive))
            {
                errorOut =
                    "Connection failed: could not resolve the Supabase host.\n\n"
                    "This usually means the Direct Connection URL uses an IPv6-only host\n"
                    "that your network does not support.\n\n"
                    "Fix: use the Session Pooler connection string instead:\n"
                    "  1. Go to Supabase Dashboard → Connect button\n"
                    "  2. Choose the \"Session pooler\" tab (port 5432)\n"
                    "  3. Copy the URL — it looks like:\n"
                    "     postgresql://postgres.<project-ref>:[PASSWORD]"
                    "@aws-0-<region>.pooler.supabase.com:5432/postgres\n"
                    "  4. Paste that URL in the Branch DB Path field and try again.";
            } else {
                errorOut = "Connection failed: " + rawErr;
            }
            QSqlDatabase::removeDatabase(connName);
            return false;
        }

        QSqlQuery q(db);
        const QStringList stmts = buildSchemaSql(schemaName);
        for (int i = 0; i < stmts.size(); ++i) {
            const QString& stmt = stmts[i];
            if (!q.exec(stmt)) {
                // The first two statements (CREATE SCHEMA, SET search_path)
                // are load-bearing: if either fails, every CREATE TABLE that
                // follows silently lands in the wrong schema (or "public"),
                // and the caller would otherwise be told deploy "succeeded"
                // with nothing actually where it's expected. Fail loudly here
                // instead of swallowing it as a warning.
                if (i < 2) {
                    errorOut = QString("Failed to prepare schema \"%1\": %2\nSQL: %3")
                                   .arg(schemaName, q.lastError().text(), stmt);
                    db.close();
                    QSqlDatabase::removeDatabase(connName);
                    return false;
                }
                QString warn = QString("Warning on [%1]: %2").arg(stmt.left(60), q.lastError().text());
                Logger::instance().warn(warn);
                // Non-fatal — continue (duplicate table errors are OK)
            }
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    Logger::instance().info(QString("Schema '%1' deployed to Supabase successfully.").arg(schemaName));
    return true;
}

// ── Migrate all data from a local SQLite file into a Supabase schema ──────────
//
// Design decisions (matching the review):
//  1. FK-safe insertion order — each table is inserted only after the tables
//     it references have already been populated.
//  2. SERIAL sequence reset — after every table that has a SERIAL PK we call
//     setval() so auto-increment starts above the highest migrated id.
//  3. Boolean conversion — SQLite stores booleans as 0/1 integers; we convert
//     them to the string literals 'true'/'false' before binding to QPSQL so
//     the driver never sees an ambiguous integer for a BOOLEAN column.
//  4. Single transaction — BEGIN before the first INSERT, COMMIT at the end.
//     Any error triggers ROLLBACK so the cloud schema is never half-migrated.
//  5. INSERT … ON CONFLICT DO NOTHING — safe to re-run; existing rows are
//     kept intact so an interrupted migration can be retried without data loss.

bool SchemaDeployer::migrateFromSqlite(
        const QString& sqlitePath,
        const QString& pgConnStr,
        const QString& schemaName,
        std::function<void(const QString&, int, int)> progressCallback,
        QString& errorOut)
{
    // ── 1. Open SQLite source ─────────────────────────────────────────────────
    const QString srcConn = "migrate_src_" + schemaName;
    {
        QSqlDatabase src = QSqlDatabase::addDatabase("QSQLITE", srcConn);
        src.setDatabaseName(sqlitePath);
        if (!src.open()) {
            errorOut = "Cannot open SQLite file: " + src.lastError().text();
            QSqlDatabase::removeDatabase(srcConn);
            return false;
        }
    }

    // ── 2. Open PostgreSQL destination ────────────────────────────────────────
    QRegularExpression re(R"(postgres(?:ql)?://([^:]+):([^@]+)@([^:/]+):?(\d*)/(.+))");
    auto match = re.match(pgConnStr);
    if (!match.hasMatch()) {
        errorOut = "Invalid PostgreSQL connection string.";
        QSqlDatabase::removeDatabase(srcConn);
        return false;
    }

    const QString dstConn = "migrate_dst_" + schemaName;
    {
        QSqlDatabase dst = QSqlDatabase::addDatabase("QPSQL", dstConn);
        dst.setUserName(match.captured(1));
        dst.setPassword(match.captured(2));
        dst.setHostName(match.captured(3));
        dst.setPort(match.captured(4).isEmpty() ? 5432 : match.captured(4).toInt());
        dst.setDatabaseName(match.captured(5));
        dst.setConnectOptions("sslmode=require");
        if (!dst.open()) {
            errorOut = "Cannot connect to Supabase: " + dst.lastError().text();
            QSqlDatabase::removeDatabase(srcConn);
            QSqlDatabase::removeDatabase(dstConn);
            return false;
        }
    }

    // Helper lambdas that always use the named connections (avoids
    // accidentally hitting the app's primary connection).
    auto src = [&]() { return QSqlDatabase::database(srcConn); };
    auto dst = [&]() { return QSqlDatabase::database(dstConn); };

    // ── 3. Deploy schema using deployToPostgres (PascalCase quoted identifiers)
    QString deployError;
    if (!deployToPostgres(pgConnStr, schemaName, deployError)) {
        errorOut = "Failed to deploy schema: " + deployError;
        QSqlDatabase::removeDatabase(srcConn);
        QSqlDatabase::removeDatabase(dstConn);
        return false;
    }

    // ── 4. Ensure search_path is set for this connection
    const QString safe = escapeIdentifier(schemaName);
    {
        QSqlQuery q(dst());
        if (!q.exec(QString("SET SESSION search_path TO \"%1\"").arg(safe))) {
            errorOut = "Failed to set search_path: " + q.lastError().text();
            QSqlDatabase::removeDatabase(srcConn);
            QSqlDatabase::removeDatabase(dstConn);
            return false;
        }
    }

    // ── 5. Ordered table list (respects FK dependencies) ─────────────────────
    // Format: { tableName, hasSerialPk, booleanColumns }
    // Use lowercase to match PostgreSQL deployment (unquoted identifiers)
    struct TableSpec {
        QString  name;
        bool     hasSerialPk;
        QStringList boolCols;   // SQLite stores these as 0/1 — convert before INSERT
    };

    const QList<TableSpec> tables = {
        { "roles",                true,  { "canmanageusers","canviewreports","canaddcustomer","caneditcustomer",
                                           "candeletecustomer","canaddproduct","caneditproduct","candeleteproduct",
                                           "canaddorder","caneditorder","cancancelorder","candeleteorders",
                                           "canmanageregions","canaccesssettings","caneditorderdate","canaccessdashboard" } },
        { "regions",              true,  {} },
        { "categories",           true,  {} },
        { "branches",             true,  {} },
        { "users",                true,  {} },
        { "customers",            true,  {} },
        { "customerphones",       false, {} },
        { "customeraddresses",    false, {} },
        { "customerfavorites",    false, {} },
        { "products",             true,  {} },
        { "productpricehistory",  true,  {} },
        { "deliverydrivers",      true,  { "active" } },
        { "orderstatuses",        true,  {} },
        { "orders",               true,  {} },
        { "orderitems",           false, {} },
        { "scheduledorders",      true,  {} },
        { "scheduledorderitems",  false, {} },
        { "missingproducts",      true,  { "purchased" } },
        { "coupons",              true,  { "active" } },
        { "orderreturns",         true,  { "isfullreturn" } },
        { "returnitems",          true,  {} },
        { "auditlogs",            true,  {} },
    };

    // ── 5. Begin transaction via Qt API (not raw BEGIN) ──────────────────────
    // Raw "BEGIN" + prepare()+exec() breaks QPSQL's unnamed prepared statement
    // lifecycle. Using QSqlDatabase::transaction() lets the driver manage it.
    if (!dst().transaction()) {
        errorOut = "Could not start transaction: " + dst().lastError().text();
        QSqlDatabase::removeDatabase(srcConn);
        QSqlDatabase::removeDatabase(dstConn);
        return false;
    }

    auto rollback = [&]() {
        dst().rollback();
        QSqlDatabase::removeDatabase(srcConn);
        QSqlDatabase::removeDatabase(dstConn);
    };

    // ── 6. Migrate each table ─────────────────────────────────────────────────
    for (const TableSpec& tbl : tables) {

        // Count rows in source (SQLite uses PascalCase unquoted - case insensitive)
        QSqlQuery countQ(src());
        countQ.exec(QString("SELECT COUNT(*) FROM %1").arg(tbl.name));
        int total = 0;
        if (countQ.next()) total = countQ.value(0).toInt();
        if (total == 0) {
            if (progressCallback) progressCallback(tbl.name, 0, 0);
            continue;
        }

        // Fetch all columns from SQLite
        QSqlQuery srcQ(src());
        if (!srcQ.exec(QString("SELECT * FROM %1").arg(tbl.name))) {
            rollback();
            errorOut = QString("Failed to read %1 from SQLite: %2")
                           .arg(tbl.name, srcQ.lastError().text());
            return false;
        }

        QSqlRecord srcRec = srcQ.record();
        int srcColCount = srcRec.count();
        
        // Get destination table structure (PostgreSQL lowercase unquoted)
        QSqlQuery dstStructQ(dst());
        dstStructQ.exec(QString("SELECT * FROM \"%1\".%2 LIMIT 0").arg(safe, tbl.name));
        QSqlRecord dstRec = dstStructQ.record();
        
        // Build case-insensitive column mapping
        QMap<QString, QString> dstColMap; // lowercase -> actual destination name
        for (int i = 0; i < dstRec.count(); ++i) {
            QString actualName = dstRec.fieldName(i);
            dstColMap[actualName.toLower()] = actualName;
        }
        
        // Build column list: only columns that exist in BOTH source and destination
        QStringList dstColNames;    // Destination column names (lowercase)
        QList<int> colIndices;      // Indices in source record
        for (int i = 0; i < srcColCount; ++i) {
            QString srcColName = srcRec.fieldName(i);
            QString srcColLower = srcColName.toLower();
            
            if (dstColMap.contains(srcColLower)) {
                dstColNames << dstColMap[srcColLower];
                colIndices << i;
            }
        }
        
        if (dstColNames.isEmpty()) {
            if (progressCallback) progressCallback(tbl.name, 0, total);
            continue; // No compatible columns, skip table
        }
        
        int done = 0;
        // DO NOT use prepare() + reuse pattern with QPSQL inside transactions.
        // QPSQL's unnamed prepared statement lifecycle is fragile across multiple exec() calls.
        // Instead: prepare() fresh before EVERY exec() to avoid "unnamed prepared statement does not exist".

        while (srcQ.next()) {
            // Build VALUES list with properly escaped literals (avoid prepare/bind entirely)
            QStringList valueLiterals;
            for (int idx : colIndices) {
                QVariant val = srcQ.value(idx);
                const QString srcColName = srcRec.fieldName(idx);

                // Boolean conversion: 0/1 → true/false
                if (tbl.boolCols.contains(srcColName, Qt::CaseInsensitive)) {
                    bool b = (val.toInt() != 0);
                    valueLiterals << (b ? "true" : "false");
                }
                else if (val.isNull()) {
                    valueLiterals << "NULL";
                }
                else if (val.typeId() == QMetaType::Int || val.typeId() == QMetaType::LongLong) {
                    valueLiterals << QString::number(val.toLongLong());
                }
                else if (val.typeId() == QMetaType::Double) {
                    valueLiterals << QString::number(val.toDouble(), 'f', 10);
                }
                else {
                    // String: escape single quotes by doubling them (PostgreSQL standard)
                    QString s = val.toString();
                    s.replace("'", "''");
                    valueLiterals << QString("'%1'").arg(s);
                }
            }

            // Use fully-qualified table name (schema.table) with lowercase unquoted identifiers
            QString insertSqlFinal = QString("INSERT INTO \"%1\".%2 (%3) VALUES (%4) ON CONFLICT DO NOTHING")
                                         .arg(safe)
                                         .arg(tbl.name)
                                         .arg(dstColNames.join(", "))
                                         .arg(valueLiterals.join(", "));

            QSqlQuery dstQ(dst());
            if (!dstQ.exec(insertSqlFinal)) {
                rollback();
                errorOut = QString("Failed to insert into %1: %2\nSQL: %3")
                               .arg(tbl.name, dstQ.lastError().text(), insertSqlFinal);
                return false;
            }

            ++done;
            if (progressCallback && (done % 50 == 0 || done == total))
                progressCallback(tbl.name, done, total);
        }

        // ── SERIAL sequence reset ─────────────────────────────────────────────
        if (tbl.hasSerialPk) {
            QSqlQuery seqQ(dst());
            // pg_get_serial_sequence needs schema-qualified table name (lowercase unquoted)
            QString resetSql = QString(
                "SELECT setval("
                "  pg_get_serial_sequence('\"%1\".%2', 'id'),"
                "  COALESCE((SELECT MAX(id) FROM \"%1\".%2), 1)"
                ")"
            ).arg(safe, tbl.name);
            if (!seqQ.exec(resetSql)) {
                // Non-fatal — warn but continue (table may have no rows)
                Logger::instance().warn(
                    QString("Sequence reset warning for %1: %2")
                        .arg(tbl.name, seqQ.lastError().text()));
            }
        }

        if (progressCallback) progressCallback(tbl.name, total, total);
        Logger::instance().info(
            QString("Migrated %1: %2 row(s)").arg(tbl.name).arg(done));
    }

    // ── 7. Commit ─────────────────────────────────────────────────────────────
    if (!dst().commit()) {
        QString commitError = dst().lastError().text();
        Logger::instance().error(QString("COMMIT failed: %1").arg(commitError));
        dst().rollback();
        errorOut = "COMMIT failed: " + commitError + " (Check app.log for details)";
        QSqlDatabase::removeDatabase(srcConn);
        QSqlDatabase::removeDatabase(dstConn);
        return false;
    }

    Logger::instance().info(
        QString("Migration to schema '%1' completed successfully.").arg(schemaName));

    QSqlDatabase::removeDatabase(srcConn);
    QSqlDatabase::removeDatabase(dstConn);
    return true;
}
