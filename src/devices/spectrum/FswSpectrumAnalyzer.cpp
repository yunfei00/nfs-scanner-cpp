#include "devices/spectrum/FswSpectrumAnalyzer.h"

#include "devices/spectrum/SpectrumParsingUtils.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

namespace {

QString cleanCell(QString text)
{
    text = text.trimmed();
    if (text.size() >= 2
        && ((text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
            || (text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\''))))) {
        text = text.mid(1, text.size() - 2).trimmed();
    }
    return text;
}

QVector<double> zeros(int count)
{
    return QVector<double>(std::max(0, count), 0.0);
}

} // namespace

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
    emit logMessage(QStringLiteral("R&S FSW connected."));
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
        setError(QStringLiteral("R&S FSW is not connected."));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    const QStringList commands{
        QStringLiteral("FREQuency:STARt %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral("FREQuency:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral("BANDwidth:RESolution %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral("BANDwidth:VIDeo %1").arg(config_.vbwHz, 0, 'f', 0),
        QStringLiteral("SWEep:POINts %1").arg(config_.sweepPoints),
        QStringLiteral("INIT:CONT OFF"),
    };

    bool ok = true;
    for (const QString &command : commands) {
        ok = client_.writeCommand(command) && ok;
    }
    if (!ok) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("FSW configure command failed: %1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("FSW configured: %1 Hz ~ %2 Hz, RBW=%3 Hz, points=%4.")
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
        setError(QStringLiteral("R&S FSW is not connected."));
        return {};
    }

    const QString traceMode = normalizeTraceMode(traceMode_);
    client_.writeCommand(QStringLiteral("DISP:TRAC1:MODE WRIT"));
    QThread::msleep(static_cast<unsigned long>(std::max(0.0, clearWriteSettleSeconds_) * 1000.0));
    client_.writeCommand(QStringLiteral("DISP:TRAC1:MODE %1").arg(traceMode == QStringLiteral("WRIT") ? QStringLiteral("MAXH") : traceMode));

    QString traceName = activeTraceName_.trimmed().toUpper();
    if (traceName.isEmpty()) {
        traceName = QStringLiteral("TRACE1");
    }
    traceName.replace(QStringLiteral("TRACE"), QStringLiteral("TRAC"));

    const QString quotedPath = QStringLiteral("\"%1\"").arg(mmemTempTracePath_);
    client_.writeCommand(QStringLiteral("DISP %1 ON").arg(traceName));
    client_.writeCommand(QStringLiteral(":FORM:DEXP:DSEP POIN"));
    client_.writeCommand(QStringLiteral(":FORM:DEXP:FORM CSV"));
    if (!client_.writeCommand(QStringLiteral("MMEM:STOR1:TRAC 1,%1").arg(quotedPath))) {
        setError(QStringLiteral("FSW MMEM store failed: %1").arg(client_.lastError()));
        return {};
    }
    if (!waitOpc(10000, QStringLiteral("FSW MMEM store"))) {
        return {};
    }

    const QString rawCsv = client_.queryLargeText(QStringLiteral("MMEM:DATA? %1").arg(quotedPath), 30000);
    SpectrumTrace trace;
    QString error;
    if (rawCsv.isEmpty() || !parseMmemCsv(rawCsv, &trace, &error)) {
        setError(QStringLiteral("FSW MMEM CSV parse failed: %1").arg(error.isEmpty() ? client_.lastError() : error));
        return {};
    }

    trace.metadata.insert(QStringLiteral("active_trace_name"), activeTraceName_);
    trace.metadata.insert(QStringLiteral("trace_mode"), traceMode);
    lastError_.clear();
    return trace;
}

QString FswSpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("R&S FSW is not connected."));
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

bool FswSpectrumAnalyzer::waitOpc(int timeoutMs, const QString &context)
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

QString FswSpectrumAnalyzer::normalizeTraceMode(const QString &mode) const
{
    QString normalized = mode.trimmed().toUpper();
    normalized.remove(QLatin1Char(' '));
    if (normalized == QStringLiteral("CLEARWRITE") || normalized == QStringLiteral("CLRW") || normalized == QStringLiteral("WRIT")) {
        return QStringLiteral("WRIT");
    }
    if (normalized == QStringLiteral("MAXHOLD") || normalized == QStringLiteral("MAXH")) {
        return QStringLiteral("MAXH");
    }
    if (normalized == QStringLiteral("AVERAGE") || normalized == QStringLiteral("AVER")) {
        return QStringLiteral("AVER");
    }
    if (normalized == QStringLiteral("MINHOLD") || normalized == QStringLiteral("MINH")) {
        return QStringLiteral("MINH");
    }
    return QStringLiteral("MAXH");
}

bool FswSpectrumAnalyzer::parseMmemCsv(const QString &rawText, SpectrumTrace *outTrace, QString *error) const
{
    if (error) {
        error->clear();
    }
    if (!outTrace) {
        if (error) {
            *error = QStringLiteral("Output trace is null.");
        }
        return false;
    }

    QString headerError;
    const QString text = Parsing::stripScpiBlockHeader(rawText, &headerError);
    if (!headerError.isEmpty()) {
        if (error) {
            *error = headerError;
        }
        return false;
    }

    QVector<double> freqs;
    QVector<double> values;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList cells = line.split(QRegularExpression(QStringLiteral("[,;]")));
        if (cells.size() < 2) {
            continue;
        }
        bool freqOk = false;
        bool valueOk = false;
        const double freq = cleanCell(cells.at(0)).toDouble(&freqOk);
        const double value = cleanCell(cells.at(1)).toDouble(&valueOk);
        if (!freqOk || !valueOk || !std::isfinite(freq) || !std::isfinite(value)) {
            continue;
        }
        freqs.push_back(freq);
        values.push_back(value);
    }

    if (freqs.isEmpty() || freqs.size() != values.size()) {
        if (error) {
            *error = QStringLiteral("FSW CSV has no valid frequency/amplitude rows.");
        }
        return false;
    }

    *outTrace = {};
    outTrace->traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    outTrace->freqs = freqs;
    outTrace->values = values;
    outTrace->realValues = values;
    outTrace->imagValues = zeros(values.size());
    outTrace->timestamp = QDateTime::currentDateTime();
    outTrace->source = QStringLiteral("FSW");
    return true;
}

void FswSpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
