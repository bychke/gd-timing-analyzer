cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(WaveformTiming VERSION 1.1.0)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/Session.cpp
    src/timing/TimingMap.cpp
    src/analysis/Analyzer.cpp
    src/audio/AudioData.cpp
    src/audio/Metronome.cpp
    src/ui/TimingEditor.cpp
    src/ui/LayoutPopup.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Brak zmiennej GEODE_SDK - zainstaluj Geode SDK (geode sdk install)")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} shell32 opengl32 comdlg32)
endif()
