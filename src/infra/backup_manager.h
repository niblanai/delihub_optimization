#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include <QString>

class BackupManager {
public:
    static BackupManager& instance();

    bool backupDatabase(const QString& backupFilePath, QString& errorMessage);
    bool restoreDatabase(const QString& backupFilePath, QString& errorMessage);

private:
    BackupManager() = default;
    ~BackupManager() = default;
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    QString getActiveDatabasePath() const;
};

#endif // BACKUP_MANAGER_H
