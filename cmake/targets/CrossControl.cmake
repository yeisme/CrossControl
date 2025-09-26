if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})

  # 仅对项目源生成 QM，避免 autogen 循环
  qt_create_translation(QM_FILES ${TS_FILES} ${PROJECT_SOURCES})

  if(QM_FILES)
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(CrossControl translations)
  endif()
else()
  if(ANDROID)
    add_library(CrossControl SHARED ${PROJECT_SOURCES})
  else()
    add_executable(CrossControl ${PROJECT_SOURCES})
  endif()

  qt5_create_translation(QM_FILES ${PROJECT_SOURCES} ${TS_FILES})

  if(QM_FILES)
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(CrossControl translations)
  endif()
endif()

target_link_libraries(
  CrossControl
  PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql logging)
target_include_directories(
  CrossControl
  PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/widgets
          ${CMAKE_SOURCE_DIR}/include/logging)

set_target_properties(CrossControl PROPERTIES WIN32_EXECUTABLE TRUE)

add_custom_command(
  TARGET CrossControl
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:CrossControl>/i18n"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES}
          "$<TARGET_FILE_DIR:CrossControl>/i18n"
  COMMENT "Copying translation files to runtime i18n directory")

install(
  TARGETS CrossControl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)
