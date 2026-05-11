#include "devices/motion/SerialMotionController.h"

#include <QIODevice>
#include <QStringList>

#include <cmath>

namespace NFSScanner::Devices::Motion {

namespace {

QString mmText(double value)
{
    return QString::number(value, 'f', 3);
}

QString feedText(double feed)
{
    const double rounded = std::round(feed);
    if (std::abs(feed - rounded) < 0.000001) {
        return QString::number(static_cast<int>(rounded));
    }
    return QString::number(feed, 'f', 3);
}

QString positionSummary(const MotionPosition &position)
{
    return QStringLiteral("X=%1 Y=%2 Z=%3")
        .arg(mmText(position.x), mmText(position.y), mmText(position.z));
}

} // namespace

SerialMotionController::SerialMotionController(QObject *parent)
    : IMotionController(parent)
{
    connect(&serial_, &QSerialPort::readyRead, this, &SerialMotionController::handleReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred, this, &SerialMotionController::handleSerialError);
}

SerialMotionController::~SerialMotionController()
{
    if (serial_.isOpen()) {
        closePort();
    }
}

QString SerialMotionController::name() const
{
    return QStringLiteral("Qt SerialPort Motion Controller");
}

bool SerialMotionController::connectDevice()
{
    if (lastPortName_.isEmpty()) {
        const QString message = QStringLiteral("未指定串口，无法连接运动控制器。");
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    return openPort(lastPortName_, lastBaudRate_);
}

void SerialMotionController::disconnectDevice()
{
    closePort();
}

bool SerialMotionController::isConnected() const
{
    return isOpen();
}

bool SerialMotionController::moveTo(double xMm, double yMm, double zMm)
{
    return moveAbs(std::optional<double>{xMm},
                   std::optional<double>{yMm},
                   std::optional<double>{zMm},
                   1000.0);
}

bool SerialMotionController::openPort(const QString &portName, int baudRate)
{
    const QString trimmedPort = portName.trimmed();
    if (trimmedPort.isEmpty()) {
        const QString message = QStringLiteral("端口名称为空，请先选择串口。");
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    if (baudRate <= 0) {
        const QString message = QStringLiteral("波特率无效：%1").arg(baudRate);
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    if (serial_.isOpen()) {
        closePort();
    }

    serial_.setPortName(trimmedPort);
    serial_.setBaudRate(baudRate);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial_.open(QIODevice::ReadWrite)) {
        const QString message = serialErrorMessage(serial_.error());
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    readBuffer_.clear();
    lastPortName_ = trimmedPort;
    lastBaudRate_ = baudRate;
    currentStatus_ = QStringLiteral("串口已连接");

    emit connectedChanged(true);
    emit connectionChanged(true);
    emit statusChanged(currentStatus_);
    emit logMessage(QStringLiteral("串口已连接：%1，%2").arg(trimmedPort).arg(baudRate));
    return true;
}

void SerialMotionController::closePort()
{
    if (serial_.isOpen()) {
        serial_.close();
    }
    readBuffer_.clear();
    currentStatus_ = QStringLiteral("串口已关闭");
    emit connectedChanged(false);
    emit connectionChanged(false);
    emit statusChanged(currentStatus_);
    emit logMessage(QStringLiteral("串口已关闭。"));
}

bool SerialMotionController::isOpen() const
{
    return serial_.isOpen();
}

bool SerialMotionController::home()
{
    return sendCommand(QStringLiteral("$H"));
}

bool SerialMotionController::queryPosition()
{
    return sendCommand(QStringLiteral("?"));
}

bool SerialMotionController::readVersion()
{
    return sendCommand(QStringLiteral("$I"));
}

bool SerialMotionController::readHelp()
{
    return sendCommand(QStringLiteral("$"));
}

bool SerialMotionController::moveAbs(std::optional<double> x,
                                     std::optional<double> y,
                                     std::optional<double> z,
                                     double feed)
{
    if (feed <= 0.0) {
        const QString message = QStringLiteral("Feed 速度无效：%1").arg(feedText(feed));
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    MotionPosition target = position_;
    if (x.has_value()) {
        target.x = *x;
    }
    if (y.has_value()) {
        target.y = *y;
    }
    if (z.has_value()) {
        target.z = *z;
    }

    if (!validatePosition(target)) {
        return false;
    }

    const QString command = QStringLiteral("G1X%1Y%2Z%3F%4")
                                .arg(mmText(target.x),
                                     mmText(target.y),
                                     mmText(target.z),
                                     feedText(feed));
    return sendCommand(command);
}

bool SerialMotionController::jogAxis(const QString &axis, double delta, double feed)
{
    const QString normalizedAxis = axis.trimmed().toUpper();
    MotionPosition target = position_;

    if (normalizedAxis == QStringLiteral("X")) {
        target.x += delta;
    } else if (normalizedAxis == QStringLiteral("Y")) {
        target.y += delta;
    } else if (normalizedAxis == QStringLiteral("Z")) {
        target.z += delta;
    } else {
        const QString message = QStringLiteral("不支持的点动轴：%1").arg(axis);
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    return moveAbs(target.x, target.y, target.z, feed);
}

MotionPosition SerialMotionController::currentPosition() const
{
    return position_;
}

QString SerialMotionController::currentStatus() const
{
    return currentStatus_;
}

bool SerialMotionController::sendCommand(const QString &command)
{
    if (!serial_.isOpen()) {
        const QString message = QStringLiteral("串口未打开，命令未发送：%1").arg(command);
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    const QByteArray payload = command.toUtf8() + '\n';
    const qint64 written = serial_.write(payload);
    if (written < 0 || written != payload.size()) {
        const QString message = QStringLiteral("命令写入失败：%1，原因：%2")
                                    .arg(command, serial_.errorString());
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    serial_.flush();
    emit logMessage(QStringLiteral("发送命令：%1").arg(command));
    return true;
}

void SerialMotionController::handleReadyRead()
{
    readBuffer_.append(serial_.readAll());

    int newlineIndex = readBuffer_.indexOf('\n');
    while (newlineIndex >= 0) {
        const QByteArray lineBytes = readBuffer_.left(newlineIndex);
        readBuffer_.remove(0, newlineIndex + 1);
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (!line.isEmpty()) {
            processLine(line);
        }
        newlineIndex = readBuffer_.indexOf('\n');
    }
}

void SerialMotionController::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) {
        return;
    }

    const QString message = serialErrorMessage(error);
    emit logMessage(QStringLiteral("串口异常：%1").arg(message));
    emit errorOccurred(message);

    if (error == QSerialPort::ResourceError
        || error == QSerialPort::DeviceNotFoundError
        || error == QSerialPort::PermissionError) {
        if (serial_.isOpen()) {
            serial_.close();
        }
        currentStatus_ = QStringLiteral("串口异常");
        emit connectedChanged(false);
        emit connectionChanged(false);
        emit statusChanged(currentStatus_);
    }
}

void SerialMotionController::processLine(const QString &line)
{
    emit rawLineReceived(line);

    const QString lowerLine = line.toLower();
    if (line.startsWith(QLatin1Char('<')) && line.endsWith(QLatin1Char('>'))) {
        parseStatusLine(line);
        return;
    }

    if (lowerLine == QStringLiteral("ok")) {
        emit logMessage(QStringLiteral("控制器返回 ok"));
        return;
    }

    if (lowerLine.startsWith(QStringLiteral("error:"))) {
        const QString message = QStringLiteral("控制器返回错误：%1").arg(line);
        emit logMessage(message);
        emit errorOccurred(message);
        return;
    }

    emit logMessage(QStringLiteral("控制器返回：%1").arg(line));
}

bool SerialMotionController::parseStatusLine(const QString &line)
{
    const QString payload = line.mid(1, line.size() - 2);
    const QStringList fields = payload.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    if (fields.isEmpty()) {
        const QString message = QStringLiteral("返回格式解析失败：%1").arg(line);
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    }

    const QString status = fields.first();
    for (const QString &field : fields) {
        if (!field.startsWith(QStringLiteral("MPos:"))) {
            continue;
        }

        const QStringList values = field.mid(5).split(QLatin1Char(','));
        if (values.size() != 3) {
            break;
        }

        bool xOk = false;
        bool yOk = false;
        bool zOk = false;
        const double x = values.at(0).toDouble(&xOk);
        const double y = values.at(1).toDouble(&yOk);
        const double z = values.at(2).toDouble(&zOk);

        if (!xOk || !yOk || !zOk) {
            break;
        }

        position_ = MotionPosition{x, y, z};
        currentStatus_ = status;

        emit positionChanged(position_.x, position_.y, position_.z);
        emit statusChanged(currentStatus_);
        emit logMessage(QStringLiteral("控制器状态：%1，MPos %2")
                            .arg(currentStatus_, positionSummary(position_)));
        return true;
    }

    const QString message = QStringLiteral("返回格式解析失败：%1").arg(line);
    emit logMessage(message);
    emit errorOccurred(message);
    return false;
}

bool SerialMotionController::validatePosition(const MotionPosition &position)
{
    auto fail = [this](const QString &axis, double value, const QString &range) {
        const QString message = QStringLiteral("坐标越界：%1=%2，允许范围 %3")
                                    .arg(axis, mmText(value), range);
        emit logMessage(message);
        emit errorOccurred(message);
        return false;
    };

    if (position.x < 0.0 || position.x > 200.0) {
        return fail(QStringLiteral("X"), position.x, QStringLiteral("0~200"));
    }
    if (position.y < -300.0 || position.y > 0.0) {
        return fail(QStringLiteral("Y"), position.y, QStringLiteral("-300~0"));
    }
    if (position.z < 0.0 || position.z > 10.0) {
        return fail(QStringLiteral("Z"), position.z, QStringLiteral("0~10"));
    }
    return true;
}

QString SerialMotionController::serialErrorMessage(QSerialPort::SerialPortError error) const
{
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        return QStringLiteral("端口不存在：%1").arg(serial_.portName());
    case QSerialPort::PermissionError:
        return QStringLiteral("权限不足或串口被占用：%1").arg(serial_.portName());
    case QSerialPort::OpenError:
        return QStringLiteral("串口打开失败，可能已被占用：%1").arg(serial_.portName());
    case QSerialPort::NotOpenError:
        return QStringLiteral("串口未打开。");
    case QSerialPort::UnsupportedOperationError:
        return QStringLiteral("当前系统不支持该串口操作：%1").arg(serial_.portName());
    case QSerialPort::ResourceError:
        return QStringLiteral("串口资源错误，设备可能已断开：%1").arg(serial_.portName());
    case QSerialPort::WriteError:
        return QStringLiteral("串口写入失败：%1").arg(serial_.errorString());
    case QSerialPort::ReadError:
        return QStringLiteral("串口读取失败：%1").arg(serial_.errorString());
    default:
        return QStringLiteral("%1：%2").arg(serial_.portName(), serial_.errorString());
    }
}

} // namespace NFSScanner::Devices::Motion
