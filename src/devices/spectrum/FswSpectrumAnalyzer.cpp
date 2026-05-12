#include "devices/spectrum/FswSpectrumAnalyzer.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

FswSpectrumAnalyzer::FswSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
    , client_(this)
{
    connect(&client_, &ScpiTcpClient::logMessage, this, &ISpectrumAnalyzer::logMessage);
    connect(&client_, &ScpiTcpClient::errorOccurred, this, &ISpectrumAnalyzer::errorOccurred);
}

QString FswSpectrumAnalyzer::name() const
{
    return QStringLiteral("R&S FSW");
}

bool FswSpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    const QString host = options.value(QStringLiteral("host")).toString();
    const quint16 port = static_cast<quint16>(options.value(QStringLiteral("port"), 5025).toInt());
    if (!client_.connectToHost(host, port, 3000)) {
        lastError_ = client_.lastError();
        return false;
    }
    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("R&S FSW 已连接。"));
    return true;
}

void FswSpectrumAnalyzer::disconnectDevice()
{
    client_.disconnectFromHost();
    emit connectedChanged(false);
}

bool FswSpectrumAnalyzer::isConnected() const
{
    return client_.isConnected();
}

bool FswSpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    if (!isConnected()) {
        setError(QStringLiteral("R&S FSW 未连接，无法应用配置。"));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    const QStringList commands{
        QStringLiteral("FREQ:STAR %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral("FREQ:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral("BAND %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral("SWE:POIN %1").arg(config_.sweepPoints),
        QStringLiteral("INIT:CONT OFF"),
    };

    bool ok = true;
    for (const QString &command : commands) {
        ok = client_.writeCommand(command) && ok;
    }
    if (!ok) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("FSW 配置命令部分发送失败：%1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("FSW 配置完成：%1 Hz ~ %2 Hz，RBW=%3 Hz，点数=%4。")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.rbwHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

SpectrumTrace FswSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(pointIndex)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(z)

    if (!isConnected()) {
        setError(QStringLiteral("R&S FSW 未连接，无法执行单次扫描。"));
        return {};
    }

    if (!client_.writeCommand(QStringLiteral("INIT:IMM"))) {
        setError(QStringLiteral("FSW sweep timeout 或启动扫描失败：%1").arg(client_.lastError()));
        return {};
    }
    const QString opc = client_.queryString(QStringLiteral("*OPC?"), 10000);
    if (opc.isEmpty()) {
        setError(QStringLiteral("FSW sweep timeout：%1").arg(client_.lastError()));
        return {};
    }

    const QVector<double> values = parseAsciiNumbers(client_.queryBinaryOrText(QStringLiteral("TRAC:DATA? TRACE1"), 10000));
    if (values.isEmpty()) {
        setError(QStringLiteral("FSW trace query timeout 或返回为空：%1").arg(client_.lastError()));
        return {};
    }
    if (values.size() != std::max(2, config_.sweepPoints)) {
        emit logMessage(QStringLiteral("FSW 返回点数与配置不一致，按实际点数保存：expected=%1 actual=%2")
                            .arg(std::max(2, config_.sweepPoints))
                            .arg(values.size()));
    }

    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.freqs = buildFrequencies(values.size());
    trace.values = values;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = name();
    lastError_.clear();
    return trace;
}

QString FswSpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("R&S FSW 未连接，无法查询 IDN。"));
        return {};
    }
    const QString idn = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (idn.isEmpty()) {
        lastError_ = client_.lastError();
    }
    return idn;
}

QString FswSpectrumAnalyzer::lastError() const
{
    return lastError_;
}

QVector<double> FswSpectrumAnalyzer::parseAsciiNumbers(const QByteArray &payload) const
{
    QString text = QString::fromLatin1(payload).trimmed();
    if (text.startsWith(QLatin1Char('#')) && text.size() > 2) {
        bool ok = false;
        const int lengthDigits = text.mid(1, 1).toInt(&ok);
        if (ok && text.size() > 2 + lengthDigits) {
            text = text.mid(2 + lengthDigits);
        }
    }

    QVector<double> values;
    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts);
    values.reserve(tokens.size());
    for (const QString &token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (ok && std::isfinite(value)) {
            values.push_back(value);
        }
    }
    return values;
}

QVector<double> FswSpectrumAnalyzer::buildFrequencies(int pointCount) const
{
    QVector<double> freqs;
    const int safeCount = std::max(2, pointCount);
    freqs.reserve(safeCount);
    for (int i = 0; i < safeCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(safeCount - 1);
        freqs.push_back(config_.startFreqHz + t * (config_.stopFreqHz - config_.startFreqHz));
    }
    return freqs;
}

void FswSpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
