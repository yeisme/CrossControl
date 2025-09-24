# Targets
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

# Module libraries
if(BUILD_HUMAN_RECOGNITION)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                                 Qt${QT_VERSION_MAJOR}::Network)
  target_link_libraries(HumanRecognition PRIVATE spdlog::spdlog fmt::fmt)
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
  target_link_libraries(
    AudioVideo
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::Multimedia)
  target_link_libraries(AudioVideo PRIVATE spdlog::spdlog fmt::fmt)
  target_include_directories(AudioVideo PRIVATE include/modules/AudioVideo)
  target_link_libraries(CrossControl PRIVATE AudioVideo)
endif()

target_link_libraries(CrossControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                           Qt${QT_VERSION_MAJOR}::Network
                                           spdlog::spdlog fmt::fmt)
target_include_directories(CrossControl PRIVATE include/widgets)

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
