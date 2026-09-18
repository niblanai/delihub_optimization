#include <cstdio>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

int main(int argc, char* argv[]) {
    fprintf(stdout, "Step 1: before QCoreApplication\n");
    fflush(stdout);

    QCoreApplication app(argc, argv);

    fprintf(stdout, "Step 2: QCoreApplication created\n");
    fflush(stdout);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "diag");
    db.setDatabaseName(":memory:");

    fprintf(stdout, "Step 3: addDatabase done\n");
    fflush(stdout);

    if (!db.open()) {
        fprintf(stdout, "ERROR: db.open failed: %s\n", db.lastError().text().toUtf8().constData());
        return 1;
    }

    fprintf(stdout, "Step 4: db.open succeeded\n");
    fflush(stdout);

    QSqlQuery q(db);
    q.exec("CREATE TABLE test (id INTEGER PRIMARY KEY, val TEXT)");
    q.prepare("INSERT INTO test (val) VALUES (?)");
    q.addBindValue("hello");
    bool ok = q.exec();

    fprintf(stdout, "Step 5: INSERT %s\n", ok ? "OK" : "FAILED");
    fflush(stdout);

    db.close();
    QSqlDatabase::removeDatabase("diag");

    fprintf(stdout, "Step 6: DONE\n");
    fflush(stdout);
    return 0;
}
