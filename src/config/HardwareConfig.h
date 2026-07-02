#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace NFSScanner::Config {

struct MotionLimits
{
    double xMin = 0.0;
    double xMax = 200.0;
    double yMin = -300.0;
    double yMax = 0.0;
    double zMin = 0.0;
    double zMax = 10.0;
};

struct MotionHardwareConfig
{
    bool enabled = false;
    QString type = QStringLiteral("grbl");
    QString port = QStringLiteral("COM3");
    int baudrate = 115200;
    int timeoutMs = 3000;
    double feedDefault = 1000.0;
    MotionLimits limits;
    bool homeOnConnect = false;
    int pollIntervalMs = 500;
};

struct SpectrumHardwareConfig
{
    bool enabled = false;
    QString type = QStringLiteral("mock");
    QString address = QStringLiteral("192.168.0.10");
    int port = 5025;
    int timeoutMs = 5000;
    int retryCount = 2;
    double startFreqHz = 1'000'000'000.0;
    double stopFreqHz = 3'000'000'000.0;
    int points = 201;
    double rbwHz = 1000.0;
    double vbwHz = 1000.0;
    double sweepTimeS = 0.1;
    QString trace = QStringLiteral("Trc1_S21");
};

struct CameraHardwareConfig
{
    bool enabled = false;
    QString type = QStringLiteral("mock");
    int deviceIndex = 0;
    int width = 640;
    int height = 480;
    double exposureMs = 0.0;
    double gain = 0.0;
    QString saveFormat = QStringLiteral("png");
    QString saveDir = QStringLiteral("images");
    int timeoutMs = 3000;
    bool flipHorizontal = false;
    bool flipVertical = false;
    int rotationDeg = 0;
};

struct ProbeHardwareConfig
{
    bool enabled = false;
    QString type = QStringLiteral("mock");
    QString orientation = QStringLiteral("Hx");
    int switchDelayMs = 500;
    QString port = QStringLiteral("COM4");
    int baudrate = 115200;
    QString hxCommand = QStringLiteral("HX");
    QString hyCommand = QStringLiteral("HY");
    QString queryCommand = QStringLiteral("?");
    bool verifyAfterSwitch = false;
};

struct HardwareConfig
{
    MotionHardwareConfig motion;
    SpectrumHardwareConfig spectrum;
    CameraHardwareConfig camera;
    ProbeHardwareConfig probe;

    static HardwareConfig defaults();
    bool validate(QStringList *errors = nullptr) const;
};

QString defaultHardwareConfigPath();
QString defaultProfilesDirectory();
QString probeOrientationToString(const QString &orientation);
bool isValidProbeOrientation(const QString &orientation);

QJsonObject hardwareConfigToJson(const HardwareConfig &config);
bool hardwareConfigFromJson(const QJsonObject &root, HardwareConfig *config, QStringList *errors = nullptr);

} // namespace NFSScanner::Config
