cmake_minimum_required(VERSION 3.21)
project(ImageResolutionAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Visual assets live in the repository-level Assets/ directory (see Assets/README.md).
set(RESORA_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/../Assets)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Test)

# .resora container library: Qt Core only, no widgets / dashboard code (docs/RESORA_FORMAT.md).
add_library(resora_format STATIC
    include/resora/resorafmt.h include/resora/resorareader.h include/resora/resorawriter.h
    include/resora/bundledexport.h include/resora/destinationfinalizer.h
    include/resora/exportcommon.h include/resora/linkedexport.h
    include/resora/resoraimport.h include/resora/importinternal.h
    src/resora/resorafmt.cpp src/resora/resorareader.cpp src/resora/resorawriter.cpp
    src/resora/bundledexport.cpp src/resora/destinationfinalizer.cpp
    src/resora/exportcommon.cpp src/resora/linkedexport.cpp
    src/resora/importparse.cpp src/resora/linkresolve.cpp src/resora/bundledextract.cpp
    src/resora/importdocument.cpp src/resora/cachesweep.cpp)
target_include_directories(resora_format PUBLIC include)
target_link_libraries(resora_format PUBLIC Qt6::Core)

# Headers live in include/ (apart from their .cpp in src/); listing them as sources lets AUTOMOC
# find Q_OBJECT classes. Re-globbed on every build so new headers are picked up.
file(GLOB_RECURSE RESORA_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# Everything except main(), so the tests link the same code as the app.
add_library(imgan_core STATIC
    ${RESORA_HEADERS}
    src/mainwindow.cpp
    src/backend/analyticsmodel.cpp
    src/backend/backendlocator.cpp
    src/backend/backendprocess.cpp
    src/backend/comparison.cpp
    src/backend/duplicatesmodel.cpp
    src/widgets/duplicatefinder.cpp
    src/widgets/duplicatefinder_results.cpp
    src/backend/previewloader.cpp
    src/widgets/imagepane.cpp
    src/widgets/comparisonviewer.cpp
    src/widgets/comparisontable.cpp
    src/backend/insightmetrics.cpp
    src/widgets/insightcanvas.cpp
    src/widgets/resolutionchart.cpp
    src/widgets/filesizebars.cpp
    src/widgets/dimensiondiagram.cpp
    src/widgets/aspectindicator.cpp
    src/widgets/metadatacompare.cpp
    src/widgets/collapsiblesection.cpp
    src/widgets/insightsview.cpp
    src/widgets/comparisonworkspace.cpp
    src/widgets/comparisonworkspace_ui.cpp
    src/backend/importselection.cpp
    src/backend/propertiesmodel.cpp
    src/backend/protocolparser.cpp
    src/locations/savedlocation.cpp
    src/locations/locationstore.cpp
    src/locations/locationrecommender.cpp
    src/ui/brand.cpp
    src/ui/icons.cpp
    src/ui/theme.cpp
    src/ui/uiformat.cpp
    src/widgets/aboutdialog.cpp
    src/widgets/analyticspanel.cpp
    src/widgets/analyticsdashboard.cpp
    src/widgets/chartbase.cpp
    src/widgets/barchart.cpp
    src/widgets/donutchart.cpp
    src/widgets/hbarchart.cpp
    src/widgets/segmentedswitch.cpp
    src/backend/dashboardmodel.cpp
    src/backend/dashboardserialize.cpp src/backend/dashboardrestore.cpp src/backend/dashboardsession.cpp
    src/backend/resoraworkflow.cpp src/backend/resorajobs.cpp src/backend/relinkmodel.cpp
    src/widgets/confirmdialog.cpp src/widgets/exportdialog.cpp src/widgets/relinkpanel.cpp
    src/widgets/openresoradialog.cpp
    src/widgets/pixelspan.cpp
    src/widgets/sharechart.cpp
    src/widgets/appheader.cpp
    src/widgets/brandlockup.cpp
    src/widgets/distributionbar.cpp
    src/widgets/viewfinder.cpp
    src/widgets/chamferbutton.cpp
    src/widgets/toggleswitch.cpp
    src/widgets/phaserail.cpp
    src/widgets/toolnav.cpp
    src/widgets/elidedlabel.cpp
    src/widgets/flowlayout.cpp
    src/widgets/framelessdialog.cpp
    src/widgets/importdialog.cpp
    src/widgets/locationrow.cpp
    src/widgets/locationsuggester.cpp
    src/widgets/savedlocationspanel.cpp
    src/widgets/settingsdialog.cpp
    src/widgets/metriccard.cpp
    src/widgets/progresspanel.cpp
    src/widgets/propertiesdialog.cpp
    src/widgets/reportselector.cpp
    src/widgets/resultspanel.cpp
    src/widgets/sectioncard.cpp
    src/widgets/sourcepicker.cpp
    ${RESORA_ASSETS}/resora.qrc
)
target_include_directories(imgan_core PUBLIC include)
target_link_libraries(imgan_core PUBLIC Qt6::Widgets Qt6::Svg resora_format)

# Executable keeps its file name (run.bat, tooling); the Windows resource carries the RESORA
# icon and display name (ProductName / FileDescription).
set(RESORA_RC "")
if(WIN32)
    set(RESORA_RC ${RESORA_ASSETS}/app/resora.rc)
    set_source_files_properties(${RESORA_RC} PROPERTIES OBJECT_DEPENDS ${RESORA_ASSETS}/app/resora.ico)
endif()
qt_add_executable(ImageResolutionAnalyzer WIN32 main/main.cpp ${RESORA_RC})
target_link_libraries(ImageResolutionAnalyzer PRIVATE imgan_core)

enable_testing()
add_subdirectory(tests)
