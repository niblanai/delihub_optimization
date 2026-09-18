#ifndef ARCHIVE_MANAGER_H
#define ARCHIVE_MANAGER_H

// ArchiveManager — Task 11
// ─────────────────────────────────────────────────────────────────────────────
// Replaces hard DELETE statements app-wide with a two-step soft-delete:
//
//   int archiveId = ArchiveManager::instance().softDelete("Customers", id);
//
// softDelete()  (a) reads the full row as a JSON blob
//               (b) inserts it into DeletedRecordsArchive
//               (c) then runs the hard DELETE
//               (d) returns the archive row's id (store this in the audit log
//                   so Undo can find the exact snapshot)
//
//   bool ok = ArchiveManager::instance().restore(archiveId, &errorMsg);
//
// restore()     (a) looks up the archive row
//               (b) deserialises JSON and re-inserts the original row with its
//                   original primary key
//               (c) sets restored_at / restored_by on the archive row
//               (d) returns true only after the re-insert actually succeeds —
//                   does NOT log success if the insert failed
//
// Primary-key collision handling
// ──────────────────────────────
// SQLite uses AUTOINCREMENT on all entity tables, which guarantees a deleted
// ID is never reused by a new insert.  However if (in some future migration)
// a row with the same ID was manually inserted after the deletion, the
// re-insert will hit a UNIQUE / PK constraint.  In that case restore() returns
// false and puts the conflict message in *errorOut so the caller can show it to
// the user instead of silently failing or overwriting the new row.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>
#include <QJsonObject>

class ArchiveManager {
public:
    static ArchiveManager& instance();

    // Soft-delete a row.
    // tableName  — exact SQL table name, e.g. "Customers", "Products"
    // pkColumn   — name of the primary key column, e.g. "Id"
    // pkValue    — value of the primary key for the row to delete
    // Returns the archive row id on success, -1 on failure.
    // errorOut (optional) receives a human-readable error string on failure.
    int softDelete(const QString& tableName,
                   const QString& pkColumn,
                   int            pkValue,
                   QString*       errorOut = nullptr);

    // Restore a previously soft-deleted row.
    // archiveId — the id returned by softDelete() (stored in the audit entry)
    // Returns true only if the row was actually re-inserted.
    bool restore(int archiveId, QString* errorOut = nullptr);

    // Returns the full JSON stored in the archive row (for display / debugging).
    QString archivedJson(int archiveId) const;

private:
    ArchiveManager() = default;
    bool ensureTable() const;   // creates DeletedRecordsArchive if missing
};

#endif // ARCHIVE_MANAGER_H
