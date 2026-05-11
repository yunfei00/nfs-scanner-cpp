#include "app/AppContext.h"

#include "core/ScanWorker.h"
#include "devices/motion/MockMotionController.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"

namespace NFSScanner::App {

AppContext::AppContext(QObject *parent)
    : QObject(parent),
      scanWorker_(std::make_unique<Core::ScanWorker>()),
      motionController_(std::make_unique<Devices::Motion::MockMotionController>()),
      spectrumAnalyzer_(std::make_unique<Devices::Spectrum::MockSpectrumAnalyzer>())
{
}

AppContext::~AppContext() = default;

Core::ScanWorker *AppContext::scanWorker() const
{
    return scanWorker_.get();
}

Devices::Motion::IMotionController *AppContext::motionController() const
{
    return motionController_.get();
}

Devices::Spectrum::ISpectrumAnalyzer *AppContext::spectrumAnalyzer() const
{
    return spectrumAnalyzer_.get();
}

} // namespace NFSScanner::App
