#include "storage/TaskStorage.h"

#include "app/AppVersion.h"
#include "devices/spectrum/SpectrumParsingUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

namespace NFSScanner::Storage {

namespace {

QString csvNumber(double value, int precision)
{
    return QString::number(value, 'f', precision);
}

QString safeFileNamePart(const QString &text, const QString &fallback)
{
    const QString trimmed = text.trimmed();
    QString result;
    result.reserve(trimmed.size());

    const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (const QChar ch : trimmed) {
        if (illegal.contains(ch)) {
            continue;
        }
        result.append(ch.isSpace() ? QLatin1Char('_') : ch);
    }

    while (result.contains(QStringLiteral("__"))) {
        result.replace(QStringLiteral("__"), QStringLiteral("_"));
    }

    result = result.trimmed();
    while (result.startsWith(QLatin1Char('_'))) {
        result.remove(0, 1);
    }
    while (result.endsWith(QLatin1Char('_'))) {
        result.chop(1);
    }

    return result.isEmpty() ? fallback : result;
}

QString jsonTime(const QDateTime &time)
{
    return time.toString(Qt::ISODateWithMs);
}

QVector<double> zeros(int count)
{
    return QVector<double>(std::max(0, count), 0.0);
}

QString traceLine(const QString &prefix, const QString &suffix, const QVector<double> &values)
{
    QString line = QStringLiteral("%1_%2").arg(prefix, suffix);
    for (double value : values) {
        line.append(QLatin1Char(','));
        line.append(csvNumber(value, 6));
    }
    return line;
}

} // namespace

bool TaskStorage::beginTask(const Core::ScanConfig &config, int pointCount)
{
    lastError_.clear();
    taskDir_.clear();
    pointsPath_.clear();
    tracesPath_.clear();
    traceFrequencyCount_ = 0;
    traceFrequencyWritten_ = false;

    const QString rootPath = config.outputDir.trimmed().isEmpty()
        ? QStringLiteral("data/scans")
        : config.outputDir.trimmed();
    QDir rootDir(rootPath);
    if (!rootDir.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("Cannot create scan root directory: %1").arg(rootPath));
        return false;
    }

    const QString projectName = safeFileNamePart(config.projectName, QStringLiteral("project"));
    const QString testName = safeFileNamePart(config.testName, QStringLiteral("test"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString dirName = QStringLiteral("%1_%2_%3").arg(timestamp, projectName, testName);

    QString candidatePath = rootDir.filePath(dirName);
    int suffix = 1;
    while (QDir(candidatePath).exists()) {
        candidatePath = rootDir.filePath(QStringLiteral("%1_%2").arg(dirName).arg(suffix++));
    }

    taskDir_ = QDir(candidatePath).absolutePath();
    QDir taskDir(taskDir_);
    if (!taskDir.mkpath(QStringLiteral(".")) || !taskDir.mkpath(QStringLiteral("exports"))) {
        setError(QStringLiteral("Cannot create task directory: %1").arg(taskDir_));
        return false;
    }

    pointsPath_ = taskDir.filePath(QStringLiteral("points.csv"));
    tracesPath_ = taskDir.filePath(QStringLiteral("traces.csv"));

    if (!writeTextFile(pointsPath_, QStringLiteral("index,x,y,z,timestamp\n"))) {
        return false;
    }
    if (!writeTextFile(tracesPath_, QString())) {
        return false;
    }
    if (!writeMetaJson(config, pointCount)) {
        return false;
    }
    if (!writeScanConfigJson(config)) {
        return false;
    }

    return true;
}

bool TaskStorage::appendPoint(const Core::ScanPoint &point, const QDateTime &timestamp)
{
    if (taskDir_.isEmpty()) {
        setError(QStringLiteral("Task directory has not been created; cannot write scan point."));
        return false;
    }

    const QString line = QStringLiteral("%1,%2,%3,%4,%5")
                             .arg(point.index)
                             .arg(csvNumber(point.x, 3),
                                  csvNumber(point.y, 3),
                                  csvNumber(point.z, 3),
                                  timestamp.toString(Qt::ISODateWithMs));
    return appendTextLine(pointsPath_, line);
}

bool TaskStorage::appendTrace(const Core::ScanResult &result)
{
    if (taskDir_.isEmpty()) {
        setError(QStringLiteral("Task directory has not been created; cannot write trace data."));
        return false;
    }

    auto trace = result.trace;
    if (trace.freqs.isEmpty() && !result.freqs.isEmpty()) {
        trace.traceId = result.traceId;
        trace.freqs = result.freqs;
        trace.values = result.values;
    }
    if (trace.traceId.trimmed().isEmpty()) {
        trace.traceId = result.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : result.traceId;
    }

    if (trace.freqs.isEmpty()) {
        setError(QStringLiteral("Spectrum trace has no frequency axis."));
        return false;
    }

    if (!traceFrequencyWritten_) {
        if (!writeTraceFrequencyRow(trace.freqs)) {
            return false;
        }
    } else if (trace.freqs.size() != traceFrequencyCount_) {
        setError(QStringLiteral("频率点数不一致：expected=%1 actual=%2，拒绝写入 traces.csv，避免文件损坏。")
                     .arg(traceFrequencyCount_)
                     .arg(trace.freqs.size()));
        return false;
    }

    const QString coordKey = Devices::Spectrum::Parsing::formatCoordKey(result.x, result.y, result.z);
    const int freqCount = trace.freqs.size();
    QStringList lines;

    auto addTraceLines = [&](const QString &traceId,
                             QVector<double> realValues,
                             QVector<double> imagValues,
                             const QVector<double> &displayValues) -> bool {
        const QString safeTraceId = traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : traceId.trimmed();
        if (realValues.isEmpty() && !displayValues.isEmpty()) {
            realValues = displayValues;
        }
        if (imagValues.isEmpty() && !realValues.isEmpty()) {
            imagValues = zeros(realValues.size());
        }
        if (realValues.size() != freqCount || imagValues.size() != freqCount) {
            setError(QStringLiteral("Trace %1 point count mismatch: freqs=%2 re=%3 im=%4")
                         .arg(safeTraceId)
                         .arg(freqCount)
                         .arg(realValues.size())
                         .arg(imagValues.size()));
            return false;
        }

        const QString prefix = QStringLiteral("%1_%2").arg(coordKey, safeTraceId);
        lines << traceLine(prefix, QStringLiteral("re"), realValues);
        lines << traceLine(prefix, QStringLiteral("im"), imagValues);
        return true;
    };

    if (!trace.components.isEmpty()) {
        for (const auto &component : trace.components) {
            if (!addTraceLines(component.traceId,
                               component.realValues,
                               component.imagValues,
                               component.displayValues)) {
                return false;
            }
        }
    } else {
        QVector<double> realValues = trace.realValues;
        QVector<double> imagValues = trace.imagValues;
        if (realValues.isEmpty()) {
            realValues = trace.values;
        }
        if (imagValues.isEmpty() && !realValues.isEmpty()) {
            imagValues = zeros(realValues.size());
        }
        if (!addTraceLines(trace.traceId, realValues, imagValues, trace.values)) {
            return false;
        }
    }

    for (const QString &line : lines) {
        if (!appendTextLine(tracesPath_, line)) {
            return false;
        }
    }
    return true;
}

bool TaskStorage::finishTask()
{
    return true;
}

QString TaskStorage::taskDir() const
{
    return taskDir_;
}

QString TaskStorage::lastError() const
{
    return lastError_;
}

bool TaskStorage::writeTextFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setError(QStringLiteral("Write file failed: %1, reason: %2").arg(path, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    if (stream.status() != QTextStream::Ok) {
        setError(QStringLiteral("Write file failed: %1").arg(path));
        return false;
    }
    return true;
}

bool TaskStorage::appendTextLine(const QString &path, const QString &line)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        setError(QStringLiteral("Append file failed: %1, reason: %2").arg(path, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << line << '\n';
    if (stream.status() != QTextStream::Ok) {
        setError(QStringLiteral("Append file failed: %1").arg(path));
        return false;
    }
    return true;
}

bool TaskStorage::writeMetaJson(const Core::ScanConfig &config, int pointCount)
{
    QJsonObject object;
    object.insert(QStringLiteral("app_name"), QStringLiteral(APP_NAME));
    object.insert(QStringLiteral("app_version"), QStringLiteral(APP_VERSION));
    object.insert(QStringLiteral("data_format_version"), QStringLiteral(DATA_FORMAT_VERSION));
    object.insert(QStringLiteral("created_at"), jsonTime(QDateTime::currentDateTime()));
    object.insert(QStringLiteral("project_name"), config.projectName);
    object.insert(QStringLiteral("test_name"), config.testName);
    object.insert(QStringLiteral("point_count"), pointCount);
    object.insert(QStringLiteral("task_dir"), taskDir_);

    const QString path = QDir(taskDir_).filePath(QStringLiteral("meta.json"));
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return writeTextFile(path, QString::fromUtf8(json));
}

bool TaskStorage::writeScanConfigJson(const Core::ScanConfig &config)
{
    QJsonObject object;
    object.insert(QStringLiteral("startX"), config.startX);
    object.insert(QStringLiteral("startY"), config.startY);
    object.insert(QStringLiteral("startZ"), config.startZ);
    object.insert(QStringLiteral("endX"), config.endX);
    object.insert(QStringLiteral("endY"), config.endY);
    object.insert(QStringLiteral("endZ"), config.endZ);
    object.insert(QStringLiteral("stepX"), config.stepX);
    object.insert(QStringLiteral("stepY"), config.stepY);
    object.insert(QStringLiteral("stepZ"), config.stepZ);
    object.insert(QStringLiteral("feed"), config.feed);
    object.insert(QStringLiteral("dwellMs"), config.dwellMs);
    object.insert(QStringLiteral("snakeMode"), config.snakeMode);
    object.insert(QStringLiteral("projectName"), config.projectName);
    object.insert(QStringLiteral("testName"), config.testName);
    object.insert(QStringLiteral("outputDir"), config.outputDir);

    const QString path = QDir(taskDir_).filePath(QStringLiteral("scan_config.json"));
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return writeTextFile(path, QString::fromUtf8(json));
}

bool TaskStorage::writeTraceFrequencyRow(const QVector<double> &freqs)
{
    if (freqs.isEmpty()) {
        setError(QStringLiteral("Frequency axis is empty; cannot write traces.csv."));
        return false;
    }

    QString line = QStringLiteral("fre");
    for (double freq : freqs) {
        line.append(QLatin1Char(','));
        line.append(csvNumber(freq, 0));
    }

    if (!appendTextLine(tracesPath_, line)) {
        return false;
    }

    traceFrequencyCount_ = freqs.size();
    traceFrequencyWritten_ = true;
    return true;
}

void TaskStorage::setError(const QString &message)
{
    lastError_ = message;
}

} // namespace NFSScanner::Storage
