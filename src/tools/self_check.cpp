#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "core/ScanPathPlanner.h"
#include "analysis/LutManager.h"
#include "project/ProjectManager.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"

#include <QCoreApplication>
#include <QDateTime>
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

void testAlignmentJsonRoundTrip()
{
    QTemporaryDir temp;
    check(temp.isValid(), "alignment temp dir valid");

    NFSScanner::Core::AlignmentManager manager;
    NFSScanner::Core::AlignmentConfig config;
    config.enabled = true;
    config.worldXMin = 10.0;
    config.worldXMax = 110.0;
    config.worldYMin = 5.0;
    config.worldYMax = 55.0;
    config.pixelXMin = 0.0;
    config.pixelXMax = 400.0;
    config.pixelYMin = 0.0;
    config.pixelYMax = 200.0;
    config.fixedAspectRatio = true;
    manager.setConfig(config);

    const QString path = QDir(temp.path()).filePath(QStringLiteral("alignment.json"));
    check(manager.saveToFile(path), "alignment json save");
    check(QFile::exists(path), "alignment json file exists");

    NFSScanner::Core::AlignmentManager loaded;
    check(loaded.loadFromFile(path), "alignment json load");
    check(qAbs(loaded.config().worldXMax - 110.0) < 0.01, "alignment json world x max");
    check(loaded.config().fixedAspectRatio, "alignment json fixed aspect ratio");
}

void testReportExport()
{
    QTemporaryDir temp;
    check(temp.isValid(), "report temp dir valid");

    NFSScanner::Report::ReportData data;
    data.projectName = QStringLiteral("SelfCheckProject");
    data.operatorName = QStringLiteral("SelfCheck");
    data.scanTaskDir = temp.path();
    data.scanTime = QDateTime::currentDateTimeUtc();
    data.traceId = QStringLiteral("S11");
    data.lutName = QStringLiteral("turbo");
    data.vmin = -40.0;
    data.vmax = 0.0;
    data.notes = QStringLiteral("mock report from self_check");

    NFSScanner::Report::ReportGenerator generator;
    const QString mdPath = QDir(temp.path()).filePath(QStringLiteral("report.md"));
    const QString htmlPath = QDir(temp.path()).filePath(QStringLiteral("report.html"));
    check(generator.exportMarkdown(data, mdPath), "report markdown export");
    check(QFile::exists(mdPath), "report markdown file exists");
    check(generator.exportHtml(data, htmlPath), "report html export");
    check(QFile::exists(htmlPath), "report html file exists");
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
    testAlignmentJsonRoundTrip();
    testLutManager();
    testProjectCreate();
    testReportExport();

    if (gFailures == 0) {
        std::printf("All self-check tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) failed.\n", gFailures);
    return 1;
}
