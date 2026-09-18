#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include "infra/config_manager.h"
#include "infra/database_connection_manager.h"
#include "data/sqlite_repositories.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QDateTime>

// ──────────────────────────────────────────────────────────────────────────────
// Custom main — QCoreApplication is required by Qt6::Sql (QSqlDatabase).
// It must be created before any QSqlDatabase call.
// ──────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Suppress Qt platform warnings — we only use Core+Sql, no Gui.
    qputenv("QT_LOGGING_RULES", "qt.sql.*=false");

    QCoreApplication app(argc, argv);
    app.setApplicationName("DeliveryTests");

    return Catch::Session().run(argc, argv);
}

// ──────────────────────────────────────────────────────────────────────────────
// Helpers: open / close a fresh in-memory SQLite connection per test case
// ──────────────────────────────────────────────────────────────────────────────
static void openTestDb() {
    ConfigManager::instance().setDatabaseType("SQLite");
    ConfigManager::instance().setSqlitePath(":memory:");
    REQUIRE(DatabaseConnectionManager::instance().openConnection() == true);
    REQUIRE(DatabaseConnectionManager::instance().isSqliteFallbackActive() == false);
}

static void closeTestDb() {
    DatabaseConnectionManager::instance().closeConnection();
    const QString name = "default_delivery_connection";
    if (QSqlDatabase::contains(name)) {
        QSqlDatabase::removeDatabase(name);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// TEST CASES
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Category Repository CRUD", "[db][category]") {
    openTestDb();

    SQLiteCategoryRepository repo;
    Category cat;
    cat.name = "Beverages";

    REQUIRE(repo.save(cat) == true);
    REQUIRE(cat.id > 0);

    REQUIRE(repo.getById(cat.id).name == "Beverages");

    cat.name = "Soft Drinks";
    REQUIRE(repo.save(cat) == true);
    REQUIRE(repo.getById(cat.id).name == "Soft Drinks");

    REQUIRE(repo.getAll().size() == 1);
    REQUIRE(repo.remove(cat.id) == true);
    REQUIRE(repo.getById(cat.id).id == 0);

    closeTestDb();
}

TEST_CASE("Region Repository CRUD", "[db][region]") {
    openTestDb();

    SQLiteRegionRepository repo;
    Region reg;
    reg.name = "North Region";

    REQUIRE(repo.save(reg) == true);
    REQUIRE(reg.id > 0);
    REQUIRE(repo.getById(reg.id).name == "North Region");
    REQUIRE(repo.remove(reg.id) == true);
    REQUIRE(repo.getById(reg.id).id == 0);

    closeTestDb();
}

TEST_CASE("Customer Repository CRUD + search", "[db][customer]") {
    openTestDb();

    SQLiteRegionRepository regRepo;
    Region reg;
    reg.name = "Downtown";
    REQUIRE(regRepo.save(reg) == true);

    SQLiteCustomerRepository repo;
    Customer c;
    c.setName("John Doe");
    c.setDistanceKm(5.5);
    c.setRegionId(reg.id);
    c.setNotes("Regular customer");
    c.setStatus(Customer::Status::Active);
    c.setFirstContactDate(QDateTime::currentDateTime());
    c.addPhone(Phone{"1234567"});
    c.addPhone(Phone{"7654321"});
    c.addAddress(Address{"123 Main St"});

    REQUIRE(repo.save(c) == true);
    REQUIRE(c.id() > 0);

    Customer fetched = repo.getById(c.id());
    CHECK(fetched.name()           == "John Doe");
    CHECK(fetched.distanceKm()     == 5.5);
    CHECK(fetched.regionId()       == reg.id);
    CHECK(fetched.phones().size()    == 2);
    CHECK(fetched.addresses().size() == 1);

    CHECK(repo.search("John").size()  == 1);
    CHECK(repo.search("12345").size() == 1);

    REQUIRE(repo.remove(c.id()) == true);

    closeTestDb();
}

TEST_CASE("Product Repository CRUD + price history", "[db][product]") {
    openTestDb();

    SQLiteCategoryRepository catRepo;
    Category cat;
    cat.name = "Food";
    REQUIRE(catRepo.save(cat) == true);

    SQLiteProductRepository repo;
    Product p;
    p.setName("Pizza");
    p.setPrice(12.99);
    p.setCategoryId(cat.id);
    p.setStatus(Product::Status::Active);

    REQUIRE(repo.save(p) == true);
    REQUIRE(p.id() > 0);

    Product fetched = repo.getById(p.id());
    CHECK(fetched.name()  == "Pizza");
    CHECK(fetched.price() == 12.99);

    // Initial price history entry
    QList<ProductPriceHistory> history = repo.getPriceHistory(p.id());
    REQUIRE(history.size() == 1);
    CHECK(history[0].price == 12.99);

    // Update price → new history entry
    fetched.setPrice(14.99);
    REQUIRE(repo.save(fetched) == true);

    history = repo.getPriceHistory(p.id());
    REQUIRE(history.size() == 2);
    CHECK(history[0].price == 14.99);  // most recent first

    REQUIRE(repo.remove(p.id()) == true);

    closeTestDb();
}
