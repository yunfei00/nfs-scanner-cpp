#pragma once

#include <QImage>
#include <QRgb>
#include <QString>
#include <QStringList>

namespace NFSScanner::Analysis {

class LutManager
{
public:
    static QStringList availableLuts();
    static QRgb colorAt(const QString &lutName, double t, int alpha = 255);
    static QImage createColorbar(const QString &lutName, int width, int height, int alpha = 255);

private:
    static double clamp01(double value);
};

} // namespace NFSScanner::Analysis
