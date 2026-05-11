#pragma once

#include <QObject>

#include <memory>

namespace NFSScanner::Core {
class ScanWorker;
}

namespace NFSScanner::Devices::Motion {
class IMotionController;
}

namespace NFSScanner::Devices::Spectrum {
class ISpectrumAnalyzer;
}

namespace NFSScanner::App {

class AppContext final : public QObject
{
    Q_OBJECT

public:
    explicit AppContext(QObject *parent = nullptr);
    ~AppContext() override;

    Core::ScanWorker *scanWorker() const;
    Devices::Motion::IMotionController *motionController() const;
    Devices::Spectrum::ISpectrumAnalyzer *spectrumAnalyzer() const;

private:
    std::unique_ptr<Core::ScanWorker> scanWorker_;
    std::unique_ptr<Devices::Motion::IMotionController> motionController_;
    std::unique_ptr<Devices::Spectrum::ISpectrumAnalyzer> spectrumAnalyzer_;
};

} // namespace NFSScanner::App
