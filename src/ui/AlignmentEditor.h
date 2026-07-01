#pragma once

#include "core/AlignmentConfig.h"
#include "core/AlignmentManager.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
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
    bool loadAlignmentFile(const QString &path);
    bool saveAlignmentFile(const QString &path);

public slots:
    void captureMockBackground(const QImage &frame, const QString &savedPath);

signals:
    void configApplied(const NFSScanner::Core::AlignmentConfig &config);
    void mockCaptureRequested();

private:
    void syncFromUi();
    void syncToUi();
    void loadBackgroundImage();
    void saveAlignmentDialog();
    void loadAlignmentDialog();
    void generatePerspectiveCorners();

    NFSScanner::Core::AlignmentManager manager_;
    QComboBox *mappingModeCombo_ = nullptr;
    QDoubleSpinBox *worldXMin_ = nullptr;
    QDoubleSpinBox *worldXMax_ = nullptr;
    QDoubleSpinBox *worldYMin_ = nullptr;
    QDoubleSpinBox *worldYMax_ = nullptr;
    QDoubleSpinBox *worldZ_ = nullptr;
    QDoubleSpinBox *pixelXMin_ = nullptr;
    QDoubleSpinBox *pixelXMax_ = nullptr;
    QDoubleSpinBox *pixelYMin_ = nullptr;
    QDoubleSpinBox *pixelYMax_ = nullptr;
    QCheckBox *fixedAspectCheck_ = nullptr;
    QLabel *backgroundLabel_ = nullptr;
};

} // namespace NFSScanner::UI
