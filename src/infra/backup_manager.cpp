#include "backup_manager.h"
#include "config_manager.h"
#include "logger.h"
#include "database_connection_manager.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>
#include <QCoreApplication>

BackupManager& BackupManager::instance() {
    static BackupManager inst;
    return inst;
}

QString BackupManager::getActiveDatabasePath() const {
    QString dbType = ConfigManager::instance().databaseType();
    if (dbType.compare("SQLite", Qt::CaseInsensitive) == 0 ||
        dbType.compare("QSQLITE", Qt::CaseInsensitive) == 0) {

        QString path = ConfigManager::instance().sqlitePath();

        // If the path is relative, resolve it relative to the exe directory
        QFileInfo fi(path);
        if (fi.isRelative()) {
            QString exeDir = QCoreApplication::applicationDirPath();
            if (exeDir.isEmpty())
                exeDir = QDir::currentPath();
            path = QDir(exeDir).filePath(path);
        }
        return path;
    } else {
        // ODBC connection string — extract DBQ= path
        QString connStr = ConfigManager::instance().odbcConnectionString();
        QRegularExpression regex("DBQ=([^;]+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = regex.match(connStr);
        if (match.hasMatch())
            return match.captured(1).trimmed();
    }
    return "";
}

bool BackupManager::backupDatabase(const QString& backupFilePath, QString& errorMessage) {
    QString sourcePath = getActiveDatabasePath();
    if (sourcePath.isEmpty()) {
        errorMessage = "Active database path could not be resolved.";
        Logger::instance().error("Backup failed: " + errorMessage);
        return false;
    }

    QFileInfo srcInfo(sourcePath);
    if (!srcInfo.exists()) {
        errorMessage = QString("Database file does not exist at: %1").arg(sourcePath);
        Logger::instance().error("Backup failed: " + errorMessage);
        return false;
    }

    Logger::instance().info(QString("Starting backup from %1 to %2").arg(sourcePath, backupFilePath));

    // Close connections to release file locks
    DatabaseConnectionManager::instance().closeConnection();

    // Perform copy
    if (QFile::exists(backupFilePath)) {
        if (!QFile::remove(backupFilePath)) {
            errorMessage = "Failed to remove existing backup file at destination.";
            Logger::instance().error("Backup failed: " + errorMessage);
            DatabaseConnectionManager::instance().openConnection(); // Re-open
            return false;
        }
    }

    // Ensure destination directory exists
    QFileInfo backupInfo(backupFilePath);
    QDir().mkpath(backupInfo.absolutePath());

    bool success = QFile::copy(sourcePath, backupFilePath);
    
    // Re-open connection
    if (!DatabaseConnectionManager::instance().openConnection()) {
        Logger::instance().warn("Failed to re-open database connection after backup.");
    }

    if (!success) {
        errorMessage = "Failed to copy database file to destination.";
        Logger::instance().error("Backup failed: " + errorMessage);
        return false;
    }

    Logger::instance().info("Backup completed successfully.");
    return true;
}

bool BackupManager::restoreDatabase(const QString& backupFilePath, QString& errorMessage) {
    QString destPath = getActiveDatabasePath();
    if (destPath.isEmpty()) {
        errorMessage = "Active database path could not be resolved.";
        Logger::instance().error("Restore failed: " + errorMessage);
        return false;
    }

    if (!QFile::exists(backupFilePath)) {
        errorMessage = QString("Backup file does not exist at: %1").arg(backupFilePath);
        Logger::instance().error("Restore failed: " + errorMessage);
        return false;
    }

    Logger::instance().info(QString("Starting restore from %1 to %2").arg(backupFilePath, destPath));

    // Close connections to release file locks
    DatabaseConnectionManager::instance().closeConnection();

    // Remove active database file
    if (QFile::exists(destPath)) {
        if (!QFile::remove(destPath)) {
            errorMessage = "Failed to remove active database file before restore.";
            Logger::instance().error("Restore failed: " + errorMessage);
            DatabaseConnectionManager::instance().openConnection(); // Re-open
            return false;
        }
    }

    // Ensure destination directory exists
    QFileInfo destInfo(destPath);
    QDir().mkpath(destInfo.absolutePath());

    bool success = QFile::copy(backupFilePath, destPath);

    // Re-open connection
    if (!DatabaseConnectionManager::instance().openConnection()) {
        errorMessage = "Database restored, but failed to re-open database connection: " + 
                       DatabaseConnectionManager::instance().lastError();
        Logger::instance().error("Restore error: " + errorMessage);
        return false;
    }

    if (!success) {
        errorMessage = "Failed to copy backup file to active database path.";
        Logger::instance().error("Restore failed: " + errorMessage);
        return false;
    }

    Logger::instance().info("Restore completed successfully.");
    return true;
}
