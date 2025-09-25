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
message(
  STATUS " * feature BUILD_HUMAN_RECOGNITION: ${BUILD_HUMAN_RECOGNITION}")
option(BUILD_AUDIO_VIDEO "Build Audio Video module" ON)
message(STATUS " * feature BUILD_AUDIO_VIDEO: ${BUILD_AUDIO_VIDEO}")
option(BUILD_MQTT_CLIENT "Build MQTT Client module" ON)
message(STATUS " * feature BUILD_MQTT_CLIENT: ${BUILD_MQTT_CLIENT}")
# Storage module is required for the project; always enable it.
set(BUILD_STORAGE ON CACHE BOOL "Build Storage (data storage) module" FORCE)
message(STATUS " * feature BUILD_STORAGE: ON (required)")
option(BUILD_SHARED_MODULES "Build modules as shared libraries" ON)
message(STATUS " * feature BUILD_SHARED_MODULES: ${BUILD_SHARED_MODULES}")
option(BUILD_TINYORM "Build TinyORM ORM library" ON)
message(STATUS " * feature BUILD_TINYORM: ${BUILD_TINYORM}")

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
endif()

# 可选的第三方模块
if(BUILD_TINYORM)
  # 优先使用外部已安装/预编译的 TinyORM（避免作为子项目时频繁受顶层改动牵连重编译）
  option(USE_SYSTEM_TINYORM
         "Prefer an installed TinyORM package over subdirectory" ON)
  set(_TINYORM_PACKAGE_OK OFF)

  if(USE_SYSTEM_TINYORM)
    # 建议使用已安装的 TinyORM 软件包；但是，避免将 CMAKE_PREFIX_PATH 指向一个不完整的构建树软件包，
    # 该软件包可能包含一个配置文件但缺少生成的目标文件（导致包含错误）。仅在配置文件和目标文件都存在时添加构建树前缀。
    set(_tinyorm_build_prefix "${CMAKE_BINARY_DIR}/src/modules/TinyORM")
    if(EXISTS "${_tinyorm_build_prefix}/TinyOrmConfig.cmake" AND EXISTS "${_tinyorm_build_prefix}/TinyOrmTargets.cmake")
      set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${_tinyorm_build_prefix}")
      message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    else()
      message(STATUS "Skipping adding build-tree TinyORM package (incomplete): ${_tinyorm_build_prefix}")
    endif()
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
    # 启用特性：当使用仓库内 TinyORM 源代码构建时，默认开启 TOM（命令行工具）和示例。
    if(NOT DEFINED TOM_EXAMPLE)
      set(TOM_EXAMPLE ON CACHE BOOL "Build TOM example application" )
    endif()

    # 统一别名，便于在项目内以 TinyOrm::TinyOrm 或 TinyORM::TinyORM 进行链接 Prefer providing
    # both alias styles so downstream code (and vcpkg/official targets) can link
    # using either convention.
    if(TARGET TinyOrm)
      if(NOT TARGET TinyOrm::TinyOrm)
        add_library(TinyOrm::TinyOrm ALIAS TinyOrm)
      endif()
      if(NOT TARGET TinyORM::TinyORM)
        add_library(TinyORM::TinyORM ALIAS TinyOrm)
      endif()

    elseif(TARGET TinyORM)
      if(NOT TARGET TinyOrm::TinyOrm)
        add_library(TinyOrm::TinyOrm ALIAS TinyORM)
      endif()
      if(NOT TARGET TinyORM::TinyORM)
        add_library(TinyORM::TinyORM ALIAS TinyORM)
      endif()
    endif()
  endif()
  message(STATUS "set TinyOrm::TinyOrm and TinyORM::TinyORM aliases")
endif()
