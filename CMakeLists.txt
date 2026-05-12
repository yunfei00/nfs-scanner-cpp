cmake_minimum_required(VERSION 3.20)

project(NFSScannerCpp
    VERSION 0.7.0
    DESCRIPTION "Near-field scanning system C++/Qt6 rewrite"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Network)

set(PROJECT_SOURCES
    src/main.cpp

    src/app/AppContext.h
    src/app/AppContext.cpp
    src/app/AppVersion.h

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/HeatmapDialog.h
    src/ui/HeatmapDialog.cpp
    src/ui/HeatmapView.h
    src/ui/HeatmapView.cpp

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

    src/devices/motion/IMotionController.h
    src/devices/motion/MockMotionController.h
    src/devices/motion/MockMotionController.cpp
    src/devices/motion/SerialMotionController.h
    src/devices/motion/SerialMotionController.cpp

    src/devices/spectrum/ISpectrumAnalyzer.h
    src/devices/spectrum/SpectrumConfig.h
    src/devices/spectrum/SpectrumTrace.h
    src/devices/spectrum/MockSpectrumAnalyzer.h
    src/devices/spectrum/MockSpectrumAnalyzer.cpp
    src/devices/spectrum/ScpiTcpClient.h
    src/devices/spectrum/ScpiTcpClient.cpp
    src/devices/spectrum/GenericScpiSpectrumAnalyzer.h
    src/devices/spectrum/GenericScpiSpectrumAnalyzer.cpp
    src/devices/spectrum/Zna67SpectrumAnalyzer.h
    src/devices/spectrum/Zna67SpectrumAnalyzer.cpp
    src/devices/spectrum/SpectrumAnalyzerFactory.h
    src/devices/spectrum/SpectrumAnalyzerFactory.cpp

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

    resources/app.qrc
)

add_executable(NFSScanner ${PROJECT_SOURCES})

set_target_properties(NFSScanner PROPERTIES
    OUTPUT_NAME NFSScanner
)

target_include_directories(NFSScanner
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(NFSScanner
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::SerialPort
        Qt6::Widgets
)

if(MSVC)
    target_compile_options(NFSScanner PRIVATE /utf-8)
endif()

if(WIN32)
    set_target_properties(NFSScanner PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS NFSScanner
    RUNTIME DESTINATION bin
)
