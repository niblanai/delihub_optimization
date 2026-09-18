#include "auto_backup_scheduler.h"
#include "backup_manager.h"
#include "config_manager.h"
#include "logger.h"
#include <QDir>
#include <QDateTime>
#include <QSettings>

AutoBackupScheduler& AutoBackupScheduler::instance() {
    static AutoBackupScheduler inst;
    return inst;
}

void AutoBackupScheduler::start() {
    if (m_timer) return;
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AutoBackupScheduler::onTimer);
    m_timer->start(60 * 60 * 1000);  // check every hour
    Logger::instance().info("AutoBackup scheduler started.");
}

void AutoBackupScheduler::stop() {
    if (m_timer) m_timer->stop();
}

void AutoBackupScheduler::onTimer() {
    QString timeStr = ConfigManager::instance().autoBackupTime();
    QTime backupTime = QTime::fromString(timeStr.isEmpty() ? "02:00" : timeStr, "HH:mm");
    if (!backupTime.isValid()) backupTime = QTime(2, 0);

    QTime now = QTime::currentTime();
    if (qAbs(now.secsTo(backupTime)) > 300) return;  // within 5 min window

    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (ConfigManager::instance().lastAutoBackupDate() == today) return;

    runBackup();
}

void AutoBackupScheduler::runBackup() {
    QString backupDir = ConfigManager::instance().autoBackupDir();
    if (backupDir.isEmpty())
        backupDir = QDir::homePath() + "/DeliHub_Backups";
    QDir().mkpath(backupDir);

    QString filename = QString("auto_backup_%1.db")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmm"));
    QString path = backupDir + "/" + filename;

    QString err;
    if (BackupManager::instance().backupDatabase(path, err)) {
        Logger::instance().info("Auto-backup completed: " + path);
        ConfigManager::instance().setLastAutoBackupDate(
            QDate::currentDate().toString("yyyy-MM-dd"));
        emit backupCompleted(path);
    } else {
        Logger::instance().error("Auto-backup failed: " + err);
        emit backupFailed(err);
    }
}
