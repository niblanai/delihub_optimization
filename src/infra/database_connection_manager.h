#ifndef DATABASE_CONNECTION_MANAGER_H
#define DATABASE_CONNECTION_MANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlError>
#include <QObject>
#include <QTimer>

class DatabaseConnectionManager : public QObject {
    Q_OBJECT
    
public:
    static DatabaseConnectionManager& instance();

    bool openConnection();
    void closeConnection();
    bool isSqliteFallbackActive() const;
    QString lastError() const;
    QString connectionType() const;  // "QSQLITE" or "QODBC"
    
    // Executes DDL statements to ensure all tables exist
    bool initializeSchema();
    
    // Ensures search_path is set correctly for the active branch (QPSQL only).
    // Call this before any query on a QPSQL connection to avoid "relation does not exist" errors.
    // This is a workaround for QPSQL connections that may lose search_path after certain operations.
    static void ensureSearchPath(QSqlDatabase db);
    
    // Check if connection is still alive (for cloud databases)
    bool isConnectionAlive();
    
    // Connection health monitoring (call periodically for cloud databases)
    void startConnectionMonitor();
    void stopConnectionMonitor();

private:
    DatabaseConnectionManager() : QObject(nullptr) {}
    ~DatabaseConnectionManager() { stopConnectionMonitor(); }
    DatabaseConnectionManager(const DatabaseConnectionManager&) = delete;
    DatabaseConnectionManager& operator=(const DatabaseConnectionManager&) = delete;

    bool openPostgresConnection(const QString& connStr);
    bool openOdbcConnection();
    bool openSqliteConnection();
    
private slots:
    void checkConnectionHealth();  // Periodic health check

private:
    bool m_sqliteFallbackActive = false;
    QString m_lastError;
    const QString m_connectionName = "default_delivery_connection";
    QTimer* m_healthCheckTimer = nullptr;
    bool m_isCloudConnection = false;
};

#endif // DATABASE_CONNECTION_MANAGER_H
