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
