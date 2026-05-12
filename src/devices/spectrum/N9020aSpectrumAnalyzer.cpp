#include "devices/spectrum/N9020aSpectrumAnalyzer.h"

#include "devices/spectrum/SpectrumParsingUtils.h"

#include <QDateTime>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

namespace {

QVector<double> zeros(int count)
{
    return QVector<double>(std::max(0, count), 0.0);
}

bool idnLooksLikeN9020a(const QString &idn)
{
    const QString upper = idn.toUpper();
    const bool modelOk = upper.contains(QStringLiteral("N9020A"))
        || upper.contains(QStringLiteral("MXA"))
        || upper.contains(QStringLiteral("X-SERIES"));
    const bool vendorOk = upper.contains(QStringLiteral("KEYSIGHT"))
        || upper.contains(QStringLiteral("AGILENT"));
    return modelOk && vendorOk;
}

} // namespace

N9020aSpectrumAnalyzer::N9020aSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
    , client_(this)
{
    connect(&client_, &ScpiTcpClient::logMessage, this, &ISpectrumAnalyzer::logMessage);
    connect(&client_, &ScpiTcpClient::errorOccurred, this, &ISpectrumAnalyzer::errorOccurred);
}

QString N9020aSpectrumAnalyzer::name() const
{
    return QStringLiteral("Keysight N9020A");
}

bool N9020aSpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    const QString host = options.value(QStringLiteral("host")).toString();
    const quint16 port = static_cast<quint16>(options.value(QStringLiteral("port"), 5025).toInt());
    if (!client_.connectToHost(host, port, 3000)) {
        lastError_ = client_.lastError();
        return false;
    }

    cachedIdnText_ = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (cachedIdnText_.isEmpty()) {
        lastError_ = QStringLiteral("N9020A IDN query failed: %1").arg(client_.lastError());
        client_.disconnectFromHost();
        return false;
    }
    if (!idnLooksLikeN9020a(cachedIdnText_)) {
        lastError_ = QStringLiteral("IDN does not look like N9020A/MXA/X-Series: %1").arg(cachedIdnText_);
        client_.disconnectFromHost();
        return false;
    }

    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("Keysight N9020A connected: %1").arg(cachedIdnText_));
    return true;
}

void N9020aSpectrumAnalyzer::disconnectDevice()
{
    client_.disconnectFromHost();
    emit connectedChanged(false);
}

bool N9020aSpectrumAnalyzer::isConnected() const
{
    return client_.isConnected();
}

bool N9020aSpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A is not connected."));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    client_.writeCommand(QStringLiteral("*CLS"));
    client_.writeCommand(QStringLiteral("FORM ASC"));

    const QStringList commands{
        QStringLiteral("FREQ:STAR %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral("FREQ:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral("BAND:RES %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral("BAND:VID %1").arg(config_.vbwHz, 0, 'f', 0),
        QStringLiteral("SWE:POIN %1").arg(config_.sweepPoints),
        QStringLiteral("TRACe1:TYPE %1").arg(normalizeTraceMode(traceMode_)),
        QStringLiteral("INIT:CONT OFF"),
    };

    bool ok = true;
    for (const QString &command : commands) {
        ok = client_.writeCommand(command) && ok;
    }
    if (!ok) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("N9020A configure command failed: %1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("N9020A configured: %1 Hz ~ %2 Hz, RBW=%3 Hz, points=%4.")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.rbwHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

SpectrumTrace N9020aSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(pointIndex)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(z)

    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A is not connected."));
        return {};
    }

    client_.writeCommand(QStringLiteral("FORM ASC"));
    client_.writeCommand(QStringLiteral("ABOR"));
    if (!client_.writeCommand(QStringLiteral("INIT:IMM"))) {
        setError(QStringLiteral("N9020A sweep start failed: %1").arg(client_.lastError()));
        return {};
    }
    if (!waitOpc(10000, QStringLiteral("N9020A sweep"))) {
        return {};
    }

    QString parseError;
    const QVector<double> numbers = Parsing::parseAsciiFloatValues(client_.queryLargeText(QStringLiteral("TRAC:DATA? TRACE1"), 10000), &parseError);
    if (numbers.isEmpty()) {
        setError(QStringLiteral("N9020A TRACE query failed: %1").arg(parseError.isEmpty() ? client_.lastError() : parseError));
        return {};
    }

    SpectrumTrace trace = traceFromNumbers(numbers);
    if (trace.values.isEmpty()) {
        return {};
    }
    lastError_.clear();
    return trace;
}

QString N9020aSpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A is not connected."));
        return {};
    }
    if (!cachedIdnText_.isEmpty()) {
        return cachedIdnText_;
    }
    const QString idn = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (idn.isEmpty()) {
        lastError_ = client_.lastError();
    }
    return idn;
}

QString N9020aSpectrumAnalyzer::lastError() const
{
    return lastError_;
}

bool N9020aSpectrumAnalyzer::waitOpc(int timeoutMs, const QString &context)
{
    const QString response = client_.queryString(QStringLiteral("*OPC?"), timeoutMs).trimmed();
    bool ok = false;
    const double value = response.toDouble(&ok);
    if (!ok || value != 1.0) {
        setError(QStringLiteral("%1 OPC timeout or unexpected response: %2").arg(context, response.isEmpty() ? client_.lastError() : response));
        return false;
    }
    return true;
}

double N9020aSpectrumAnalyzer::queryDoubleOrFallback(const QString &command, double fallback) const
{
    auto *self = const_cast<N9020aSpectrumAnalyzer *>(this);
    bool ok = false;
    const double value = self->client_.queryString(command, 2500).toDouble(&ok);
    return ok && std::isfinite(value) ? value : fallback;
}

int N9020aSpectrumAnalyzer::queryIntOrFallback(const QString &command, int fallback) const
{
    auto *self = const_cast<N9020aSpectrumAnalyzer *>(this);
    bool ok = false;
    const int value = self->client_.queryString(command, 2500).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

QString N9020aSpectrumAnalyzer::normalizeTraceMode(const QString &mode) const
{
    QString normalized = mode.trimmed().toUpper();
    normalized.remove(QLatin1Char(' '));
    if (normalized == QStringLiteral("WRITE") || normalized == QStringLiteral("CLEARWRITE") || normalized == QStringLiteral("WRIT")) {
        return QStringLiteral("WRIT");
    }
    if (normalized == QStringLiteral("AVERAGE") || normalized == QStringLiteral("AVER")) {
        return QStringLiteral("AVER");
    }
    if (normalized == QStringLiteral("MAXHOLD") || normalized == QStringLiteral("MAXH")) {
        return QStringLiteral("MAXH");
    }
    if (normalized == QStringLiteral("MINHOLD") || normalized == QStringLiteral("MINH")) {
        return QStringLiteral("MINH");
    }
    return QStringLiteral("WRIT");
}

SpectrumTrace N9020aSpectrumAnalyzer::traceFromNumbers(const QVector<double> &numbers) const
{
    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = QStringLiteral("N9020A");

    const int configuredPoints = std::max(2, config_.sweepPoints);
    if (numbers.size() == configuredPoints * 2) {
        trace.realValues.reserve(configuredPoints);
        trace.imagValues.reserve(configuredPoints);
        trace.values.reserve(configuredPoints);
        for (int i = 0; i + 1 < numbers.size(); i += 2) {
            const double re = numbers.at(i);
            const double im = numbers.at(i + 1);
            trace.realValues.push_back(re);
            trace.imagValues.push_back(im);
            trace.values.push_back(std::hypot(re, im));
        }
    } else {
        if (numbers.size() != configuredPoints) {
            emit const_cast<N9020aSpectrumAnalyzer *>(this)->logMessage(
                QStringLiteral("N9020A point count differs from config; using actual count. expected=%1 actual=%2")
                    .arg(configuredPoints)
                    .arg(numbers.size()));
        }
        trace.values = numbers;
        trace.realValues = numbers;
        trace.imagValues = zeros(numbers.size());
    }

    const double startHz = queryDoubleOrFallback(QStringLiteral("FREQ:STAR?"), config_.startFreqHz);
    const double stopHz = queryDoubleOrFallback(QStringLiteral("FREQ:STOP?"), config_.stopFreqHz);
    const int points = queryIntOrFallback(QStringLiteral("SWE:POIN?"), trace.values.size());
    trace.freqs = Parsing::buildFrequencyAxis(startHz, stopHz, points == trace.values.size() ? points : trace.values.size());
    trace.metadata.insert(QStringLiteral("idn"), cachedIdnText_);
    const_cast<N9020aSpectrumAnalyzer *>(this)->lastError_.clear();
    return trace;
}

void N9020aSpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
