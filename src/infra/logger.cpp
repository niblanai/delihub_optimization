#include "logger.h"
#include <QDebug>
#include <QDateTime>
#include <cstdio>
#include <cstring>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    QMutexLocker locker(&m_mutex);
    if (m_file) {
        fclose(static_cast<FILE*>(m_file));
        m_file = nullptr;
    }
}

void Logger::init(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    if (m_initialized) return;

    // Use plain fopen — avoids QFile's internal event-loop dependency
    // that can deadlock on Windows 10 with certain Qt 6.8+ builds
    FILE* f = fopen(filePath.toLocal8Bit().constData(), "a");
    if (f) {
        m_file = f;
        m_initialized = true;
        // Unlock before calling info() to avoid recursive lock
        locker.unlock();
        info("Logger initialized. Log file: " + filePath);
    } else {
        qWarning() << "Logger: failed to open log file:" << filePath;
    }
}

void Logger::log(Level level, const QString& message) {
    QMutexLocker locker(&m_mutex);

    QString levelStr = levelToString(level);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, levelStr, message);

    // Console output
    if (level == Level::Error)
        qCritical().noquote() << logLine;
    else if (level == Level::Warning)
        qWarning().noquote() << logLine;
    else
        qDebug().noquote() << logLine;

    // File output via plain FILE*
    if (m_initialized && m_file) {
        FILE* f = static_cast<FILE*>(m_file);
        fprintf(f, "%s\n", logLine.toLocal8Bit().constData());
        fflush(f);
    }
}

void Logger::info(const QString& message)  { log(Level::Info,    message); }
void Logger::warn(const QString& message)  { log(Level::Warning, message); }
void Logger::error(const QString& message) { log(Level::Error,   message); }

QString Logger::levelToString(Level level) {
    switch (level) {
    case Level::Info:    return "INFO";
    case Level::Warning: return "WARN";
    case Level::Error:   return "ERROR";
    }
    return "UNKNOWN";
}
