#include "devices/spectrum/SpectrumParsingUtils.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum::Parsing {

namespace {

QString trimOuterQuotes(QString text)
{
    text = text.trimmed();
    while (text.size() >= 2
           && ((text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
               || (text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\''))))) {
        text = text.mid(1, text.size() - 2).trimmed();
    }
    return text;
}

} // namespace

QString stripScpiBlockHeader(const QString &rawText, QString *error)
{
    if (error) {
        error->clear();
    }

    QString text = trimOuterQuotes(rawText);
    if (!text.startsWith(QLatin1Char('#'))) {
        return text;
    }

    if (text.size() < 2) {
        if (error) {
            *error = QStringLiteral("Invalid SCPI block header.");
        }
        return {};
    }

    bool ok = false;
    const int headerDigits = text.mid(1, 1).toInt(&ok);
    if (!ok || headerDigits < 0 || headerDigits > 9) {
        if (error) {
            *error = QStringLiteral("Invalid SCPI block header digit count.");
        }
        return {};
    }

    const int lengthStart = 2;
    const int payloadStart = lengthStart + headerDigits;
    if (text.size() < payloadStart) {
        if (error) {
            *error = QStringLiteral("Incomplete SCPI block header.");
        }
        return {};
    }

    bool lengthOk = false;
    const int payloadLength = headerDigits == 0 ? text.size() - payloadStart : text.mid(lengthStart, headerDigits).toInt(&lengthOk);
    if (headerDigits != 0 && (!lengthOk || payloadLength < 0)) {
        if (error) {
            *error = QStringLiteral("Invalid SCPI block payload length.");
        }
        return {};
    }

    const QString payload = text.mid(payloadStart, payloadLength);
    const QString tail = text.mid(payloadStart + payloadLength).trimmed();
    return tail.isEmpty() ? payload : payload + tail;
}

QVector<double> parseAsciiFloatValues(const QString &rawText, QString *error)
{
    if (error) {
        error->clear();
    }

    QString headerError;
    const QString payload = stripScpiBlockHeader(rawText, &headerError).trimmed();
    if (!headerError.isEmpty()) {
        if (error) {
            *error = headerError;
        }
        return {};
    }

    if (payload.isEmpty()) {
        if (error) {
            *error = QStringLiteral("ASCII payload is empty.");
        }
        return {};
    }

    const QStringList tokens = payload.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        if (error) {
            *error = QStringLiteral("ASCII payload has no numeric tokens.");
        }
        return {};
    }

    QVector<double> values;
    values.reserve(tokens.size());
    for (QString token : tokens) {
        token = trimOuterQuotes(token);
        bool ok = false;
        const double value = token.toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            if (error) {
                *error = QStringLiteral("Invalid numeric token: %1").arg(token);
            }
            return {};
        }
        values.push_back(value);
    }

    return values;
}

QVector<double> buildFrequencyAxis(double startHz, double stopHz, int pointCount)
{
    QVector<double> freqs;
    if (pointCount <= 0) {
        return freqs;
    }

    freqs.reserve(pointCount);
    if (pointCount == 1) {
        freqs.push_back(startHz);
        return freqs;
    }

    for (int i = 0; i < pointCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(pointCount - 1);
        freqs.push_back(startHz + t * (stopHz - startHz));
    }
    return freqs;
}

double magnitudeToDb(double re, double im)
{
    const double mag = std::max(std::hypot(re, im), 1e-12);
    return 20.0 * std::log10(mag);
}

QString formatCoordKey(double x, double y, double z)
{
    auto one = [](double value) {
        QString text = QString::number(value, 'g', 12);
        if (text == QStringLiteral("-0")) {
            return QStringLiteral("0");
        }
        return text;
    };
    return QStringLiteral("%1_%2_%3").arg(one(x), one(y), one(z));
}

double parseFrequencyValue(const QString &text, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QString valueText = text.trimmed();
    const QString upper = valueText.toUpper();
    double factor = 1.0;
    if (upper.endsWith(QStringLiteral("GHZ"))) {
        factor = 1e9;
        valueText.chop(3);
    } else if (upper.endsWith(QStringLiteral("MHZ"))) {
        factor = 1e6;
        valueText.chop(3);
    } else if (upper.endsWith(QStringLiteral("KHZ"))) {
        factor = 1e3;
        valueText.chop(3);
    } else if (upper.endsWith(QStringLiteral("HZ"))) {
        valueText.chop(2);
    }

    bool localOk = false;
    const double value = valueText.trimmed().toDouble(&localOk);
    if (ok) {
        *ok = localOk;
    }
    return localOk ? value * factor : 0.0;
}

void normalizeFrequencyWindow(double *startHz, double *stopHz, double *centerHz, double *spanHz)
{
    if (!startHz || !stopHz || !centerHz || !spanHz) {
        return;
    }

    if (*stopHz > *startHz) {
        *centerHz = (*startHz + *stopHz) * 0.5;
        *spanHz = *stopHz - *startHz;
        return;
    }

    if (*spanHz <= 0.0) {
        *spanHz = 1.0;
    }
    *startHz = *centerHz - *spanHz * 0.5;
    *stopHz = *centerHz + *spanHz * 0.5;
}

} // namespace NFSScanner::Devices::Spectrum::Parsing
