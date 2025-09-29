# 项目配置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 抑制开发者警告以避免 FFmpeg 包名不匹配的提示
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "" FORCE)

# 模块可扩展性选项
# HumanRecognition and AudioVideo are required and installed as part of Core.
# These modules are always built and not exposed as configuration options.
option(BUILD_MQTT_CLIENT "Build MQTT Client module" OFF) # TODO: 默认关闭，暂时不支持
message(STATUS " * feature BUILD_MQTT_CLIENT: ${BUILD_MQTT_CLIENT}")

# Storage module is required and always built; not exposed as a configuration option.
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
