set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(COMPANY "Jose Vicente Campos")
set(PROGRAM_NAME "hce-lab")
set(ICON_FILE_PATH "../assets/app/rc/hce-laboratory.ico")
set(APP_TARGET_DIR "@HomeDir@/.${PROGRAM_NAME}")

set(PRIVATE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

if (PROJECT_VERSION_MAJOR GREATER 0)
    set(ENABLE_CONSOLE_OUTPUT OFF)
else ()
    set(ENABLE_CONSOLE_OUTPUT ON)
endif ()

find_package(Qt6 COMPONENTS Core Widgets Network PrintSupport REQUIRED)

string(TIMESTAMP YEAR "%Y")

configure_file(${PRIVATE_SOURCE_DIR}/main.rc.in ${PRIVATE_SOURCE_DIR}/main.rc)
configure_file(${PRIVATE_SOURCE_DIR}/QtConfig.h.in ${PRIVATE_SOURCE_DIR}/QtConfig.h)

set(SOURCES
        src/main/cpp/main.cpp
        src/main/cpp/QtApplication.cpp
        src/main/cpp/QtControl.cpp
        src/main/cpp/QtWindow.cpp
        src/main/cpp/dialogs/ConfigDialog.cpp
        src/main/cpp/events/ConsoleLogEvent.cpp
        src/main/cpp/events/ListenerControlEvent.cpp
        src/main/cpp/events/ListenerFrameEvent.cpp
        src/main/cpp/events/ListenerStatusEvent.cpp
        src/main/cpp/events/SystemShutdownEvent.cpp
        src/main/cpp/events/SystemStartupEvent.cpp
        src/main/cpp/format/DataFormat.cpp
        src/main/cpp/model/ParserModel.cpp
        src/main/cpp/model/StreamFilter.cpp
        src/main/cpp/model/StreamModel.cpp
        src/main/cpp/protocol/ProtocolFrame.cpp
        src/main/cpp/protocol/ProtocolParser.cpp
        src/main/cpp/styles/IconStyle.cpp
        src/main/cpp/styles/Theme.cpp
        src/main/cpp/widgets/HexViewWidget.cpp
        src/main/cpp/widgets/IconDelegate.cpp
        src/main/cpp/widgets/ParserDelegate.cpp
        src/main/cpp/widgets/ParserWidget.cpp
        src/main/cpp/widgets/StreamDelegate.cpp
        src/main/cpp/widgets/StreamHeader.cpp
        src/main/cpp/widgets/StreamMenu.cpp
        src/main/cpp/widgets/StreamWidget.cpp
        src/main/cpp/3party/customplot/QCustomPlot.cpp
        src/main/assets/theme/dark/icons/icons.qrc
        src/main/assets/theme/dark/style/style.qrc
        src/main/assets/theme/light/icons/icons.qrc
        src/main/assets/theme/light/style/style.qrc
        src/main/assets/app/app.qrc
)

# Build for WIN32 / UNIX-LINUX
if (WIN32)

    set(PLATFORM_LIBS mingw32 psapi dwmapi)

    if (ENABLE_CONSOLE_OUTPUT)
        message(STATUS "Enable debug output to console.")
        add_executable(hce-lab ${SOURCES})
        add_compile_definitions(ENABLE_CONSOLE_LOGGING)
    else ()
        add_executable(hce-lab WIN32 ${SOURCES})
    endif ()

elseif (UNIX)
    add_executable(hce-lab ${SOURCES})
endif ()

add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x050F00)

target_include_directories(hce-lab PRIVATE ${PRIVATE_SOURCE_DIR})
target_include_directories(hce-lab PRIVATE ${AUTOGEN_BUILD_DIR}/include)
target_include_directories(hce-lab PRIVATE ${LIBUSB_INCLUDE_DIR})

target_link_libraries(hce-lab
        ${PLATFORM_LIBS}
        hce-core
        hce-tasks
        hce-targets
        hw-dev
        rt-lang
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::PrintSupport)

#-------------------------------------------------------------------------------
# configure files
#-------------------------------------------------------------------------------

if (WIN32)

    # generate installer scripts
    configure_file(
            ${CMAKE_SOURCE_DIR}/dat/installer/script.nsi.in
            ${CMAKE_BINARY_DIR}/installer/meta/script.nsi)

endif (WIN32)

# Install targets
if (UNIX)

    # Option to use Flatpak naming convention
    install(TARGETS hce-lab DESTINATION bin)

    set(ICON_NAME "hce-lab")
    set(DESKTOP_FILE_NAME "hce-lab.desktop")
    set(ICON_PNG_NAME "hce-lab.png")
    set(ICON_SVG_NAME "hce-lab.svg")

    # Configure desktop file with correct icon name
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/src/main/assets/app/rc/hce-lab.desktop
            ${CMAKE_CURRENT_BINARY_DIR}/hce-lab.desktop @ONLY)

    # Install configured desktop file
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hce-lab.desktop
            DESTINATION share/applications
            RENAME ${DESKTOP_FILE_NAME})

    # Install icons
    install(FILES src/main/assets/app/rc/hce-lab.png
            DESTINATION share/icons/hicolor/512x512/apps
            RENAME ${ICON_PNG_NAME})

    install(FILES src/main/assets/app/rc/hce-lab.svg
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME ${ICON_SVG_NAME})

endif (UNIX)
