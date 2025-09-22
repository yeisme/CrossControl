cmake_minimum_required(VERSION 3.16)

project(
  CrossControl
  VERSION 0.1
  LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES CrossControl_yue_CN.ts)

include_directories(include)

set(PROJECT_SOURCES
    src/main.cpp
    src/widgets/crosscontrolwidget.cpp
    include/widgets/crosscontrolwidget.h
    src/widgets/crosscontrolwidget.ui
    src/widgets/loginwidget.cpp
    include/widgets/loginwidget.h
    src/widgets/loginwidget.ui
    src/widgets/mainwidget.cpp
    include/widgets/mainwidget.h
    src/widgets/mainwidget.ui
    src/widgets/monitorwidget.cpp
    include/widgets/monitorwidget.h
    src/widgets/monitorwidget.ui
    src/widgets/visitrecordwidget.cpp
    include/widgets/visitrecordwidget.h
    src/widgets/visitrecordwidget.ui
    src/widgets/messagewidget.cpp
    include/widgets/messagewidget.h
    src/widgets/messagewidget.ui
    src/widgets/settingwidget.cpp
    include/widgets/settingwidget.h
    src/widgets/settingwidget.ui
    src/widgets/unlockwidget.cpp
    include/widgets/unlockwidget.h
    src/widgets/unlockwidget.ui
    ${TS_FILES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})
  # Define target properties for Android with Qt 6 as: set_property(TARGET
  # CrossControl APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
  # ${CMAKE_CURRENT_SOURCE_DIR}/android) For more information, see
  # https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

  qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
  if(ANDROID)
    add_library(CrossControl SHARED ${PROJECT_SOURCES})
    # Define properties for Android with Qt 5 after find_package() calls as:
    # set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
  else()
    add_executable(CrossControl ${PROJECT_SOURCES})
  endif()

  qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(CrossControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_include_directories(CrossControl PRIVATE include/widgets)
target_include_directories(CrossControl PRIVATE include)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1. If
# you are developing for iOS or macOS you should consider setting an explicit,
# fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.CrossControl)
endif()
set_target_properties(
  CrossControl
  PROPERTIES ${BUNDLE_ID_OPTION} MACOSX_BUNDLE_BUNDLE_VERSION
             ${PROJECT_VERSION} MACOSX_BUNDLE_SHORT_VERSION_STRING
             ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} MACOSX_BUNDLE
             TRUE WIN32_EXECUTABLE
             TRUE)

include(GNUInstallDirs)
install(
  TARGETS CrossControl
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(CrossControl)
endif()
