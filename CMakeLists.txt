cmake_minimum_required(VERSION 3.16)

project(mklipper VERSION 1.8.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    DBus
    Network
    Sql
    Svg
)

find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
pkg_check_modules(XTST REQUIRED IMPORTED_TARGET xtst)

set(MKLIPPER_CORE_SOURCES
    src/settings.cpp
    src/settings.h
    src/translations.cpp
    src/translations.h
    src/textutils.cpp
    src/textutils.h
    src/mimetypes.h
    src/historyitem.cpp
    src/historyitem.h
    src/sourceinfo.cpp
    src/sourceinfo.h
    src/systemclipboard.cpp
    src/systemclipboard.h
    src/historymodel.cpp
    src/historymodel.h
    src/historycycler.cpp
    src/historycycler.h
    src/clipcommandprocess.cpp
    src/clipcommandprocess.h
    src/urlgrabber.cpp
    src/urlgrabber.h
    src/x11keyboard.cpp
    src/x11keyboard.h
    src/globalshortcut.cpp
    src/globalshortcut.h
    src/shortcutrecorder.cpp
    src/shortcutrecorder.h
    src/x11paster.cpp
    src/x11paster.h
    src/notifications.cpp
    src/notifications.h
    src/flaticons.cpp
    src/flaticons.h
    src/themestyle.cpp
    src/themestyle.h
    src/qrcodewidget.cpp
    src/qrcodewidget.h
    src/previewwidget.cpp
    src/previewwidget.h
    src/klipperpopup.cpp
    src/klipperpopup.h
    src/editactiondialog.cpp
    src/editactiondialog.h
    src/configdialog.cpp
    src/configdialog.h
    src/helpdialog.cpp
    src/helpdialog.h
    src/updater.cpp
    src/updater.h
    src/updatedialog.cpp
    src/updatedialog.h
    src/klipperapp.cpp
    src/klipperapp.h
    resources.qrc
)

add_library(mklipper_core OBJECT ${MKLIPPER_CORE_SOURCES})

target_include_directories(mklipper_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(MKLIPPER_REPO_SLUG "Castellanoi/Mklipper" CACHE STRING "GitHub owner/repo slug used by the in-app updater")

target_compile_definitions(mklipper_core PUBLIC
    MKLIPPER_VERSION="${PROJECT_VERSION}"
    MKLIPPER_REPO_SLUG="${MKLIPPER_REPO_SLUG}"
)

target_link_libraries(mklipper_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
    Qt6::Sql
    Qt6::Svg
    X11::X11
    PkgConfig::XTST
    PkgConfig::QRENCODE
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mklipper_core PUBLIC
        -Wall
        -Wextra
        -Wpedantic
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -fstack-protector-strong
        -Wformat
        -Wformat-security
        -fPIE
        $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
    )
endif()

add_executable(mklipper src/main.cpp)
target_link_libraries(mklipper PRIVATE mklipper_core)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(mklipper PRIVATE
        -pie
        -Wl,-z,relro
        -Wl,-z,now
    )
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(mklipper_tests tests/test_mklipper.cpp)
    target_link_libraries(mklipper_tests PRIVATE mklipper_core)
    target_compile_definitions(mklipper_tests PRIVATE
        MKLIPPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME mklipper_unit_tests COMMAND mklipper_tests)
    set_tests_properties(mklipper_unit_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_FOUND)
        add_test(NAME mklipper_i18n_validation
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_i18n.py ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS mklipper
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES data/mklipper.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
install(FILES data/mklipper-autostart.desktop
    DESTINATION /etc/xdg/autostart
    RENAME mklipper.desktop
)
install(FILES data/icons/mklipper.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)
