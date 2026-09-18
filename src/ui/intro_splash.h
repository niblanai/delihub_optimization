#ifndef INTRO_SPLASH_H
#define INTRO_SPLASH_H

#include <QDialog>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QAudioOutput>

// Frameless full-screen video splash.
// Shows the intro video, auto-closes when it ends.
// No frame, no title bar, no skip button.
class IntroSplash : public QDialog {
    Q_OBJECT
public:
    explicit IntroSplash(const QString& videoPath, QWidget* parent = nullptr);
    bool isValid() const;

private:
    QMediaPlayer* m_player = nullptr;
    QVideoWidget* m_video  = nullptr;
    bool          m_valid  = false;
};

#endif // INTRO_SPLASH_H
