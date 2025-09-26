# Targets
include(cmake/targets/CrossControl.cmake)

# logging 单独放到文件中，便于管理
include(cmake/targets/Logging.cmake)

# 模块独立为单文件，包含它们的配置/链接/头文件等
include(cmake/targets/HumanRecognition.cmake)
include(cmake/targets/AudioVideo.cmake)
include(cmake/targets/Storage.cmake)
include(cmake/targets/Config.cmake)
include(cmake/targets/Connect.cmake)

if(NOT TARGET Qt6::Widgets)
  message(FATAL_ERROR "Qt6::Widgets target not found. Ensure cmake/Dependencies.cmake found Qt6.")
endif()
target_link_libraries(
        CrossControl PRIVATE Qt6::Widgets Qt6::Network logging)
target_include_directories(CrossControl PRIVATE include/widgets include/logging)

# 输出/MSVC 专用设置放到单独文件中，便于管理
include(cmake/Output.cmake)

# Keep Windows subsystem flag for GUI app
set_target_properties(CrossControl PROPERTIES WIN32_EXECUTABLE TRUE)

# Copy translation QM files next to executable under i18n/ for runtime lookup
add_custom_command(
        TARGET CrossControl
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:CrossControl>/i18n"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES}
        "$<TARGET_FILE_DIR:CrossControl>/i18n"
        COMMENT "Copying translation files to runtime i18n directory")

# Copy default CrossControlConfig.json next to executable so ConfigManager can auto-load it
add_custom_command(
        TARGET CrossControl
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/CrossControlConfig.json
                "$<TARGET_FILE_DIR:CrossControl>/CrossControlConfig.json"
        COMMENT "Copy default CrossControlConfig.json to runtime directory")
