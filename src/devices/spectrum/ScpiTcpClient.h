#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class QTcpSocket;

namespace NFSScanner::Devices::Spectrum {

class ScpiTcpClient final : public QObject
{
    Q_OBJECT

public:
    explicit ScpiTcpClient(QObject *parent = nullptr);

    bool connectToHost(const QString &host, quint16 port, int timeoutMs = 3000);
    void disconnectFromHost();
    bool isConnected() const;

    bool writeCommand(const QString &command);
    QString queryString(const QString &command, int timeoutMs = 5000);
    QByteArray queryBinaryOrText(const QString &command, int timeoutMs = 10000);

    QString lastError() const;

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private:
    void setError(const QString &message);

    QTcpSocket *socket_ = nullptr;
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
