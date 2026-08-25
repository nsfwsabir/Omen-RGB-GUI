cmake_minimum_required(VERSION 3.16)
project(omen-rgb VERSION 1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# Optional: Qt6 Charts not required - we use custom FanCurveWidget
# find_package(Qt6 COMPONENTS Charts QUIET)

set(SOURCES
    src/main.cpp
    src/core/SysFs.cpp
    src/core/OmenDevice.cpp
    src/core/RgbController.cpp
    src/core/AnimationController.cpp
    src/core/FanController.cpp
    src/core/MuteLedController.cpp
    src/core/DkmsInstaller.cpp
    src/ui/MainWindow.cpp
    src/ui/ColorTab.cpp
    src/ui/AnimationTab.cpp
    src/ui/FanTab.cpp
    src/ui/SettingsTab.cpp
    src/ui/GradientEditorDialog.cpp
    src/ui/FanCurveWidget.cpp
)

set(HEADERS
    src/core/SysFs.h
    src/core/OmenDevice.h
    src/core/RgbController.h
    src/core/AnimationController.h
    src/core/FanController.h
    src/core/MuteLedController.h
    src/core/DkmsInstaller.h
    src/ui/MainWindow.h
    src/ui/ColorTab.h
    src/ui/AnimationTab.h
    src/ui/FanTab.h
    src/ui/SettingsTab.h
    src/ui/GradientEditorDialog.h
    src/ui/FanCurveWidget.h
)

qt_add_executable(omen-rgb ${SOURCES} ${HEADERS} resources.qrc)

target_include_directories(omen-rgb PRIVATE src)

target_link_libraries(omen-rgb PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)

# Install
install(TARGETS omen-rgb DESTINATION bin)
install(FILES resources/omen-rgb.desktop DESTINATION share/applications)
install(FILES resources/icons/omen-rgb.svg DESTINATION share/icons/hicolor/scalable/apps)
# Polkit policy must go to /usr/share/polkit-1/actions (not /usr/local/share) for polkit to find it
# Use absolute destination to ensure correct path regardless of CMAKE_INSTALL_PREFIX
install(FILES resources/io.omenrgb.policy DESTINATION /usr/share/polkit-1/actions)
# Also install to relative share for packaging compatibility
install(FILES resources/io.omenrgb.policy DESTINATION share/polkit-1/actions)
# Udev rules for password-less control via input group (fixes repeated pkexec prompts)
install(FILES resources/99-omen-rgb-keyboard.rules DESTINATION /etc/udev/rules.d)
install(FILES resources/99-omen-rgb-keyboard.rules DESTINATION lib/udev/rules.d)

# For local dev without polkit file, also handle manually
