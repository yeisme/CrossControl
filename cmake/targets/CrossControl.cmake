qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})

# 仅使用 Qt6 API 生成翻译文件
qt_create_translation(QM_FILES ${TS_FILES} ${PROJECT_SOURCES})

if(QM_FILES)
  add_custom_target(translations ALL DEPENDS ${QM_FILES})
  add_dependencies(CrossControl translations)
endif()

if(NOT TARGET Qt6::Widgets)
  message(FATAL_ERROR "Qt6::Widgets target not found. Ensure cmake/Dependencies.cmake found Qt6.")
endif()

target_link_libraries(
  CrossControl
  PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql logging config)
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
