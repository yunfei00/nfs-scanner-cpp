#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "core/ScanPathPlanner.h"
#include "analysis/LutManager.h"
#include "project/ProjectManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cstdio>

namespace {

int gFailures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++gFailures;
    } else {
        std::printf("PASS: %s\n", message);
    }
}

void testScanPathSnake()
{
    NFSScanner::Core::ScanConfig config;
    config.startX = 0.0;
    config.endX = 2.0;
    config.stepX = 1.0;
    config.startY = 0.0;
    config.endY = 1.0;
    config.stepY = 1.0;
    config.snakeMode = true;

    NFSScanner::Core::ScanPathPlanner planner;
    const auto points = planner.generate(config);
    check(points.size() == 6, "snake 3x2 grid produces 6 points");
    if (points.size() >= 4) {
        check(points.at(0).x <= points.at(1).x, "row 0 x ascending");
        check(points.at(3).x >= points.at(2).x, "row 1 x reversed in snake mode");
    }
}

void testScanPathInvalidStep()
{
    NFSScanner::Core::ScanConfig config;
    config.stepX = 0.0;
    NFSScanner::Core::ScanPathPlanner planner;
    const auto points = planner.generate(config);
    check(points.isEmpty(), "invalid stepX rejected");
}

void testAlignmentMapping()
{
    NFSScanner::Core::AlignmentManager manager;
    NFSScanner::Core::AlignmentConfig config;
    config.enabled = true;
    config.worldXMin = 0.0;
    config.worldXMax = 100.0;
    config.worldYMin = 0.0;
    config.worldYMax = 50.0;
    config.pixelXMin = 0.0;
    config.pixelXMax = 200.0;
    config.pixelYMin = 0.0;
    config.pixelYMax = 100.0;
    manager.setConfig(config);

    const QPointF px = manager.worldToPixel(50.0, 25.0);
    check(qAbs(px.x() - 100.0) < 0.01, "worldToPixel x center");
    check(qAbs(px.y() - 50.0) < 0.01, "worldToPixel y center");

    const QPointF world = manager.pixelToWorld(200.0, 100.0);
    check(qAbs(world.x() - 100.0) < 0.01, "pixelToWorld x max");
    check(qAbs(world.y() - 50.0) < 0.01, "pixelToWorld y max");
}

void testLutManager()
{
    const QStringList luts = NFSScanner::Analysis::LutManager::availableLuts();
    check(!luts.isEmpty(), "LUT list non-empty");
    check(luts.contains(QStringLiteral("turbo")), "turbo LUT exists");

    const QImage bar = NFSScanner::Analysis::LutManager::createColorbar(QStringLiteral("turbo"), 28, 120, 255);
    check(!bar.isNull() && bar.width() == 28 && bar.height() == 120, "colorbar size");
}

void testProjectCreate()
{
    QTemporaryDir temp;
    check(temp.isValid(), "temp dir valid");
    NFSScanner::Project::ProjectManager manager;
    const bool ok = manager.createProject(QStringLiteral("SelfCheckProject"), temp.path());
    check(ok, "project create");
    check(QDir(manager.currentProject().scansDir()).exists(), "project scans dir exists");
    check(QFile::exists(manager.currentProject().projectJsonPath()), "project.json exists");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    std::printf("NFSScanner Self Check\n");
    testScanPathSnake();
    testScanPathInvalidStep();
    testAlignmentMapping();
    testLutManager();
    testProjectCreate();

    if (gFailures == 0) {
        std::printf("All self-check tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) failed.\n", gFailures);
    return 1;
}
