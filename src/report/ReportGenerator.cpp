#include "report/ReportGenerator.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QStringConverter>
#include <QTextDocument>
#include <QTextStream>

namespace NFSScanner::Report {

bool ReportGenerator::exportHtml(const ReportData &data, const QString &outputPath) const
{
    lastError_.clear();
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        lastError_ = QStringLiteral("无法写入 HTML：%1").arg(outputPath);
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                          "<title>NFS Scanner Report</title></head><body>\n");
    out << buildSummaryHtml(data);
    out << QStringLiteral("</body></html>\n");
    return true;
}

bool ReportGenerator::exportMarkdown(const ReportData &data, const QString &outputPath) const
{
    lastError_.clear();
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        lastError_ = QStringLiteral("无法写入 Markdown：%1").arg(outputPath);
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("# NFS Scanner 报告\n\n");
    out << QStringLiteral("- 项目：%1\n").arg(data.projectName);
    out << QStringLiteral("- 扫描目录：%1\n").arg(data.scanTaskDir);
    out << QStringLiteral("- 时间：%1\n").arg(data.scanTime.toString(Qt::ISODate));
    out << QStringLiteral("- Trace：%1\n").arg(data.traceId);
    out << QStringLiteral("- 探头方向：%1\n").arg(data.probeOrientation.isEmpty() ? QStringLiteral("Hx") : data.probeOrientation);
    out << QStringLiteral("- LUT：%1\n").arg(data.lutName);
    out << QStringLiteral("- 范围：%1 ~ %2\n").arg(data.vmin).arg(data.vmax);
    out << QStringLiteral("\n## 备注\n\n%1\n").arg(data.notes);
    return true;
}

bool ReportGenerator::exportPdf(const ReportData &data, const QString &outputPath) const
{
    lastError_.clear();
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        lastError_ = QStringLiteral("无法写入 PDF：%1").arg(outputPath);
        return false;
    }
    file.close();

    QPdfWriter writer(outputPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);

    QString html = buildSummaryHtml(data);
    if (!data.heatmapImage.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        data.heatmapImage.save(&buffer, "PNG");
        html += QStringLiteral("<p><img src=\"data:image/png;base64,%1\" width=\"480\"/></p>")
                    .arg(QString::fromLatin1(bytes.toBase64()));
    }

    QTextDocument document;
    document.setHtml(html);
    document.print(&writer);
    return true;
}

bool ReportGenerator::exportPngImages(const ReportData &data, const QString &outputDirectory) const
{
    lastError_.clear();
    QDir dir(outputDirectory);
    if (!dir.mkpath(QStringLiteral("."))) {
        lastError_ = QStringLiteral("无法创建导出目录。");
        return false;
    }

    bool ok = true;
    if (!data.heatmapImage.isNull()) {
        ok = data.heatmapImage.save(dir.filePath(QStringLiteral("heatmap.png")));
    }
    if (ok && !data.screenshotImage.isNull()) {
        ok = data.screenshotImage.save(dir.filePath(QStringLiteral("screenshot.png")));
    }
    if (!ok) {
        lastError_ = QStringLiteral("PNG 导出失败。");
    }
    return ok;
}

QString ReportGenerator::lastError() const
{
    return lastError_;
}

QString ReportGenerator::buildSummaryHtml(const ReportData &data) const
{
    return QStringLiteral("<h1>NFS Scanner 报告</h1>"
                          "<p><b>项目</b>：%1</p>"
                          "<p><b>扫描</b>：%2</p>"
                          "<p><b>时间</b>：%3</p>"
                          "<p><b>设备</b>：%4</p>"
                          "<p><b>Trace</b>：%5 &nbsp; <b>LUT</b>：%6</p>"
                          "<p><b>范围</b>：%7 ~ %8</p>"
                          "<p>%9</p>")
        .arg(data.projectName,
             data.scanTaskDir,
             data.scanTime.toString(Qt::ISODate),
             data.deviceSummary,
             data.traceId,
             data.lutName,
             QString::number(data.vmin, 'g', 6),
             QString::number(data.vmax, 'g', 6),
             data.notes.toHtmlEscaped());
}

} // namespace NFSScanner::Report
