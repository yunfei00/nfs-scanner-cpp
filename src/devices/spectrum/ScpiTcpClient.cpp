#include "devices/spectrum/ScpiTcpClient.h"

#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QTcpSocket>

#include <algorithm>

namespace NFSScanner::Devices::Spectrum {

ScpiTcpClient::ScpiTcpClient(QObject *parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
{
}

bool ScpiTcpClient::connectToHost(const QString &host, quint16 port, int timeoutMs)
{
    lastError_.clear();
    if (host.trimmed().isEmpty()) {
        setError(QStringLiteral("SCPI host 为空。"));
        return false;
    }

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }

    emit logMessage(QStringLiteral("SCPI 连接：%1:%2").arg(host).arg(port));
    socket_->connectToHost(host, port);
    if (!socket_->waitForConnected(std::max(1, timeoutMs))) {
        setError(QStringLiteral("SCPI 连接失败：%1").arg(socket_->errorString()));
        socket_->abort();
        return false;
    }

    emit logMessage(QStringLiteral("SCPI 已连接：%1:%2").arg(host).arg(port));
    emit logMessage(QStringLiteral("SCPI 连接状态：ConnectedState"));
    return true;
}

void ScpiTcpClient::disconnectFromHost()
{
    if (socket_->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    socket_->disconnectFromHost();
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->waitForDisconnected(1000);
    }
    emit logMessage(QStringLiteral("SCPI 连接状态：UnconnectedState"));
    emit logMessage(QStringLiteral("SCPI 已断开。"));
}

bool ScpiTcpClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void ScpiTcpClient::clearBuffer()
{
    if (!socket_) {
        return;
    }

    while (socket_->bytesAvailable() > 0) {
        socket_->readAll();
    }
}

bool ScpiTcpClient::writeCommand(const QString &command)
{
    lastError_.clear();
    if (!isConnected()) {
        setError(QStringLiteral("SCPI 未连接，无法发送命令。"));
        return false;
    }

    QByteArray payload = command.trimmed().toUtf8();
    payload.append('\n');
    emit logMessage(QStringLiteral("SCPI 发送：%1").arg(QString::fromUtf8(payload).trimmed()));

    const qint64 written = socket_->write(payload);
    if (written != payload.size()) {
        setError(QStringLiteral("SCPI 写入失败：%1").arg(socket_->errorString()));
        return false;
    }
    if (!socket_->waitForBytesWritten(1000)) {
        setError(QStringLiteral("SCPI 写入超时：%1").arg(socket_->errorString()));
        return false;
    }
    return true;
}

QString ScpiTcpClient::queryString(const QString &command, int timeoutMs)
{
    const QByteArray bytes = queryBinaryOrText(command, timeoutMs);
    return QString::fromUtf8(bytes).trimmed();
}

QByteArray ScpiTcpClient::queryBinaryOrText(const QString &command, int timeoutMs)
{
    lastError_.clear();
    clearBuffer();
    if (!writeCommand(command)) {
        return {};
    }

    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    const int safeTimeout = std::max(1, timeoutMs);

    while (timer.elapsed() < safeTimeout) {
        const int remaining = std::max(1, safeTimeout - static_cast<int>(timer.elapsed()));
        if (!socket_->waitForReadyRead(std::min(remaining, 250))) {
            if (!result.isEmpty()) {
                break;
            }
            continue;
        }

        result.append(socket_->readAll());
        while (socket_->waitForReadyRead(50)) {
            result.append(socket_->readAll());
        }

        if (result.contains('\n')) {
            break;
        }
    }

    if (result.isEmpty()) {
        setError(QStringLiteral("SCPI query timeout: %1").arg(command));
    }
    return result;
}

QString ScpiTcpClient::lastError() const
{
    return lastError_;
}

void ScpiTcpClient::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
