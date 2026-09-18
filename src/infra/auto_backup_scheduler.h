#ifndef AUTO_BACKUP_SCHEDULER_H
#define AUTO_BACKUP_SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QTime>

// Runs a database backup once per day at a configurable time.
// Configure via config.ini:
//   [Backup]
//   DailyTime=02:00     (24h format)
//   AutoDir=C:/Backups
class AutoBackupScheduler : public QObject {
    Q_OBJECT
public:
    static AutoBackupScheduler& instance();
    void start();
    void stop();
    void runBackup();   // call manually to trigger an immediate backup

signals:
    void backupCompleted(const QString& path);
    void backupFailed(const QString& error);

private slots:
    void onTimer();

private:
    AutoBackupScheduler() = default;
    QTimer* m_timer = nullptr;
};

#endif // AUTO_BACKUP_SCHEDULER_H
