#include "intro_splash.h"
#include <QVBoxLayout>
#include <QFile>
#include <QUrl>
#include <QScreen>
#include <QApplication>
#include <QAudioOutput>
#include <QTimer>

IntroSplash::IntroSplash(const QString& videoPath, QWidget* parent)
    : QDialog(parent,
              Qt::FramelessWindowHint |
              Qt::WindowStaysOnTopHint |
              Qt::Tool)          // Tool = no taskbar entry, no title bar
{
    m_valid = QFile::exists(videoPath);
    if (!m_valid) return;

    setModal(true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    // Size: 720×480 centered on screen (medium box, not fullscreen)
    resize(720, 480);
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move(sg.center() - rect().center());
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_video = new QVideoWidget;
    m_video->setStyleSheet("background:black;");
    m_video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_video);

    m_player = new QMediaPlayer(this);
    auto* audioOut = new QAudioOutput(this);
    audioOut->setVolume(0.8f);
    m_player->setAudioOutput(audioOut);
    m_player->setVideoOutput(m_video);
    m_player->setSource(QUrl::fromLocalFile(videoPath));

    // Close when video ends
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
        [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                m_player->stop();
                accept();
            }
        });

    // Close on any error so we never block the app
    connect(m_player, &QMediaPlayer::errorOccurred, this,
        [this](QMediaPlayer::Error, const QString&) {
            accept();
        });

    // Fallback timeout: close after video duration + 2s buffer
    // In case EndOfMedia doesn't fire on this platform
    QTimer::singleShot(8000, this, [this]() {
        if (isVisible()) {
            m_player->stop();
            accept();
        }
    });

    // Brief delay so dialog is fully painted before playback starts
    QTimer::singleShot(200, this, [this]() {
        m_player->play();
    });
}

bool IntroSplash::isValid() const { return m_valid; }
