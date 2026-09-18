#ifndef NOTIFICATION_SOUND_PLAYER_H
#define NOTIFICATION_SOUND_PLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "notification_service.h"

// Subscribes to NotificationService and plays notification.mp3
// whenever a ScheduledOrderDueToday event fires.
// Resolved from the app directory — same approach as intro.mp4.
// Construct once (e.g. in MainWindow) and keep alive for the session.
class NotificationSoundPlayer : public QObject {
    Q_OBJECT
public:
    explicit NotificationSoundPlayer(QObject* parent = nullptr);
    ~NotificationSoundPlayer() override;
    bool soundFound() const { return m_soundFound; }

private:
    void playSound();

    QMediaPlayer* m_player     = nullptr;
    QAudioOutput* m_audio      = nullptr;
    bool          m_soundFound = false;
    int           m_subHandle  = 0;
};

#endif // NOTIFICATION_SOUND_PLAYER_H
