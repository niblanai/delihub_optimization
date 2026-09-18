#ifndef SCHEMA_DEPLOYER_H
#define SCHEMA_DEPLOYER_H

#include <QString>
#include <functional>

// Utility class to create the DeliHub schema on a PostgreSQL (Supabase) database.
// Each branch gets its own schema derived from the branch name.
class SchemaDeployer {
public:
    // Deploy (create) the schema + all tables on the given connection.
    // schemaName: sanitized name e.g. "حسام", "main_branch"
    // Returns true on success.
    static bool deployToPostgres(const QString& connectionString,
                                  const QString& schemaName,
                                  QString& errorOut);

    // Migrate all data from a local SQLite file into an existing (or new)
    // Supabase schema. Safe to run on a schema that already has data —
    // uses INSERT ... ON CONFLICT DO NOTHING so existing rows are preserved.
    //
    // The entire migration runs inside a single BEGIN/COMMIT transaction.
    // If anything fails mid-way the whole operation is rolled back so the
    // cloud schema is never left in a half-migrated state.
    //
    // progressCallback(tableName, rowsDone, rowsTotal) — called after each
    // table is finished so the UI can update a progress bar. Pass nullptr
    // if you don't need progress reporting.
    //
    // Returns true on success; errorOut is populated on failure.
    static bool migrateFromSqlite(const QString& sqlitePath,
                                   const QString& pgConnStr,
                                   const QString& schemaName,
                                   std::function<void(const QString&, int, int)> progressCallback,
                                   QString& errorOut);

    // Derives a stable Postgres schema identifier directly from the
    // human-entered branch name (Arabic or Latin, with spaces, etc.)
    static QString sanitizeSchemaName(const QString& branchName);
};

#endif // SCHEMA_DEPLOYER_H
