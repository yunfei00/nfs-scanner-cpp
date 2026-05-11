#include "analysis/FrequencyCsvParser.h"

#include "analysis/FrequencyData.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>
#include <complex>

namespace NFSScanner::Analysis {

namespace {

struct ParsedKey
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    QString traceId;
    bool isReal = true;
};

struct TraceParts
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    QString traceId;
    QVector<double> real;
    QVector<double> imag;
    bool hasReal = false;
    bool hasImag = false;
};

QString dataKey(double x, double y, double z, const QString &traceId)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(QString::number(x, 'g', 17),
             QString::number(y, 'g', 17),
             QString::number(z, 'g', 17),
             traceId);
}

QStringList splitCsvLine(const QString &line)
{
    QStringList parts = line.split(QLatin1Char(','));
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

bool parseKey(const QString &text, ParsedKey *outKey)
{
    if (!outKey) {
        return false;
    }

    const QStringList parts = text.split(QLatin1Char('_'));
    if (parts.size() < 5) {
        return false;
    }

    const QString suffix = parts.last().trimmed().toLower();
    if (suffix != QStringLiteral("re") && suffix != QStringLiteral("im")) {
        return false;
    }

    bool xOk = false;
    bool yOk = false;
    bool zOk = false;
    const double x = parts.at(0).toDouble(&xOk);
    const double y = parts.at(1).toDouble(&yOk);
    const double z = parts.at(2).toDouble(&zOk);
    if (!xOk || !yOk || !zOk) {
        return false;
    }

    const QString traceId = parts.mid(3, parts.size() - 4).join(QLatin1Char('_')).trimmed();
    if (traceId.isEmpty()) {
        return false;
    }

    outKey->x = x;
    outKey->y = y;
    outKey->z = z;
    outKey->traceId = traceId;
    outKey->isReal = suffix == QStringLiteral("re");
    return true;
}

} // namespace

bool FrequencyCsvParser::loadFile(const QString &filePath, FrequencyData *outData)
{
    lastError_.clear();
    if (!outData) {
        lastError_ = QStringLiteral("输出数据对象为空。");
        return false;
    }

    outData->clear();

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        lastError_ = QStringLiteral("traces.csv 文件不存在：%1").arg(filePath);
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lastError_ = QStringLiteral("无法打开 traces.csv：%1，原因：%2")
                         .arg(fileInfo.absoluteFilePath(), file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    if (stream.atEnd()) {
        lastError_ = QStringLiteral("traces.csv 为空：%1").arg(fileInfo.absoluteFilePath());
        return false;
    }

    const QString firstLine = stream.readLine().trimmed();
    const QStringList frequencyTokens = splitCsvLine(firstLine);
    if (frequencyTokens.isEmpty() || frequencyTokens.first().toLower() != QStringLiteral("fre")) {
        lastError_ = QStringLiteral("traces.csv 第一行必须以 fre 开头。");
        return false;
    }

    QVector<double> freqs;
    freqs.reserve(std::max<qsizetype>(0, frequencyTokens.size() - 1));
    for (int i = 1; i < frequencyTokens.size(); ++i) {
        bool ok = false;
        const double freq = frequencyTokens.at(i).toDouble(&ok);
        if (!ok) {
            lastError_ = QStringLiteral("频率值无法解析：%1").arg(frequencyTokens.at(i));
            return false;
        }
        freqs.push_back(freq);
    }

    if (freqs.isEmpty()) {
        lastError_ = QStringLiteral("traces.csv 未包含频率点。");
        return false;
    }

    QMap<QString, TraceParts> traces;
    int skippedLines = 0;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList tokens = splitCsvLine(line);
        if (tokens.size() != freqs.size() + 1) {
            ++skippedLines;
            continue;
        }

        ParsedKey key;
        if (!parseKey(tokens.first(), &key)) {
            ++skippedLines;
            continue;
        }

        QVector<double> values;
        values.reserve(freqs.size());
        bool valuesOk = true;
        for (int i = 1; i < tokens.size(); ++i) {
            bool ok = false;
            const double value = tokens.at(i).toDouble(&ok);
            if (!ok) {
                valuesOk = false;
                break;
            }
            values.push_back(value);
        }

        if (!valuesOk) {
            ++skippedLines;
            continue;
        }

        const QString mapKey = dataKey(key.x, key.y, key.z, key.traceId);
        TraceParts &parts = traces[mapKey];
        parts.x = key.x;
        parts.y = key.y;
        parts.z = key.z;
        parts.traceId = key.traceId;
        if (key.isReal) {
            parts.real = values;
            parts.hasReal = true;
        } else {
            parts.imag = values;
            parts.hasImag = true;
        }
    }

    outData->setFrequencies(freqs);
    for (const TraceParts &parts : traces) {
        QVector<std::complex<double>> complexValues;
        complexValues.reserve(freqs.size());
        for (int i = 0; i < freqs.size(); ++i) {
            const double re = parts.hasReal && i < parts.real.size() ? parts.real.at(i) : 0.0;
            const double im = parts.hasImag && i < parts.imag.size() ? parts.imag.at(i) : 0.0;
            complexValues.push_back({re, im});
        }
        outData->addTraceValues(parts.x, parts.y, parts.z, parts.traceId, complexValues);
    }
    outData->finalize();

    if (!outData->isValid()) {
        lastError_ = outData->lastError().isEmpty()
            ? QStringLiteral("未解析到有效频谱数据。")
            : outData->lastError();
        return false;
    }

    if (skippedLines > 0) {
        lastError_ = QStringLiteral("解析完成，但跳过了 %1 行格式不匹配的数据。").arg(skippedLines);
        outData->setLastError(lastError_);
    }

    return true;
}

QString FrequencyCsvParser::lastError() const
{
    return lastError_;
}

} // namespace NFSScanner::Analysis
