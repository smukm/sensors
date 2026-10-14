cmake_minimum_required(VERSION 3.19)
project(sensors LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

# Явно компилируем ресурсы
qt_add_resources(RESOURCES resources.qrc)

qt_add_executable(sensors
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ${RESOURCES}
)

qt_add_translations(
    TARGETS sensors
    TS_FILES sensors_ru_RU.ts
)

target_link_libraries(sensors
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS sensors
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET sensors
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
