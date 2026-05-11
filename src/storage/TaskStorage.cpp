#include "storage/TaskStorage.h"

#include "app/AppVersion.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringConverter>
#include <QTextStream>

namespace NFSScanner::Storage {

namespace {

QString csvNumber(double value, int precision)
{
    return QString::number(value, 'f', precision);
}

QString compactNumber(double value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text == QStringLiteral("-0")) {
        return QStringLiteral("0");
    }
    return text;
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
        if (ch.isSpace()) {
            result.append(QLatin1Char('_'));
        } else {
            result.append(ch);
        }
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
        setError(QStringLiteral("无法创建扫描根目录：%1").arg(rootPath));
        return false;
    }

    const QString projectName = safeFileNamePart(config.projectName, QStringLiteral("project"));
    const QString testName = safeFileNamePart(config.testName, QStringLiteral("test"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString dirName = QStringLiteral("%1_%2_%3").arg(timestamp, projectName, testName);

    QString candidatePath = rootDir.filePath(dirName);
    int suffix = 1;
    while (QDir(candidatePath).exists()) {
        candidatePath = rootDir.filePath(QStringLiteral("%1_%2").arg(dirName).arg(suffix++));
    }

    taskDir_ = QDir(candidatePath).absolutePath();
    QDir taskDir(taskDir_);
    if (!taskDir.mkpath(QStringLiteral(".")) || !taskDir.mkpath(QStringLiteral("exports"))) {
        setError(QStringLiteral("无法创建任务目录：%1").arg(taskDir_));
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
        setError(QStringLiteral("任务目录尚未创建，无法写入扫描点。"));
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
        setError(QStringLiteral("任务目录尚未创建，无法写入频谱数据。"));
        return false;
    }

    if (result.freqs.isEmpty() || result.values.isEmpty() || result.freqs.size() != result.values.size()) {
        setError(QStringLiteral("频谱数据无效：频率点和数据点数量不一致。"));
        return false;
    }

    if (!traceFrequencyWritten_) {
        if (!writeTraceFrequencyRow(result.freqs)) {
            return false;
        }
    } else if (result.freqs.size() != traceFrequencyCount_) {
        setError(QStringLiteral("频率点数量变化，拒绝写入 traces.csv，避免文件损坏。"));
        return false;
    }

    const QString prefix = QStringLiteral("%1_%2_%3_%4")
                               .arg(compactNumber(result.x),
                                    compactNumber(result.y),
                                    compactNumber(result.z),
                                    result.traceId);

    QString reLine = QStringLiteral("%1_re").arg(prefix);
    QString imLine = QStringLiteral("%1_im").arg(prefix);
    for (double value : result.values) {
        reLine.append(QLatin1Char(','));
        reLine.append(csvNumber(value, 6));
        imLine.append(QStringLiteral(",0"));
    }

    return appendTextLine(tracesPath_, reLine) && appendTextLine(tracesPath_, imLine);
}

bool TaskStorage::finishTask()
{
    if (taskDir_.isEmpty()) {
        return true;
    }

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
        setError(QStringLiteral("写入文件失败：%1，原因：%2").arg(path, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    if (stream.status() != QTextStream::Ok) {
        setError(QStringLiteral("写入文件失败：%1").arg(path));
        return false;
    }
    return true;
}

bool TaskStorage::appendTextLine(const QString &path, const QString &line)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        setError(QStringLiteral("追加文件失败：%1，原因：%2").arg(path, file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << line << '\n';
    if (stream.status() != QTextStream::Ok) {
        setError(QStringLiteral("追加文件失败：%1").arg(path));
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
        setError(QStringLiteral("频率点为空，无法写入 traces.csv。"));
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
