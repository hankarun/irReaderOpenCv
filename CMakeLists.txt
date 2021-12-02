cmake_minimum_required(VERSION 3.5)

include(.env.cmake OPTIONAL)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${OPENCV_DIR})
include(InstallRequiredSystemLibraries)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

project(irDataReader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(OpenCV REQUIRED)

get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(irDataReader
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET irDataReader APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(irDataReader SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(irDataReader WIN32
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(irDataReader PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(irDataReader PRIVATE ${OpenCV_LIBS})
include_directories( ${OpenCV_INCLUDE_DIRS} )

set_target_properties(irDataReader PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(irDataReader)
endif()


install(TARGETS irDataReader 
	EXPORT irDataReader-targets
	RUNTIME DESTINATION .
	ARCHIVE DESTINATION ..
	LIBRARY DESTINATION ..
	COMPONENT irDataReader_installer)

set(CPACK_PACKAGE_NAME "IRDataAnalyser")
set(CPACK_PACKAGE_VENDOR "Simsoft")
set(CPACK_VERBATIM_VARIABLES true)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 2)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_GENERATOR "ZIP" CACHE STRING "Generators to support. semi-colon delimited list")
include(CPack)