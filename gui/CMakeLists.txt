cmake_minimum_required(VERSION 3.21)
project(ImageResolutionAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# Everything except main(), so the tests link the same code as the app.
add_library(imgan_core STATIC
    src/mainwindow.cpp
    src/backend/backendlocator.cpp
    src/backend/backendprocess.cpp
    src/backend/importselection.cpp
    src/backend/protocolparser.cpp
    src/ui/brand.cpp
    src/ui/theme.cpp
    src/ui/uiformat.cpp
    src/widgets/aboutdialog.cpp
    src/widgets/appheader.cpp
    src/widgets/brandlockup.cpp
    src/widgets/distributionbar.cpp
    src/widgets/viewfinder.cpp
    src/widgets/chamferbutton.cpp
    src/widgets/toggleswitch.cpp
    src/widgets/phaserail.cpp
    src/widgets/elidedlabel.cpp
    src/widgets/importdialog.cpp
    src/widgets/metriccard.cpp
    src/widgets/progresspanel.cpp
    src/widgets/reportselector.cpp
    src/widgets/resultspanel.cpp
    src/widgets/sectioncard.cpp
    src/widgets/sourcepicker.cpp
)
target_include_directories(imgan_core PUBLIC src)
target_link_libraries(imgan_core PUBLIC Qt6::Widgets)

# Executable keeps its file name (run.bat, tooling); the Windows resource carries the RESORA
# icon and display name (ProductName / FileDescription).
set(RESORA_RC "")
if(WIN32)
    set(RESORA_RC assets/resora.rc)
    set_source_files_properties(${RESORA_RC} PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/resora.ico)
endif()
qt_add_executable(ImageResolutionAnalyzer WIN32 src/main.cpp ${RESORA_RC})
target_link_libraries(ImageResolutionAnalyzer PRIVATE imgan_core)

enable_testing()
add_subdirectory(tests)
