cmake_minimum_required(VERSION 3.20)

project(NFSScannerCpp
    VERSION 0.10.0
    DESCRIPTION "Near-field scanning system C++/Qt6 rewrite"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Network PrintSupport)

set(CORE_SOURCES
    src/app/AppContext.h
    src/app/AppContext.cpp
    src/app/AppVersion.h

    src/core/ScanConfig.h
    src/core/ScanConfig.cpp
    src/core/ScanPoint.h
    src/core/ScanPoint.cpp
    src/core/ScanResult.h
    src/core/ScanResult.cpp
    src/core/ScanPathPlanner.h
    src/core/ScanPathPlanner.cpp
    src/core/ScanManager.h
    src/core/ScanManager.cpp
    src/core/ScanWorker.h
    src/core/ScanWorker.cpp
    src/core/DeviceManager.h
    src/core/DeviceManager.cpp
    src/core/AlignmentConfig.h
    src/core/AlignmentConfig.cpp
    src/core/AlignmentManager.h
    src/core/AlignmentManager.cpp

    src/devices/motion/IMotionController.h
    src/devices/motion/MockMotionController.h
    src/devices/motion/MockMotionController.cpp
    src/devices/motion/SerialMotionController.h
    src/devices/motion/SerialMotionController.cpp

    src/devices/camera/ICamera.h
    src/devices/camera/MockCamera.h
    src/devices/camera/MockCamera.cpp

    src/devices/spectrum/ISpectrumAnalyzer.h
    src/devices/spectrum/SpectrumConfig.h
    src/devices/spectrum/SpectrumTrace.h
    src/devices/spectrum/SpectrumParsingUtils.h
    src/devices/spectrum/SpectrumParsingUtils.cpp
    src/devices/spectrum/SpectrumAcquisitionRequest.h
    src/devices/spectrum/SpectrumAcquisitionResult.h
    src/devices/spectrum/SpectrumAcquisitionWorker.h
    src/devices/spectrum/SpectrumAcquisitionWorker.cpp
    src/devices/spectrum/MockSpectrumAnalyzer.h
    src/devices/spectrum/MockSpectrumAnalyzer.cpp
    src/devices/spectrum/ScpiTcpClient.h
    src/devices/spectrum/ScpiTcpClient.cpp
    src/devices/spectrum/GenericScpiSpectrumAnalyzer.h
    src/devices/spectrum/GenericScpiSpectrumAnalyzer.cpp
    src/devices/spectrum/Zna67SpectrumAnalyzer.h
    src/devices/spectrum/Zna67SpectrumAnalyzer.cpp
    src/devices/spectrum/FswSpectrumAnalyzer.h
    src/devices/spectrum/FswSpectrumAnalyzer.cpp
    src/devices/spectrum/N9020aSpectrumAnalyzer.h
    src/devices/spectrum/N9020aSpectrumAnalyzer.cpp
    src/devices/spectrum/SpectrumAnalyzerFactory.h
    src/devices/spectrum/SpectrumAnalyzerFactory.cpp
    src/devices/spectrum/SpectrumDeviceHost.h
    src/devices/spectrum/SpectrumDeviceHost.cpp

    src/project/Project.h
    src/project/Project.cpp
    src/project/ProjectManager.h
    src/project/ProjectManager.cpp

    src/report/ReportData.h
    src/report/ReportData.cpp
    src/report/ReportGenerator.h
    src/report/ReportGenerator.cpp

    src/license/MachineId.h
    src/license/MachineId.cpp
    src/license/LicenseSignatureVerifier.h
    src/license/LicenseSignatureVerifier.cpp
    src/license/LicenseManager.h
    src/license/LicenseManager.cpp

    src/storage/TaskStorage.h
    src/storage/TaskStorage.cpp

    src/analysis/HeatmapGenerator.h
    src/analysis/HeatmapGenerator.cpp
    src/analysis/LutManager.h
    src/analysis/LutManager.cpp
    src/analysis/FrequencyData.h
    src/analysis/FrequencyData.cpp
    src/analysis/FrequencyCsvParser.h
    src/analysis/FrequencyCsvParser.cpp

    src/infra/Logger.h
    src/infra/Logger.cpp
)

set(ED25519_C_SOURCES
    src/license/ed25519/fe.c
    src/license/ed25519/ge.c
    src/license/ed25519/sc.c
    src/license/ed25519/sha512.c
    src/license/ed25519/keypair.c
    src/license/ed25519/sign.c
    src/license/ed25519/verify.c
)
set_source_files_properties(${ED25519_C_SOURCES} PROPERTIES LANGUAGE C)
list(APPEND CORE_SOURCES ${ED25519_C_SOURCES})

set(UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/UiFormUtils.h
    src/ui/UiFormUtils.cpp
    src/ui/HeatmapDialog.h
    src/ui/HeatmapDialog.cpp
    src/ui/HeatmapView.h
    src/ui/HeatmapView.cpp
    src/ui/pages/ScanPage.h
    src/ui/pages/ScanPage.cpp
    src/ui/pages/DevicePage.h
    src/ui/pages/DevicePage.cpp
    src/ui/pages/AnalysisPage.h
    src/ui/pages/AnalysisPage.cpp
    src/ui/pages/ReportPage.h
    src/ui/pages/ReportPage.cpp
    src/ui/AlignmentEditor.h
    src/ui/AlignmentEditor.cpp
    src/ui/AnalysisController.h
    src/ui/AnalysisController.cpp
    src/ui/DeviceStatusBar.h
    src/ui/DeviceStatusBar.cpp
)

add_executable(NFSScanner
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    resources/app.qrc
)

add_executable(NFSScannerSelfCheck
    src/tools/self_check.cpp
    ${CORE_SOURCES}
)

set_target_properties(NFSScanner PROPERTIES OUTPUT_NAME NFSScanner)
set_target_properties(NFSScannerSelfCheck PROPERTIES OUTPUT_NAME NFSScannerSelfCheck)

foreach(target NFSScanner NFSScannerSelfCheck)
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_definitions(${target} PRIVATE ED25519_NO_SEED)
    target_link_libraries(${target}
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Network
            Qt6::SerialPort
            Qt6::Widgets
    )
    if(target STREQUAL "NFSScanner")
        target_link_libraries(${target} PRIVATE Qt6::PrintSupport)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
endforeach()

if(WIN32)
    set_target_properties(NFSScanner PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS NFSScanner NFSScannerSelfCheck
    RUNTIME DESTINATION bin
)
