cmake_minimum_required(VERSION 3.16)

project(Project2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Core
    SerialPort
)
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appProject2
    main.cpp
)

qt_add_qml_module(appProject2
    URI Project2
    VERSION 1.0

    QML_FILES
        Main.qml
        CarMainView.qml
        ui/BottomBar/BottomBar.qml
        ui/RightScreen/RightScreen.qml
        ui/LeftScreen/LeftScreen.qml
        ui/RightScreen/NavigationSearchBox.qml

    SOURCES
        Controllers/controller.h
        Controllers/controller.cpp

        Models/model.h
        Models/model.cpp

        Services/services.h
        Services/services.cpp

        ../../can_messages_shared/can_messages.h

    RESOURCES
        ui/assets/carRender.png
        ui/assets/carIcon.png
        ui/assets/lock.png
        ui/assets/unlock.png
        ui/assets/search.png
        SOURCES Models/dashboardmodel.h Models/dashboardmodel.cpp
        QML_FILES ui/BottomBar/HVACComponenet.qml
        RESOURCES
        RESOURCES
        QML_FILES ui/BottomBar/VolumeControlComponent.qml
        RESOURCES ui/assets/0-volume-white.png ui/assets/1-volume-white.png ui/assets/2-volume-white.png ui/assets/3-volume-white.png
        QML_FILES ui/LeftScreen/SpeedComponent.qml
        QML_FILES ui/BottomBar/InfoComponenet.qml
        RESOURCES ui/assets/abs-off.png ui/assets/abs-on.png ui/assets/fuel.png ui/assets/fuel1.png ui/assets/hand_warning_light.png ui/assets/hand_warning2.png ui/assets/highbeam-off.png ui/assets/highbeam-on.png ui/assets/lowbeam-off.png ui/assets/lowbeam-on.png ui/assets/turn-left-off.png ui/assets/turn-left-on.png ui/assets/turn-right-off.png ui/assets/turn-right-on.png ui/assets/wiper-off.png ui/assets/wiper-on.png
        QML_FILES ui/LeftScreen/GearPosition.qml
        RESOURCES ui/assets/reverse-close.png ui/assets/reverse-far.png ui/assets/reverse-med.png ui/assets/reverse-off.png ui/assets/trunk-close.png ui/assets/trunk-open.png
        QML_FILES ui/LeftScreen/VehicleStatus.qml
        RESOURCES ui/assets/mil-off.png ui/assets/mil-on.png
)

target_include_directories(appProject2 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../can_messages_shared"
)

set_target_properties(appProject2 PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appProject2 PRIVATE
    Qt6::Quick
    Qt6::Core
    Qt6::SerialPort
)
target_link_libraries(appProject2 PRIVATE Qt6::Core)

include(GNUInstallDirs)

install(TARGETS appProject2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
