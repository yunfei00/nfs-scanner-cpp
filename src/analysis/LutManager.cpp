#include "analysis/LutManager.h"

#include <QColor>

#include <algorithm>
#include <array>
#include <cmath>

namespace NFSScanner::Analysis {

namespace {

struct Stop
{
    double t;
    double r;
    double g;
    double b;
};

int clampAlpha(int alpha)
{
    return std::clamp(alpha, 0, 255);
}

QRgb rgba(double r, double g, double b, int alpha)
{
    return qRgba(qRound(std::clamp(r, 0.0, 1.0) * 255.0),
                 qRound(std::clamp(g, 0.0, 1.0) * 255.0),
                 qRound(std::clamp(b, 0.0, 1.0) * 255.0),
                 clampAlpha(alpha));
}

template <std::size_t N>
QRgb interpolateStops(const std::array<Stop, N> &stops, double t, int alpha)
{
    if (t <= stops.front().t) {
        return rgba(stops.front().r, stops.front().g, stops.front().b, alpha);
    }
    if (t >= stops.back().t) {
        return rgba(stops.back().r, stops.back().g, stops.back().b, alpha);
    }

    for (std::size_t i = 1; i < N; ++i) {
        if (t <= stops[i].t) {
            const Stop &a = stops[i - 1];
            const Stop &b = stops[i];
            const double span = std::max(1e-12, b.t - a.t);
            const double u = (t - a.t) / span;
            return rgba(a.r + (b.r - a.r) * u,
                        a.g + (b.g - a.g) * u,
                        a.b + (b.b - a.b) * u,
                        alpha);
        }
    }

    return rgba(stops.back().r, stops.back().g, stops.back().b, alpha);
}

QRgb colorFromStops(const QString &name, double t, int alpha)
{
    static constexpr std::array<Stop, 6> viridis{{
        {0.00, 0.267, 0.005, 0.329},
        {0.20, 0.254, 0.265, 0.530},
        {0.40, 0.164, 0.471, 0.558},
        {0.60, 0.135, 0.659, 0.518},
        {0.80, 0.478, 0.821, 0.318},
        {1.00, 0.993, 0.906, 0.144},
    }};
    static constexpr std::array<Stop, 6> plasma{{
        {0.00, 0.050, 0.030, 0.528},
        {0.20, 0.417, 0.001, 0.658},
        {0.40, 0.692, 0.165, 0.565},
        {0.60, 0.881, 0.392, 0.383},
        {0.80, 0.988, 0.653, 0.211},
        {1.00, 0.940, 0.975, 0.131},
    }};
    static constexpr std::array<Stop, 6> inferno{{
        {0.00, 0.001, 0.000, 0.014},
        {0.20, 0.258, 0.039, 0.406},
        {0.40, 0.578, 0.148, 0.404},
        {0.60, 0.865, 0.317, 0.226},
        {0.80, 0.988, 0.645, 0.039},
        {1.00, 0.988, 0.998, 0.645},
    }};
    static constexpr std::array<Stop, 6> magma{{
        {0.00, 0.001, 0.000, 0.014},
        {0.20, 0.232, 0.060, 0.438},
        {0.40, 0.550, 0.161, 0.506},
        {0.60, 0.868, 0.288, 0.409},
        {0.80, 0.995, 0.624, 0.427},
        {1.00, 0.987, 0.991, 0.749},
    }};
    static constexpr std::array<Stop, 5> hot{{
        {0.00, 0.000, 0.000, 0.000},
        {0.33, 0.750, 0.000, 0.000},
        {0.62, 1.000, 0.650, 0.000},
        {0.84, 1.000, 1.000, 0.250},
        {1.00, 1.000, 1.000, 1.000},
    }};
    static constexpr std::array<Stop, 5> rainbow{{
        {0.00, 0.180, 0.000, 0.360},
        {0.25, 0.000, 0.350, 1.000},
        {0.50, 0.000, 0.800, 0.200},
        {0.75, 1.000, 0.850, 0.000},
        {1.00, 0.850, 0.000, 0.000},
    }};

    if (name == QStringLiteral("viridis")) {
        return interpolateStops(viridis, t, alpha);
    }
    if (name == QStringLiteral("plasma")) {
        return interpolateStops(plasma, t, alpha);
    }
    if (name == QStringLiteral("inferno")) {
        return interpolateStops(inferno, t, alpha);
    }
    if (name == QStringLiteral("magma")) {
        return interpolateStops(magma, t, alpha);
    }
    if (name == QStringLiteral("hot")) {
        return interpolateStops(hot, t, alpha);
    }
    if (name == QStringLiteral("rainbow")) {
        return interpolateStops(rainbow, t, alpha);
    }

    return qRgba(0, 0, 0, clampAlpha(alpha));
}

} // namespace

QStringList LutManager::availableLuts()
{
    return {
        QStringLiteral("gray"),
        QStringLiteral("jet"),
        QStringLiteral("turbo"),
        QStringLiteral("viridis"),
        QStringLiteral("plasma"),
        QStringLiteral("inferno"),
        QStringLiteral("magma"),
        QStringLiteral("hot"),
        QStringLiteral("cool"),
        QStringLiteral("rainbow"),
    };
}

QRgb LutManager::colorAt(const QString &lutName, double t, int alpha)
{
    const QString name = lutName.trimmed().toLower();
    const double value = clamp01(t);
    const int safeAlpha = clampAlpha(alpha);

    if (name == QStringLiteral("gray")) {
        return rgba(value, value, value, safeAlpha);
    }

    if (name == QStringLiteral("cool")) {
        return rgba(value, 1.0 - value, 1.0, safeAlpha);
    }

    if (name == QStringLiteral("jet")) {
        const double r = std::clamp(1.5 - std::abs(4.0 * value - 3.0), 0.0, 1.0);
        const double g = std::clamp(1.5 - std::abs(4.0 * value - 2.0), 0.0, 1.0);
        const double b = std::clamp(1.5 - std::abs(4.0 * value - 1.0), 0.0, 1.0);
        return rgba(r, g, b, safeAlpha);
    }

    if (name == QStringLiteral("turbo")) {
        // Polynomial approximation of Google's Turbo colormap.
        const double x = value;
        const double r = 0.13572138 + x * (4.61539260 + x * (-42.66032258 + x * (132.13108234 + x * (-152.94239396 + x * 59.28637943))));
        const double g = 0.09140261 + x * (2.19418839 + x * (4.84296658 + x * (-14.18503333 + x * (4.27729857 + x * 2.82956604))));
        const double b = 0.10667330 + x * (12.64194608 + x * (-60.58204836 + x * (110.36276771 + x * (-89.90310912 + x * 27.34824973))));
        return rgba(r, g, b, safeAlpha);
    }

    return colorFromStops(name, value, safeAlpha);
}

QImage LutManager::createColorbar(const QString &lutName, int width, int height, int alpha)
{
    const int safeWidth = std::max(1, width);
    const int safeHeight = std::max(1, height);
    QImage image(safeWidth, safeHeight, QImage::Format_ARGB32);

    for (int y = 0; y < safeHeight; ++y) {
        const double t = 1.0 - static_cast<double>(y) / static_cast<double>(std::max(1, safeHeight - 1));
        const QRgb color = colorAt(lutName, t, alpha);
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < safeWidth; ++x) {
            line[x] = color;
        }
    }

    return image;
}

double LutManager::clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

} // namespace NFSScanner::Analysis
