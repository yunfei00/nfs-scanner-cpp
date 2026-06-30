#include "ui/pages/ReportPage.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace NFSScanner::UI {

ReportPage::ReportPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("reportPage"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    reportList_ = new QListWidget(this);
    reportList_->setObjectName(QStringLiteral("reportList"));
    reportList_->addItems(QStringList{
        QStringLiteral("扫描报告 - Demo 001"),
        QStringLiteral("扫描报告 - Demo 002"),
        QStringLiteral("分析摘要 - Demo 003"),
    });
    reportList_->setFixedWidth(240);

    previewEditor_ = new QPlainTextEdit(this);
    previewEditor_->setObjectName(QStringLiteral("reportPreviewEditor"));
    previewEditor_->setReadOnly(true);
    previewEditor_->setPlainText(QStringLiteral(
        "NFS Scanner 报告预览\n\n"
        "选择左侧报告条目后，在此显示摘要。\n"
        "可通过「工具 → 导出报告」导出 HTML / Markdown。"));

    layout->addWidget(reportList_);
    layout->addWidget(previewEditor_, 1);

    connect(reportList_, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        if (!previewEditor_ || text.isEmpty()) {
            return;
        }
        previewEditor_->setPlainText(QStringLiteral("报告：%1\n\n"
                                                    "项目：Demo Project\n"
                                                    "扫描时间：2026-06-30\n"
                                                    "Trace：Trc1_S21\n"
                                                    "备注：Mock 报告预览。").arg(text));
    });
    if (reportList_->count() > 0) {
        reportList_->setCurrentRow(0);
    }
}

QListWidget *ReportPage::reportList() const
{
    return reportList_;
}

QPlainTextEdit *ReportPage::previewEditor() const
{
    return previewEditor_;
}

} // namespace NFSScanner::UI
