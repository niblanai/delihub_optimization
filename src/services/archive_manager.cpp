#include "archive_manager.h"
#include "services/session_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlField>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

static constexpr const char* kConn = "default_delivery_connection";

// ─────────────────────────────────────────────────────────────────────────────
ArchiveManager& ArchiveManager::instance() {
    static ArchiveManager inst;
    return inst;
}

// ── Ensure the archive table exists (idempotent) ──────────────────────────────
bool ArchiveManager::ensureTable() const {
    QSqlDatabase db = QSqlDatabase::database(kConn);
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    const bool isPsql = (db.driverName() == "QPSQL");

    // DDL is driver-specific: PostgreSQL uses SERIAL, SQLite uses AUTOINCREMENT.
    // All string columns use TEXT (compatible with both drivers).
    const QString ddl = isPsql
        ? QString("CREATE TABLE IF NOT EXISTS \"DeletedRecordsArchive\" ("
                  "  id           SERIAL      PRIMARY KEY,"
                  "  table_name   TEXT        NOT NULL,"
                  "  record_id    TEXT        NOT NULL,"
                  "  record_data  TEXT        NOT NULL,"
                  "  deleted_at   TEXT        NOT NULL,"
                  "  deleted_by   TEXT,"
                  "  restored_at  TEXT,"
                  "  restored_by  TEXT"
                  ")")
        : QString("CREATE TABLE IF NOT EXISTS DeletedRecordsArchive ("
                  "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "  table_name   TEXT    NOT NULL,"
                  "  record_id    TEXT    NOT NULL,"
                  "  record_data  TEXT    NOT NULL,"
                  "  deleted_at   TEXT    NOT NULL,"
                  "  deleted_by   TEXT,"
                  "  restored_at  TEXT,"
                  "  restored_by  TEXT"
                  ")");

    bool ok = q.exec(ddl);
    if (!ok)
        Logger::instance().error("ArchiveManager: failed to create table — "
                                 + q.lastError().text());
    return ok;
}

// ── softDelete ────────────────────────────────────────────────────────────────
int ArchiveManager::softDelete(const QString& tableName,
                               const QString& pkColumn,
                               int            pkValue,
                               QString*       errorOut)
{
    QSqlDatabase db = QSqlDatabase::database(kConn);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = "Database not open.";
        return -1;
    }

    // CRITICAL: Ensure search_path is set before any query (QPSQL can lose it)
    DatabaseConnectionManager::ensureSearchPath(db);

    if (!ensureTable()) {
        if (errorOut) *errorOut = "Could not create archive table.";
        return -1;
    }

    if (!db.transaction()) {
        if (errorOut) *errorOut = "Could not start transaction: " + db.lastError().text();
        return -1;
    }

    // ── 1. Read the full row as a map ──────────────────────────────────────────
    // Use unquoted table/column names (PostgreSQL converts to lowercase, SQLite is case-insensitive)
    QString selectSql = QString("SELECT * FROM %1 WHERE %2 = %3")
                            .arg(tableName, pkColumn)
                            .arg(pkValue);
    QSqlQuery selQ(db);
    if (!selQ.exec(selectSql) || !selQ.next()) {
        db.rollback();
        QString err = QString("Row %1=%2 not found in %3: ERROR: %4\nLINE 1: %5")
                          .arg(pkColumn).arg(pkValue).arg(tableName)
                          .arg(selQ.lastError().text())
                          .arg(selectSql);
        Logger::instance().error("ArchiveManager::softDelete — " + err);
        if (errorOut) *errorOut = err;
        return -1;
    }

    // Serialise all columns to JSON
    QJsonObject rowJson;
    const QSqlRecord rec = selQ.record();
    for (int i = 0; i < rec.count(); ++i) {
        const QSqlField f = rec.field(i);
        QVariant v = f.value();
        if (v.isNull())
            rowJson[f.name()] = QJsonValue::Null;
        else if (v.typeId() == QMetaType::Int || v.typeId() == QMetaType::LongLong)
            rowJson[f.name()] = v.toLongLong();
        else if (v.typeId() == QMetaType::Double)
            rowJson[f.name()] = v.toDouble();
        else
            rowJson[f.name()] = v.toString();
    }
    QString jsonStr = QJsonDocument(rowJson).toJson(QJsonDocument::Compact);
    selQ.finish();   // release before preparing the next query on this connection

    // ── 2. Insert into archive ─────────────────────────────────────────────────
    QString deletedBy = SessionManager::instance().currentUser().username();
    const bool isPsql = (db.driverName() == "QPSQL");
    const QString archiveTable = isPsql ? "\"DeletedRecordsArchive\"" : "DeletedRecordsArchive";

    // Escape single quotes in JSON string for PostgreSQL
    QString jsonEscaped = jsonStr;
    jsonEscaped.replace("'", "''");
    QString deletedByEscaped = deletedBy;
    deletedByEscaped.replace("'", "''");
    QString deletedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    deletedAt.replace("'", "''");

    QString insertSql = QString("INSERT INTO %1 "
                                "(table_name, record_id, record_data, deleted_at, deleted_by) "
                                "VALUES ('%2', '%3', '%4', '%5', '%6')")
                            .arg(archiveTable)
                            .arg(tableName)
                            .arg(QString::number(pkValue))
                            .arg(jsonEscaped)
                            .arg(deletedAt)
                            .arg(deletedByEscaped);

    QSqlQuery insQ(db);
    if (!insQ.exec(insertSql)) {
        db.rollback();
        QString err = "Archive insert failed: " + insQ.lastError().text();
        Logger::instance().error("ArchiveManager::softDelete — " + err);
        if (errorOut) *errorOut = err;
        return -1;
    }
    int archiveId = insQ.lastInsertId().toInt();
    insQ.finish();

    // ── 3. Hard-delete the original row ───────────────────────────────────────
    // Use unquoted table/column names for compatibility
    QString deleteSql = QString("DELETE FROM %1 WHERE %2 = %3")
                            .arg(tableName, pkColumn)
                            .arg(pkValue);
    QSqlQuery delQ(db);
    if (!delQ.exec(deleteSql)) {
        db.rollback();
        QString err = "Hard delete failed: " + delQ.lastError().text();
        Logger::instance().error("ArchiveManager::softDelete — " + err);
        if (errorOut) *errorOut = err;
        return -1;
    }

    db.commit();
    Logger::instance().info(QString("ArchiveManager: archived %1 #%2 → archive id %3")
                                .arg(tableName).arg(pkValue).arg(archiveId));
    return archiveId;
}

// ── restore ───────────────────────────────────────────────────────────────────
bool ArchiveManager::restore(int archiveId, QString* errorOut)
{
    QSqlDatabase db = QSqlDatabase::database(kConn);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = "Database not open.";
        return false;
    }

    // ── 1. Read the archive row ────────────────────────────────────────────────
    QSqlQuery archQ(db);
    const bool isPsql = (db.driverName() == "QPSQL");
    const QString archiveTable = isPsql ? "\"DeletedRecordsArchive\"" : "DeletedRecordsArchive";
    archQ.prepare(
        QString("SELECT table_name, record_id, record_data, restored_at "
                "FROM %1 WHERE id = ?").arg(archiveTable));
    archQ.addBindValue(archiveId);
    if (!archQ.exec() || !archQ.next()) {
        if (errorOut) *errorOut = "Archive record not found (id=" +
                                  QString::number(archiveId) + ").";
        return false;
    }
    QString tableName  = archQ.value(0).toString();
    QString recordId   = archQ.value(1).toString();
    QString jsonStr    = archQ.value(2).toString();
    QString restoredAt = archQ.value(3).toString();

    // Guard: don't restore the same archive row twice
    if (!restoredAt.isEmpty()) {
        if (errorOut) *errorOut =
            QString("This record was already restored on %1.").arg(restoredAt);
        return false;
    }

    // ── 2. Deserialise JSON ────────────────────────────────────────────────────
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) *errorOut = "Corrupt archive JSON: " + pe.errorString();
        return false;
    }
    QJsonObject obj = doc.object();
    archQ.finish();   // release before preparing the next query on this connection

    // ── 3. Build INSERT from the JSON keys ────────────────────────────────────
    //   We reconstruct the exact row with the original primary key.
    //   Use unquoted identifiers (PostgreSQL converts to lowercase, SQLite is case-insensitive)
    QStringList cols, placeholders;
    QList<QVariant> vals;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        cols         << it.key().toLower();  // Ensure lowercase for PostgreSQL
        placeholders << "?";
        const QJsonValue jv = it.value();
        if (jv.isNull())        vals << QVariant();
        else if (jv.isBool())   vals << QVariant(jv.toBool());
        else if (jv.isDouble()) vals << QVariant(jv.toDouble());
        else                    vals << QVariant(jv.toString());
    }

    QString insertSql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                            .arg(tableName,
                                 cols.join(", "),
                                 placeholders.join(", "));

    if (!db.transaction()) {
        if (errorOut) *errorOut = "Could not start restore transaction: " + db.lastError().text();
        return false;
    }
    QSqlQuery insQ(db);
    insQ.prepare(insertSql);
    for (const QVariant& v : vals) insQ.addBindValue(v);

    if (!insQ.exec()) {
        db.rollback();
        // Distinguish PK collision from other errors
        QString sqlErr = insQ.lastError().text();
        bool isPkConflict = sqlErr.contains("UNIQUE", Qt::CaseInsensitive)
                         || sqlErr.contains("PRIMARY KEY", Qt::CaseInsensitive);

        QString msg = isPkConflict
            ? QString("Cannot restore %1 #%2: the original primary key is now "
                      "used by a different record. The archive entry is preserved. "
                      "Please contact your administrator.")
                  .arg(tableName, recordId)
            : QString("Re-insert failed for %1 #%2: %3")
                  .arg(tableName, recordId, sqlErr);

        db.rollback();
        Logger::instance().error("ArchiveManager::restore — " + msg);
        if (errorOut) *errorOut = msg;
        return false;
    }

    // ── 4. Mark archive row as restored (keep it — permanent history) ──────────
    insQ.finish();
    QString restoredBy = SessionManager::instance().currentUser().username();
    QSqlQuery updQ(db);
    updQ.prepare(
        QString("UPDATE %1 "
                "SET restored_at = ?, restored_by = ? "
                "WHERE id = ?").arg(archiveTable));
    updQ.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updQ.addBindValue(restoredBy);
    updQ.addBindValue(archiveId);
    updQ.exec();   // non-fatal if this fails — the row is already restored

    db.commit();
    Logger::instance().info(QString("ArchiveManager: restored %1 #%2 from archive %3")
                                .arg(tableName, recordId).arg(archiveId));
    return true;
}

// ── archivedJson ─────────────────────────────────────────────────────────────
QString ArchiveManager::archivedJson(int archiveId) const {
    QSqlDatabase db = QSqlDatabase::database(kConn);
    const bool isPsql = (db.driverName() == "QPSQL");
    const QString archiveTable = isPsql ? "\"DeletedRecordsArchive\"" : "DeletedRecordsArchive";
    QSqlQuery q(db);
    q.prepare(QString("SELECT record_data FROM %1 WHERE id = ?").arg(archiveTable));
    q.addBindValue(archiveId);
    if (q.exec() && q.next()) return q.value(0).toString();
    return {};
}
