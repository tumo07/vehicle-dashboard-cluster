cmake_minimum_required(VERSION 3.16)

project(Project2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appProject2
    main.cpp
    Controllers/controller.cpp
)

qt_add_qml_module(appProject2
    URI Project2
    VERSION 1.0
    QML_FILES
        Main.qml
        QML_FILES CarMainView.qml
        SOURCES
        SOURCES
        SOURCES
        SOURCES
        SOURCES
        SOURCES
        QML_FILES ui/BottomBar/BottomBar.qml
        QML_FILES ui/RightScreen/RightScreen.qml
        QML_FILES ui/LeftScreen/LeftScreen.qml
        RESOURCES ui/assets/carRender.png
        RESOURCES
        RESOURCES
        RESOURCES ui/assets/carIcon.png
        SOURCES Controllers/controller.h Controllers/controller.cpp
        SOURCES
        SOURCES Models/model.h Models/model.cpp
        RESOURCES ui/assets/lock.png ui/assets/unlock.png
        QML_FILES ui/RightScreen/NavigationSearchBox.qml
        RESOURCES ui/assets/search.png
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appProject2 PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appProject2
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appProject2
    PRIVATE Qt6::Quick
)
target_link_libraries(appProject2 PRIVATE Qt6::Core)
target_link_libraries(appProject2 PRIVATE Qt6::Core)

include(GNUInstallDirs)
install(TARGETS appProject2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
