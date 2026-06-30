#pragma once

#include <QWidget>

namespace NFSScanner::UI {

class HeatmapView;

class ScanPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ScanPage(QWidget *parent = nullptr);

    HeatmapView *canvas() const;

private:
    HeatmapView *canvas_ = nullptr;
};

} // namespace NFSScanner::UI
