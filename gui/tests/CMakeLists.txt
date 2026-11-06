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
imgan_add_test(tst_comparison)
imgan_add_test(tst_duplicates)

# Developer tool: renders windows/dialogs to PNG on the real platform plugin (not run by ctest).
qt_add_executable(grab_screens grab_screens.cpp)
target_link_libraries(grab_screens PRIVATE imgan_core)
