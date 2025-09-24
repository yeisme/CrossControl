# Project Configuration
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress developer warnings to avoid FFmpeg package name mismatch warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "" FORCE)

# Module options for extensibility
option(BUILD_HUMAN_RECOGNITION "Build Human Recognition module" ON)
option(BUILD_AUDIO_VIDEO "Build Audio Video module" ON)
option(BUILD_MQTT_CLIENT "Build MQTT Client module" ON)
option(BUILD_STORAGE "Build Storage (data storage) module" ON)
option(BUILD_SHARED_MODULES "Build modules as shared libraries" ON)
option(BUILD_TINYORM "Build TinyORM ORM library" ON)

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
      set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILER "${CCACHE_PROGRAM};${SCCACHE_PROGRAM}")
    endif()
  endif()
endif()

# 可选的第三方模块
if(BUILD_TINYORM)
  # 优先使用外部已安装/预编译的 TinyORM（避免作为子项目时频繁受顶层改动牵连重编译）
  option(USE_SYSTEM_TINYORM
         "Prefer an installed TinyORM package over subdirectory" ON)
  set(_TINYORM_PACKAGE_OK OFF)

  if(USE_SYSTEM_TINYORM)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_BINARY_DIR}/src/modules/TinyORM")
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    find_package(TinyORM QUIET CONFIG)
    if(TinyORM_FOUND)
      set(_TINYORM_PACKAGE_OK ON)
      message(STATUS "Found TinyORM package: using imported targets")
    else()
      message(
        STATUS "TinyORM package not found, will fall back to subdirectory")
    endif()
  endif()

  if(NOT _TINYORM_PACKAGE_OK)
    # 回退：作为子目录加入，但用 EXCLUDE_FROM_ALL 降低无关改动引发的构建概率 注意：若你的目标链接到了
    # TinyORM，依赖会触发其构建，这属于预期。
    add_subdirectory(src/modules/TinyORM EXCLUDE_FROM_ALL)
    # 统一别名，便于在项目内以 TinyORM::TinyORM 进行链接
    if(TARGET TinyOrm)
      add_library(TinyORM::TinyORM ALIAS TinyOrm)
    elseif(TARGET TinyORM)
      add_library(TinyORM::TinyORM ALIAS TinyORM)
    endif()
  endif()
endif()
