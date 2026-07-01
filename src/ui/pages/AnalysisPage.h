#pragma once

#include "ui/AnalysisController.h"

#include <QImage>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::UI {

class HeatmapView;

class AnalysisPage final : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisPage(QWidget *parent = nullptr);

    HeatmapView *previewCanvas() const;
    QLabel *hintLabel() const;
    QWidget *paramPanel() const;

    void bind(HeatmapView *scanCanvas,
              NFSScanner::Project::ProjectManager *projectManager,
              QWidget *messageBoxParent);

    void loadFrequencyData();
    void setResultDir(const QString &dir);
    QString resultDir() const;
    void refreshProjectPaths();
    QString currentTraceId() const;
    QString currentLutName() const;
    double currentVmin() const;
    double currentVmax() const;
    QImage heatmapImage() const;

    void showHeatmap();
    void exportAnalysisConfigJson();

signals:
    void logMessage(const QString &text);

private:
    QGroupBox *createResultGroup();
    void populateFrequencyControls();
    bool refreshHeatmapPreview();
    void scheduleHeatmapPreviewRefresh();
    void updateHeatmapCursorReadout(double worldX, double worldY, bool insideImage);
    void applyHeatmapPreviewToCanvases();
    void updateColorbarDisplay();
    void updateOpacityLabel(int percent);
    AnalysisRenderParams buildAnalysisParams() const;
    QString resolveTraceCsvPath() const;
    QString selectedDisplayMode() const;
    QString formatFrequency(double hz) const;

    AnalysisController *analysisController_ = nullptr;
    HeatmapView *scanCanvas_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    QWidget *messageBoxParent_ = nullptr;

    QWidget *paramPanel_ = nullptr;
    HeatmapView *previewCanvas_ = nullptr;
    QLabel *hintLabel_ = nullptr;

    QLineEdit *resultDirEdit_ = nullptr;
    QComboBox *traceCombo_ = nullptr;
    QComboBox *frequencyCombo_ = nullptr;
    QComboBox *displayModeCombo_ = nullptr;
    QComboBox *lutCombo_ = nullptr;
    QCheckBox *autoRangeCheck_ = nullptr;
    QDoubleSpinBox *vminSpin_ = nullptr;
    QDoubleSpinBox *vmaxSpin_ = nullptr;
    QSlider *opacitySlider_ = nullptr;
    QLabel *opacityLabel_ = nullptr;
    QLabel *colorbarLabel_ = nullptr;
    QLabel *colorbarMinLabel_ = nullptr;
    QLabel *colorbarMaxLabel_ = nullptr;
};

} // namespace NFSScanner::UI
