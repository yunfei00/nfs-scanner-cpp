#pragma once

#include <QWidget>

class QLabel;

namespace NFSScanner::UI {

class HeatmapView;

class AnalysisPage final : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisPage(QWidget *parent = nullptr);

    HeatmapView *previewCanvas() const;
    QLabel *hintLabel() const;

private:
    HeatmapView *previewCanvas_ = nullptr;
    QLabel *hintLabel_ = nullptr;
};

} // namespace NFSScanner::UI
