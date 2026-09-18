#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QMutex>

class Logger {
public:
    enum class Level { Info, Warning, Error };

    static Logger& instance();

    void init(const QString& filePath);
    void log(Level level, const QString& message);

    void info(const QString& message);
    void warn(const QString& message);
    void error(const QString& message);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QString levelToString(Level level);

    // Use void* (plain FILE*) instead of QFile to avoid
    // Qt event-loop dependency that deadlocks on Windows 10 + Qt 6.8+
    void*  m_file        = nullptr;
    QMutex m_mutex;
    bool   m_initialized = false;
};

#endif // LOGGER_H
