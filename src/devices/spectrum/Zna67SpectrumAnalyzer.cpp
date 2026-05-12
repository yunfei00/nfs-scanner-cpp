#include "devices/spectrum/Zna67SpectrumAnalyzer.h"

#include "devices/spectrum/SpectrumParsingUtils.h"

#include <QDateTime>
#include <QMap>
#include <QRegularExpression>
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

QStringList splitCsvLine(const QString &line)
{
    return line.split(QLatin1Char(';'));
}

QVector<double> zeros(int count)
{
    return QVector<double>(std::max(0, count), 0.0);
}

} // namespace

Zna67SpectrumAnalyzer::Zna67SpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
    , client_(this)
{
    connect(&client_, &ScpiTcpClient::logMessage, this, &ISpectrumAnalyzer::logMessage);
    connect(&client_, &ScpiTcpClient::errorOccurred, this, &ISpectrumAnalyzer::errorOccurred);
}

QString Zna67SpectrumAnalyzer::name() const
{
    return QStringLiteral("R&S ZNA67");
}

bool Zna67SpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    const QString host = options.value(QStringLiteral("host")).toString();
    const quint16 port = static_cast<quint16>(options.value(QStringLiteral("port"), 5025).toInt());
    if (!client_.connectToHost(host, port, 3000)) {
        lastError_ = client_.lastError();
        return false;
    }

    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("R&S ZNA67 connected."));
    return true;
}

void Zna67SpectrumAnalyzer::disconnectDevice()
{
    client_.disconnectFromHost();
    emit connectedChanged(false);
}

bool Zna67SpectrumAnalyzer::isConnected() const
{
    return client_.isConnected();
}

bool Zna67SpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    if (!isConnected()) {
        setError(QStringLiteral("R&S ZNA67 is not connected."));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    const QStringList commands{
        QStringLiteral("SENS1:FREQ:STAR %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral("SENS1:FREQ:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral("SENS1:SWE:POIN %1").arg(config_.sweepPoints),
        QStringLiteral("SENS1:BAND %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral("INIT1:CONT OFF"),
    };

    bool ok = true;
    for (const QString &command : commands) {
        ok = client_.writeCommand(command) && ok;
    }

    if (!ok) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("ZNA67 configure command failed: %1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("ZNA67 configured: %1 Hz ~ %2 Hz, RBW=%3 Hz, points=%4.")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.rbwHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

SpectrumTrace Zna67SpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(pointIndex)

    if (!isConnected()) {
        setError(QStringLiteral("R&S ZNA67 is not connected."));
        return {};
    }

    const QString quotedPath = QStringLiteral("\"%1\"").arg(mmemTempTracePath_);
    emit logMessage(QStringLiteral("ZNA67 MMEM store trace CSV: %1").arg(mmemTempTracePath_));

    SpectrumTrace trace;
    QString parseError;
    bool cleanupNeeded = false;

    if (client_.writeCommand(QStringLiteral("MMEM:STOR:TRAC:CHAN 1, %1").arg(quotedPath))) {
        cleanupNeeded = true;
        if (waitOpc(6000, QStringLiteral("ZNA67 MMEM store"))) {
            emit logMessage(QStringLiteral("ZNA67 MMEM read trace CSV."));
            const QString rawCsv = client_.queryLargeText(QStringLiteral("MMEM:DATA? %1").arg(quotedPath), 30000);
            if (!rawCsv.isEmpty() && parseZnaMmemCsv(rawCsv, x, y, z, &trace, &parseError)) {
                client_.writeCommand(QStringLiteral("MMEM:DEL %1").arg(quotedPath));
                waitOpc(3000, QStringLiteral("ZNA67 MMEM delete"));
                lastError_.clear();
                return trace;
            }
            if (parseError.isEmpty()) {
                parseError = client_.lastError();
            }
        }
    } else {
        parseError = client_.lastError();
    }

    if (cleanupNeeded) {
        emit logMessage(QStringLiteral("ZNA67 cleanup temp CSV."));
        client_.writeCommand(QStringLiteral("MMEM:DEL %1").arg(quotedPath));
        waitOpc(3000, QStringLiteral("ZNA67 MMEM delete"));
    }

    emit logMessage(QStringLiteral("ZNA67 MMEM CSV failed, fallback to CALC1:DATA? SDATA: %1").arg(parseError));
    return fallbackSDataSweep();
}

QString Zna67SpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("R&S ZNA67 is not connected."));
        return {};
    }

    const QString idn = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (idn.isEmpty()) {
        lastError_ = client_.lastError();
    }
    return idn;
}

QString Zna67SpectrumAnalyzer::lastError() const
{
    return lastError_;
}

bool Zna67SpectrumAnalyzer::waitOpc(int timeoutMs, const QString &context)
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

bool Zna67SpectrumAnalyzer::parseZnaMmemCsv(const QString &rawText,
                                            double x,
                                            double y,
                                            double z,
                                            SpectrumTrace *outTrace,
                                            QString *error) const
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

    const QStringList rawLines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    QStringList lines;
    for (QString line : rawLines) {
        line = line.trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
            lines << line;
        }
    }
    if (lines.isEmpty()) {
        if (error) {
            *error = QStringLiteral("ZNA MMEM CSV is empty.");
        }
        return false;
    }

    const QStringList header = splitCsvLine(lines.first());
    if (header.isEmpty() || !cleanCell(header.first()).toLower().startsWith(QStringLiteral("freq"))) {
        if (error) {
            *error = QStringLiteral("ZNA MMEM CSV header must start with freq.");
        }
        return false;
    }

    struct ColumnInfo {
        int cellIndex = -1;
        int componentIndex = -1;
        bool isReal = true;
    };

    QVector<SpectrumTrace::TraceComponent> components;
    QMap<QString, int> traceIndexByName;
    QVector<ColumnInfo> columns;
    columns.reserve(std::max<qsizetype>(0, header.size() - 1));

    for (int col = 1; col < header.size(); ++col) {
        const QString cell = cleanCell(header.at(col));
        const int sep = cell.indexOf(QLatin1Char(':'));
        if (sep <= 0 || sep >= cell.size() - 1) {
            continue;
        }
        const QString role = cell.left(sep).trimmed().toLower();
        const QString traceId = cell.mid(sep + 1).trimmed();
        if (traceId.isEmpty() || (role != QStringLiteral("re") && role != QStringLiteral("im"))) {
            continue;
        }

        if (!traceIndexByName.contains(traceId)) {
            SpectrumTrace::TraceComponent component;
            component.traceId = traceId;
            traceIndexByName.insert(traceId, components.size());
            components.push_back(component);
        }
        columns.push_back({col, traceIndexByName.value(traceId), role == QStringLiteral("re")});
    }

    if (components.isEmpty()) {
        if (error) {
            *error = QStringLiteral("ZNA MMEM CSV has no re:/im: trace columns.");
        }
        return false;
    }

    QVector<double> freqs;
    for (int row = 1; row < lines.size(); ++row) {
        const QStringList cells = splitCsvLine(lines.at(row));
        if (cells.isEmpty()) {
            continue;
        }

        bool freqOk = false;
        const double freq = cleanCell(cells.first()).toDouble(&freqOk);
        if (!freqOk || !std::isfinite(freq)) {
            continue;
        }
        freqs.push_back(freq);

        for (int i = 0; i < columns.size(); ++i) {
            const int cellIndex = columns.at(i).cellIndex;
            double value = 0.0;
            if (cellIndex >= 0 && cellIndex < cells.size()) {
                bool valueOk = false;
                value = cleanCell(cells.at(cellIndex)).toDouble(&valueOk);
                if (!valueOk || !std::isfinite(value)) {
                    value = 0.0;
                }
            }

            auto &component = components[columns.at(i).componentIndex];
            if (columns.at(i).isReal) {
                component.realValues.push_back(value);
            } else {
                component.imagValues.push_back(value);
            }
        }
    }

    if (freqs.isEmpty()) {
        if (error) {
            *error = QStringLiteral("ZNA MMEM CSV has no valid data rows.");
        }
        return false;
    }

    for (auto &component : components) {
        if (component.realValues.isEmpty()) {
            component.realValues = zeros(freqs.size());
        }
        if (component.imagValues.isEmpty()) {
            component.imagValues = zeros(freqs.size());
        }
        if (component.realValues.size() != freqs.size() || component.imagValues.size() != freqs.size()) {
            if (error) {
                *error = QStringLiteral("ZNA trace %1 length mismatch. freqs=%2 re=%3 im=%4")
                             .arg(component.traceId)
                             .arg(freqs.size())
                             .arg(component.realValues.size())
                             .arg(component.imagValues.size());
            }
            return false;
        }

        component.displayValues.reserve(freqs.size());
        for (int i = 0; i < freqs.size(); ++i) {
            component.displayValues.push_back(Parsing::magnitudeToDb(component.realValues.at(i), component.imagValues.at(i)));
        }
    }

    *outTrace = {};
    outTrace->traceId = components.first().traceId;
    outTrace->freqs = freqs;
    outTrace->realValues = components.first().realValues;
    outTrace->imagValues = components.first().imagValues;
    outTrace->values = components.first().displayValues;
    outTrace->components = components;
    outTrace->timestamp = QDateTime::currentDateTime();
    outTrace->source = QStringLiteral("ZNA67");

    QStringList traceNames;
    for (const auto &component : components) {
        traceNames << component.traceId;
    }
    outTrace->metadata.insert(QStringLiteral("primary_trace_name"), outTrace->traceId);
    outTrace->metadata.insert(QStringLiteral("trace_names"), traceNames);
    outTrace->metadata.insert(QStringLiteral("coord_key"), Parsing::formatCoordKey(x, y, z));
    outTrace->metadata.insert(QStringLiteral("mmem_csv_preview"), text.left(512));
    return true;
}

SpectrumTrace Zna67SpectrumAnalyzer::fallbackSDataSweep()
{
    if (!client_.writeCommand(QStringLiteral("INIT1:IMM"))) {
        setError(QStringLiteral("ZNA67 sweep start failed: %1").arg(client_.lastError()));
        return {};
    }
    if (!waitOpc(10000, QStringLiteral("ZNA67 SDATA sweep"))) {
        return {};
    }

    QString parseError;
    const QVector<double> numbers = Parsing::parseAsciiFloatValues(client_.queryLargeText(QStringLiteral("CALC1:DATA? SDATA"), 10000), &parseError);
    if (numbers.isEmpty()) {
        setError(QStringLiteral("ZNA67 SDATA query failed: %1").arg(parseError.isEmpty() ? client_.lastError() : parseError));
        return {};
    }

    const int expected = std::max(2, config_.sweepPoints);
    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = QStringLiteral("ZNA67");

    if (numbers.size() == expected * 2) {
        trace.realValues.reserve(expected);
        trace.imagValues.reserve(expected);
        trace.values.reserve(expected);
        for (int i = 0; i + 1 < numbers.size(); i += 2) {
            const double re = numbers.at(i);
            const double im = numbers.at(i + 1);
            trace.realValues.push_back(re);
            trace.imagValues.push_back(im);
            trace.values.push_back(Parsing::magnitudeToDb(re, im));
        }
    } else if (numbers.size() == expected) {
        trace.values = numbers;
        trace.realValues = numbers;
        trace.imagValues = zeros(numbers.size());
    } else {
        setError(QStringLiteral("ZNA67 SDATA point count mismatch: actual=%1 expected=%2 or %3")
                     .arg(numbers.size())
                     .arg(expected)
                     .arg(expected * 2));
        return {};
    }

    trace.freqs = Parsing::buildFrequencyAxis(config_.startFreqHz, config_.stopFreqHz, trace.values.size());
    lastError_.clear();
    return trace;
}

void Zna67SpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
