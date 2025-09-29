set(TS_FILES src/i18n/CrossControl_zh_CN.ts)
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
    src/widgets/logwidget.cpp
    include/widgets/logwidget.h
    src/widgets/logwidget.ui
    src/widgets/facerecognitionwidget.cpp
    include/widgets/facerecognitionwidget.h
    src/widgets/unlockwidget.cpp
    include/widgets/unlockwidget.h
    src/widgets/unlockwidget.ui
    src/widgets/weatherwidget.cpp
    include/widgets/weatherwidget.h
    src/widgets/weatherwidget.ui
    resources/icons.qrc
    include/logging/logging.h
    ${TS_FILES})

qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})

# 仅使用 Qt6 API 生成翻译文件
qt_create_translation(QM_FILES ${TS_FILES} ${PROJECT_SOURCES})

if(QM_FILES)
  add_custom_target(translations ALL DEPENDS ${QM_FILES})
  add_dependencies(CrossControl translations)
endif()

# 确认 Qt6::Widgets 可用（Dependencies.cmake 应负责查找 Qt）
if(NOT TARGET Qt6::Widgets)
  message(
    FATAL_ERROR
      "Qt6::Widgets target not found. Ensure cmake/Dependencies.cmake found Qt6."
  )
endif()

# Core Qt link libraries - ensure targets exist before linking
set(_core_qt_libs)
if(TARGET Qt6::Widgets)
  list(APPEND _core_qt_libs Qt6::Widgets)
endif()
if(TARGET Qt6::Network)
  list(APPEND _core_qt_libs Qt6::Network)
endif()
if(TARGET Qt6::Sql)
  list(APPEND _core_qt_libs Qt6::Sql)
endif()

target_link_libraries(CrossControl PRIVATE ${_core_qt_libs} logging config)

# Qt Multimedia 为摄像头 UI 所需
if(NOT TARGET Qt6::Multimedia)
  message(
    FATAL_ERROR
      "Qt6::Multimedia target not found. Multimedia support is required for the FaceRecognition UI. Ensure cmake/Dependencies.cmake configures Qt Multimedia or set CMAKE_PREFIX_PATH to your Qt6 installation that includes Multimedia."
  )
endif()
target_link_libraries(CrossControl PRIVATE Qt6::Multimedia)
target_include_directories(
  CrossControl
  PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/widgets
          ${CMAKE_SOURCE_DIR}/include/logging)

# 统一输出目录，便于打包
cc_set_output_dirs(CrossControl)

add_custom_command(
  TARGET CrossControl
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:CrossControl>/i18n"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES}
          "$<TARGET_FILE_DIR:CrossControl>/i18n"
  COMMENT "Copying translation files to runtime i18n directory")

# Install CrossControl under the Core component
cc_install_target(CrossControl Core)

# Ensure generated translation files are installed into the Core component so
# they are included in CPack packages (NSIS/7Z). QM_FILES is produced by
# qt_create_translation and refers to generated .qm files in the build tree.
if(QM_FILES)
  install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}/i18n" COMPONENT Core)
endif()

# Optionally deploy Qt runtime when ENABLE_DEPLOY_QT is ON. Support both
# Windows (windeployqt) and Linux (linuxdeployqt) for developer workflows.
if(ENABLE_DEPLOY_QT AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  # Add automatic deploy step to copy Qt runtime and plugins into the runtime dir
  cc_deploy_qt_runtime(CrossControl)
endif()
