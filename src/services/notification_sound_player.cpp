#include "notification_sound_player.h"
#include <QCoreApplication>
#include <QFile>
#include <QUrl>

NotificationSoundPlayer::NotificationSoundPlayer(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(1.0f);

    // Resolve notification.mp3 relative to the app binary directory
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/notification.mp3",
        appDir + "/../src/notification.mp3"  // dev machine fallback
    };
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            m_player->setSource(QUrl::fromLocalFile(path));
            m_soundFound = true;
            break;
        }
    }

    // Subscribe to notification events
    m_subHandle = NotificationService::instance().subscribe(
        [this](const Notification& n) {
            if (n.event == NotificationEvent::ScheduledOrderDueToday)
                playSound();
        });
}

NotificationSoundPlayer::~NotificationSoundPlayer() {
    NotificationService::instance().unsubscribe(m_subHandle);
}

void NotificationSoundPlayer::playSound() {
    if (!m_soundFound) return;
    m_player->stop();
    m_player->setPosition(0);
    m_player->play();
}
