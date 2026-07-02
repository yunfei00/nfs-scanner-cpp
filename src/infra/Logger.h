#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace NFSScanner::Infra {

class Logger final : public QObject
{
    Q_OBJECT

public:
    enum class Level {
        Info,
        Warning,
        Error
    };

    explicit Logger(QObject *parent = nullptr);

    static QStringList recentLines(int maxCount = 100);
    static void appendGlobalHistory(const QString &line);

    static QString format(Level level, const QString &message);

public slots:
    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);

signals:
    void messageReady(const QString &message);

private:
    void emitMessage(Level level, const QString &message);
};

} // namespace NFSScanner::Infra
