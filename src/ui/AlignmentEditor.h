#pragma once

#include "core/AlignmentConfig.h"
#include "core/AlignmentManager.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace NFSScanner::UI {

class AlignmentEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit AlignmentEditor(QWidget *parent = nullptr);

    NFSScanner::Core::AlignmentManager *manager();

signals:
    void configApplied(const NFSScanner::Core::AlignmentConfig &config);

private:
    void syncFromUi();
    void syncToUi();
    void loadBackgroundImage();

    NFSScanner::Core::AlignmentManager manager_;
    QDoubleSpinBox *worldXMin_ = nullptr;
    QDoubleSpinBox *worldXMax_ = nullptr;
    QDoubleSpinBox *worldYMin_ = nullptr;
    QDoubleSpinBox *worldYMax_ = nullptr;
    QDoubleSpinBox *pixelXMin_ = nullptr;
    QDoubleSpinBox *pixelXMax_ = nullptr;
    QDoubleSpinBox *pixelYMin_ = nullptr;
    QDoubleSpinBox *pixelYMax_ = nullptr;
    QLabel *backgroundLabel_ = nullptr;
};

} // namespace NFSScanner::UI
