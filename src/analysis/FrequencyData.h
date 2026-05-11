#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <complex>

namespace NFSScanner::Analysis {

class FrequencyData
{
public:
    void clear();

    bool isValid() const;
    QString lastError() const;

    QVector<double> freqs() const;
    QStringList traceIds() const;
    QVector<double> xs() const;
    QVector<double> ys() const;
    QVector<double> zs() const;

    int frequencyCount() const;
    int pointCount() const;

    bool hasValue(double x, double y, double z, const QString &traceId) const;
    std::complex<double> complexValue(double x,
                                      double y,
                                      double z,
                                      const QString &traceId,
                                      int freqIndex) const;
    double scalarValue(double x,
                       double y,
                       double z,
                       const QString &traceId,
                       int freqIndex,
                       const QString &mode) const;

    void setLastError(const QString &error);

    void setFrequencies(const QVector<double> &freqs);
    void addTraceValues(double x,
                        double y,
                        double z,
                        const QString &traceId,
                        const QVector<std::complex<double>> &values);
    void finalize();

private:
    static QString makeKey(double x, double y, double z, const QString &traceId);
    static QString coordinateKey(double x, double y, double z);

    QVector<double> freqs_;
    QStringList traceIds_;
    QVector<double> xs_;
    QVector<double> ys_;
    QVector<double> zs_;
    QMap<QString, QVector<std::complex<double>>> data_;
    QSet<QString> pointKeys_;
    mutable QString lastError_;
    bool valid_ = false;
};

} // namespace NFSScanner::Analysis
