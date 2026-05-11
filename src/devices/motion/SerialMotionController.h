#pragma once

#include "devices/motion/IMotionController.h"

#include <QByteArray>
#include <QSerialPort>
#include <QString>

#include <optional>

namespace NFSScanner::Devices::Motion {

struct MotionPosition
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

class SerialMotionController final : public IMotionController
{
    Q_OBJECT

public:
    explicit SerialMotionController(QObject *parent = nullptr);
    ~SerialMotionController() override;

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool moveTo(double xMm, double yMm, double zMm) override;

    bool openPort(const QString &portName, int baudRate);
    void closePort();
    bool isOpen() const;

    bool home();
    bool queryPosition();
    bool readVersion();
    bool readHelp();

    bool moveAbs(std::optional<double> x,
                 std::optional<double> y,
                 std::optional<double> z,
                 double feed);
    bool jogAxis(const QString &axis, double delta, double feed);

    MotionPosition currentPosition() const;
    QString currentStatus() const;

signals:
    void connectedChanged(bool connected);
    void statusChanged(const QString &status);
    void rawLineReceived(const QString &line);
    void logMessage(const QString &message);

private:
    bool sendCommand(const QString &command);
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
    void processLine(const QString &line);
    bool parseStatusLine(const QString &line);
    bool validatePosition(const MotionPosition &position);
    QString serialErrorMessage(QSerialPort::SerialPortError error) const;

    QSerialPort serial_;
    QByteArray readBuffer_;
    MotionPosition position_;
    QString currentStatus_ = QStringLiteral("未连接");
    QString lastPortName_;
    int lastBaudRate_ = 115200;
};

} // namespace NFSScanner::Devices::Motion
