# 项目配置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 抑制开发者警告以避免 FFmpeg 包名不匹配的提示
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "" FORCE)

# 模块可扩展性选项
option(BUILD_HUMAN_RECOGNITION "Build Human Recognition module" ON)
message(STATUS " * feature BUILD_HUMAN_RECOGNITION: ${BUILD_HUMAN_RECOGNITION}")
option(BUILD_AUDIO_VIDEO "Build Audio Video module" ON)
message(STATUS " * feature BUILD_AUDIO_VIDEO: ${BUILD_AUDIO_VIDEO}")
option(BUILD_MQTT_CLIENT "Build MQTT Client module" OFF) # TODO: 默认关闭，暂时不支持
message(STATUS " * feature BUILD_MQTT_CLIENT: ${BUILD_MQTT_CLIENT}")

# Storage 模块为项目必需；始终启用。
set(BUILD_STORAGE
    ON
    CACHE BOOL "Build Storage (data storage) module" FORCE)
message(STATUS " * feature BUILD_STORAGE: ON (required)")
option(BUILD_SHARED_MODULES "Build modules as shared libraries" ON)
message(STATUS " * feature BUILD_SHARED_MODULES: ${BUILD_SHARED_MODULES}")

# 启用编译缓存以加速重编译
if(NOT MSVC)
  find_program(SCCACHE_PROGRAM sccache)

  # Optional build accelerator (speeds up rebuilds even if triggered)
  if(NOT SCCACHE_PROGRAM)
    message(STATUS "sccache not found")
  else()
    find_program(CCACHE_PROGRAM ccache)

    if(CCACHE_PROGRAM)
      message(STATUS "Using ccache with sccache")
      set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILER
                                   "${CCACHE_PROGRAM};${SCCACHE_PROGRAM}")
    endif()
  endif()
  message(STATUS "sccache found: ${SCCACHE_PROGRAM}, using it for C/C++ compilation")
endif()
