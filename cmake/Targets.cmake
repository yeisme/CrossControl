# Targets
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})
  # Generate QM files from TS, scanning only actual project sources to avoid
  # autogen cycles
  qt_create_translation(QM_FILES ${TS_FILES} ${PROJECT_SOURCES})
  # Make sure translation files are generated when building 'all'
  if(QM_FILES)
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    # Ensure the executable build waits for translations before running
    # POST_BUILD copy
    add_dependencies(CrossControl translations)
  endif()
else()
  if(ANDROID)
    add_library(CrossControl SHARED ${PROJECT_SOURCES})
  else()
    add_executable(CrossControl ${PROJECT_SOURCES})
  endif()
  # Qt5 translation generation with only project sources
  qt5_create_translation(QM_FILES ${PROJECT_SOURCES} ${TS_FILES})
  if(QM_FILES)
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(CrossControl translations)
  endif()
endif()

# global logging library
add_library(logging STATIC src/logging/logging.cpp)
target_include_directories(logging PUBLIC include/logging)
target_link_libraries(logging PUBLIC spdlog::spdlog fmt::fmt
                                     Qt${QT_VERSION_MAJOR}::Widgets)

# Module libraries
if(BUILD_HUMAN_RECOGNITION)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                                 Qt${QT_VERSION_MAJOR}::Network)
  target_link_libraries(HumanRecognition PRIVATE logging)
  target_include_directories(HumanRecognition
                             PRIVATE include/modules/HumanRecognition)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)
endif()

if(BUILD_AUDIO_VIDEO)
  if(BUILD_SHARED_MODULES)
    add_library(AudioVideo SHARED ${AUDIO_VIDEO_SOURCES})
  else()
    add_library(AudioVideo STATIC ${AUDIO_VIDEO_SOURCES})
  endif()
  # 移除对 QtMultimedia 的依赖，使用 FFmpeg 进行音视频处理
  target_link_libraries(AudioVideo PRIVATE logging)
  target_include_directories(AudioVideo PRIVATE include/modules/AudioVideo)
  target_link_libraries(CrossControl PRIVATE AudioVideo)
endif()

target_link_libraries(
  CrossControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                       Qt${QT_VERSION_MAJOR}::Network logging)
target_include_directories(CrossControl PRIVATE include/widgets include/logging)

# 确保可执行文件和共享库放置在特定于配置的 bin 目录中，以便在从 Visual Studio 启动时 EXE 可以找到 DLL。
# 对多配置生成器（Visual Studio）使用生成器表达式 $<CONFIG>。
if(CMAKE_CONFIGURATION_TYPES)
  set(CONFIG_BIN_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/bin")
  set(CONFIG_LIB_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/lib")
else()
  # single-config generators (Unix Makefiles, Ninja single-config)
  set(CONFIG_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
  set(CONFIG_LIB_DIR "${CMAKE_BINARY_DIR}/lib")
endif()

# 应用输出目录，并且（对于MSVC）设置VS调试器工作目录， 以便从Visual Studio运行时使用包含DLL的bin文件夹。
foreach(_tgt IN ITEMS CrossControl HumanRecognition AudioVideo)
  if(TARGET ${_tgt})
    set_target_properties(
      ${_tgt}
      PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CONFIG_BIN_DIR}" # 运行时（可执行文件和DLL）
                 LIBRARY_OUTPUT_DIRECTORY "${CONFIG_BIN_DIR}" # 共享库（.so/.dylib）
                 ARCHIVE_OUTPUT_DIRECTORY "${CONFIG_LIB_DIR}" # 静态库 (.lib/.a)
    )
    if(MSVC)
      # Visual Studio will substitute $<CONFIG> in the property at generation
      # time
      set_target_properties(${_tgt} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
                                               "${CONFIG_BIN_DIR}")
    endif()
  endif()
endforeach()

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
