# Tests are grouped by area in subfolders (backend/, resora/, features/, dashboard/, ui/, tools/).
# Executable and ctest names are unchanged (tst_<name>), only the source location differs.

# GUI test: links the full app library + QtTest.
function(imgan_add_test dir name)
    qt_add_executable(${name} ${dir}/${name}.cpp)
    target_link_libraries(${name} PRIVATE imgan_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # Offscreen so widget tests run without a visible desktop.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# Container library test: links only the Qt-Core-only library (proves no widgets dependency).
function(resora_add_test dir name)
    qt_add_executable(${name} ${dir}/${name}.cpp)
    target_link_libraries(${name} PRIVATE resora_format Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# backend/ - process + protocol
imgan_add_test(backend tst_protocolparser)
imgan_add_test(backend tst_backendprocess)

# ui/ - widgets, dialogs, locations, icons, main window
imgan_add_test(ui tst_widgets)
imgan_add_test(ui tst_locations)
imgan_add_test(ui tst_icons)
imgan_add_test(ui tst_dialogs)
imgan_add_test(ui tst_mainwindow)

# features/ - properties, analytics, comparison, insights, duplicates
imgan_add_test(features tst_properties)
imgan_add_test(features tst_analytics)
imgan_add_test(features tst_comparison)
imgan_add_test(features tst_insights)
imgan_add_test(features tst_duplicates)

# dashboard/ - dashboard UI and state (compatibility fixtures in dashboard/fixtures)
imgan_add_test(dashboard tst_dashboard)
imgan_add_test(dashboard tst_dashboardstate)

# resora/ - .resora container, export/import, reliability
imgan_add_test(resora tst_resoraui)
resora_add_test(resora tst_resorafile)
resora_add_test(resora tst_bundledexport)
resora_add_test(resora tst_linkedexport)
resora_add_test(resora tst_resoraimport)
resora_add_test(resora tst_resorareliability)
resora_add_test(resora tst_exportlowdisk)

# tools/ - developer tool: renders windows/dialogs to PNG on the real platform plugin (not run by ctest).
qt_add_executable(grab_screens tools/grab_screens.cpp)
target_link_libraries(grab_screens PRIVATE imgan_core)
