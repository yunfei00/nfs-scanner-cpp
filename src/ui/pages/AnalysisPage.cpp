#include "ui/pages/AnalysisPage.h"

#include "ui/HeatmapView.h"

#include <QLabel>
#include <QVBoxLayout>

namespace NFSScanner::UI {

AnalysisPage::AnalysisPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("analysisPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    hintLabel_ = new QLabel(QStringLiteral("分析工作区：加载 traces.csv 后在此预览热力图。"), this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setAlignment(Qt::AlignCenter);

    previewCanvas_ = new HeatmapView(this);
    previewCanvas_->setObjectName(QStringLiteral("analysisCanvasView"));
    previewCanvas_->setMinimumHeight(320);

    layout->addWidget(hintLabel_, 0);
    layout->addWidget(previewCanvas_, 1);
}

HeatmapView *AnalysisPage::previewCanvas() const
{
    return previewCanvas_;
}

QLabel *AnalysisPage::hintLabel() const
{
    return hintLabel_;
}

} // namespace NFSScanner::UI
