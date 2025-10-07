# 项目配置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DLIB build/configuration options moved here so they're available early
# before Dependencies.cmake is processed. These mirror the options that
# the Dependencies file expects to be set by the project.
option(DLIB_ENABLE_CUDA "Enable CUDA support when building dlib" OFF)
option(DLIB_ENABLE_FFMPEG "Enable FFmpeg support in dlib" ON)
option(DLIB_ENABLE_JPEG "Enable JPEG support in dlib" ON)
option(DLIB_ENABLE_PNG "Enable PNG support in dlib" ON)
option(DLIB_ENABLE_BLAS "Enable BLAS/LAPACK support in dlib" ON)
option(DLIB_ENABLE_ISO_CPP_ONLY
  "Build dlib in ISO C++ only mode (disables some wrappers)" OFF)

# Optional string to set CUDA architectures explicitly (e.g. "75;80").
# Leave empty for auto/native.
set(DLIB_CUDA_ARCHITECTURES
    ""
    CACHE
      STRING
      "Semicolon-separated list of CUDA architectures for CMAKE_CUDA_ARCHITECTURES (set in ProjectConfig)"
)

# 抑制开发者警告以避免 FFmpeg 包名不匹配的提示
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "" FORCE)

# 模块可扩展性选项
# HumanRecognition and AudioVideo are required and installed as part of Core.
# These modules are always built and not exposed as configuration options.
option(BUILD_MQTT_CLIENT "Build MQTT Client module" OFF) # TODO: 默认关闭，暂时不支持
if(BUILD_MQTT_CLIENT)
  message(STATUS " * feature BUILD_MQTT_CLIENT: ON - MQTT gateway/client support will be compiled. Ensure Paho MQTT (paho-mqttpp3) is available through vcpkg or system packages.")
else()
  message(STATUS " * feature BUILD_MQTT_CLIENT: OFF - MQTT support disabled. Enable with -DBUILD_MQTT_CLIENT=ON to build MQTT client features.")
endif()

# Storage module is required and always built; not exposed as a configuration option.
option(BUILD_SHARED_MODULES "Build modules as shared libraries" ON)
if(BUILD_SHARED_MODULES)
  message(STATUS " * feature BUILD_SHARED_MODULES: ON - Modules will be built as shared libraries (recommended for plugin-style modules).")
else()
  message(STATUS " * feature BUILD_SHARED_MODULES: OFF - Modules will be linked statically into the main executable.")
endif()

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
