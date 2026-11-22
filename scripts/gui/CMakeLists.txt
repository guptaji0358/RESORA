cmake_minimum_required(VERSION 3.21)
project(ImageResolutionAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Visual assets live in the repository-level scripts/Assets/ directory (see scripts/Assets/README.md).
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
    src/backend/features/analyticsmodel.cpp
    src/backend/process/backendlocator.cpp
    src/backend/process/backendprocess.cpp
    src/backend/features/comparison.cpp
    src/backend/features/duplicatesmodel.cpp
    src/widgets/duplicates/duplicatefinder.cpp
    src/widgets/duplicates/duplicatefinder_results.cpp
    src/backend/features/previewloader.cpp
    src/widgets/comparison/imagepane.cpp
    src/widgets/comparison/comparisonviewer.cpp
    src/widgets/comparison/comparisontable.cpp
    src/backend/features/insightmetrics.cpp
    src/widgets/insights/insightcanvas.cpp
    src/widgets/charts/resolutionchart.cpp
    src/widgets/charts/filesizebars.cpp
    src/widgets/insights/dimensiondiagram.cpp
    src/widgets/insights/aspectindicator.cpp
    src/widgets/comparison/metadatacompare.cpp
    src/widgets/controls/collapsiblesection.cpp
    src/widgets/insights/insightsview.cpp
    src/widgets/comparison/comparisonworkspace.cpp
    src/widgets/comparison/comparisonworkspace_ui.cpp
    src/backend/workflow/importselection.cpp
    src/backend/features/propertiesmodel.cpp
    src/backend/process/protocolparser.cpp
    src/locations/savedlocation.cpp
    src/locations/locationstore.cpp
    src/locations/locationrecommender.cpp
    src/ui/brand.cpp
    src/ui/icons.cpp
    src/ui/theme.cpp
    src/ui/uiformat.cpp
    src/widgets/dialogs/aboutdialog.cpp
    src/widgets/analytics/analyticspanel.cpp
    src/widgets/analytics/analyticsdashboard.cpp
    src/widgets/charts/chartbase.cpp
    src/widgets/charts/barchart.cpp
    src/widgets/charts/donutchart.cpp
    src/widgets/charts/hbarchart.cpp
    src/widgets/controls/segmentedswitch.cpp
    src/backend/dashboard/dashboardmodel.cpp
    src/backend/dashboard/dashboardserialize.cpp src/backend/dashboard/dashboardrestore.cpp src/backend/dashboard/dashboardsession.cpp
    src/backend/workflow/cachecleanupmanager.cpp
    src/backend/workflow/resoraworkflow.cpp src/backend/workflow/resorajobs.cpp src/backend/workflow/relinkmodel.cpp
    src/widgets/dialogs/confirmdialog.cpp src/widgets/dialogs/exportdialog.cpp src/widgets/panels/relinkpanel.cpp
    src/widgets/dialogs/openresoradialog.cpp
    src/widgets/analytics/pixelspan.cpp
    src/widgets/charts/sharechart.cpp
    src/widgets/chrome/appheader.cpp
    src/widgets/chrome/brandlockup.cpp
    src/widgets/charts/distributionbar.cpp
    src/widgets/chrome/viewfinder.cpp
    src/widgets/controls/chamferbutton.cpp
    src/widgets/controls/toggleswitch.cpp
    src/widgets/chrome/phaserail.cpp
    src/widgets/chrome/toolnav.cpp
    src/widgets/controls/elidedlabel.cpp
    src/widgets/controls/flowlayout.cpp
    src/widgets/dialogs/framelessdialog.cpp
    src/widgets/dialogs/importdialog.cpp
    src/widgets/panels/locationrow.cpp
    src/widgets/panels/locationsuggester.cpp
    src/widgets/panels/savedlocationspanel.cpp
    src/widgets/dialogs/settingsdialog.cpp
    src/widgets/controls/metriccard.cpp
    src/widgets/panels/progresspanel.cpp
    src/widgets/dialogs/propertiesdialog.cpp
    src/widgets/panels/reportselector.cpp
    src/widgets/panels/resultspanel.cpp
    src/widgets/controls/sectioncard.cpp
    src/widgets/panels/sourcepicker.cpp
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
