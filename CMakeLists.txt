cmake_minimum_required(VERSION 3.15)

project(
  CrossControl
  VERSION 0.1
  LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS CoreTools Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS CoreTools Widgets LinguistTools Network)

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
    src/widgets/weatherwidget.cpp
    include/widgets/weatherwidget.h
    src/widgets/weatherwidget.ui
    ${TS_FILES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})
  qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
  if(ANDROID)
    add_library(CrossControl SHARED ${PROJECT_SOURCES})
  else()
    add_executable(CrossControl ${PROJECT_SOURCES})
  endif()
  qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(CrossControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(CrossControl PRIVATE include/widgets)
target_include_directories(CrossControl PRIVATE include)

set_target_properties(
  CrossControl
  PROPERTIES 
  WIN32_EXECUTABLE TRUE)

include(GNUInstallDirs)
install(
  TARGETS CrossControl
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT Runtime)

if(QT_VERSION_MAJOR EQUAL 6)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} --dir ${CMAKE_BINARY_DIR}/windeployqt_output \${CMAKE_INSTALL_PREFIX}/bin/CrossControl.exe)
        ")
        install(DIRECTORY ${CMAKE_BINARY_DIR}/windeployqt_output/ DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
    endif()
endif()

if(MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    install(FILES
        ${MINGW_BIN_DIR}/libc++.dll
        ${MINGW_BIN_DIR}/libunwind.dll
        ${MINGW_BIN_DIR}/libwinpthread-1.dll
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
    )
endif()

# CPack配置
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

if(WIN32)
  set(CPACK_GENERATOR "NSIS64;7Z")

  # 明确指定要打包的组件 (确保包含Runtime组件)
  set(CPACK_COMPONENTS_ALL Runtime)
endif()

# 确保系统运行时库被包含
include(InstallRequiredSystemLibraries)
include(CPack)
