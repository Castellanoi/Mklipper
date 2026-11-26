cmake_minimum_required(VERSION 3.16)

project(mklipper VERSION 1.6.2 LANGUAGES CXX)

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

set(MKLIPPER_SOURCES
    src/main.cpp
    src/settings.cpp
    src/settings.h
    src/translations.cpp
    src/translations.h
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
    src/globalshortcut.cpp
    src/globalshortcut.h
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
    src/klipperpopup.cpp
    src/klipperpopup.h
    src/editactiondialog.cpp
    src/editactiondialog.h
    src/configdialog.cpp
    src/configdialog.h
    src/updater.cpp
    src/updater.h
    src/updatedialog.cpp
    src/updatedialog.h
    src/klipperapp.cpp
    src/klipperapp.h
    resources.qrc
)

add_executable(mklipper ${MKLIPPER_SOURCES})

target_compile_definitions(mklipper PRIVATE
    MKLIPPER_VERSION="${PROJECT_VERSION}"
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mklipper PRIVATE
        -fstack-protector-strong
        -Wformat
        -Wformat-security
        -fPIE
        $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
    )
    target_link_options(mklipper PRIVATE
        -pie
        -Wl,-z,relro
        -Wl,-z,now
    )
endif()

target_link_libraries(mklipper PRIVATE
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
