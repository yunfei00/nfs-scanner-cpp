#include "ui/pages/ScanPage.h"

#include "ui/HeatmapView.h"

#include <QVBoxLayout>

namespace NFSScanner::UI {

ScanPage::ScanPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("scanPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    canvas_ = new HeatmapView(this);
    canvas_->setObjectName(QStringLiteral("scanCanvasView"));
    layout->addWidget(canvas_, 1);
}

HeatmapView *ScanPage::canvas() const
{
    return canvas_;
}

} // namespace NFSScanner::UI
