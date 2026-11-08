function(imgan_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE imgan_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # Offscreen so widget tests run without a visible desktop.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

imgan_add_test(tst_protocolparser)
imgan_add_test(tst_backendprocess)
imgan_add_test(tst_widgets)
imgan_add_test(tst_locations)
imgan_add_test(tst_icons)
imgan_add_test(tst_dialogs)
imgan_add_test(tst_properties)
imgan_add_test(tst_analytics)
imgan_add_test(tst_dashboard)
imgan_add_test(tst_comparison)
imgan_add_test(tst_insights)
imgan_add_test(tst_duplicates)

# Container library test: links only the Qt-Core-only library (proves no widgets dependency).
qt_add_executable(tst_resorafile tst_resorafile.cpp)
target_link_libraries(tst_resorafile PRIVATE resora_format Qt6::Test)
add_test(NAME tst_resorafile COMMAND tst_resorafile)

qt_add_executable(tst_bundledexport tst_bundledexport.cpp)
target_link_libraries(tst_bundledexport PRIVATE resora_format Qt6::Test)
add_test(NAME tst_bundledexport COMMAND tst_bundledexport)

qt_add_executable(tst_linkedexport tst_linkedexport.cpp)
target_link_libraries(tst_linkedexport PRIVATE resora_format Qt6::Test)
add_test(NAME tst_linkedexport COMMAND tst_linkedexport)

qt_add_executable(tst_resoraimport tst_resoraimport.cpp)
target_link_libraries(tst_resoraimport PRIVATE resora_format Qt6::Test)
add_test(NAME tst_resoraimport COMMAND tst_resoraimport)

# Developer tool: renders windows/dialogs to PNG on the real platform plugin (not run by ctest).
qt_add_executable(grab_screens grab_screens.cpp)
target_link_libraries(grab_screens PRIVATE imgan_core)
