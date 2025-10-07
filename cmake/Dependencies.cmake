# Dependencies.cmake: 管理项目依赖项的查找和基本校验

# Enable Qt auto tools so qt_add_executable / qt_create_translation are
# available
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 via vcpkg-installed packages. Adjust components as needed.
find_package(
  Qt6 CONFIG REQUIRED
  COMPONENTS Core
             Gui
             Widgets
             Network
             Sql
             Multimedia
             SerialPort
             LinguistTools
             Concurrent)

# 项目使用的核心格式化/日志库
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED) # 使用 spdlog 作为日志库，比 QDebug 更快且功能更丰富

find_package(Drogon CONFIG REQUIRED)
message(STATUS "Found Drogon: enabling drogon HTTP support")

find_package(OpenCV CONFIG REQUIRED)
message(STATUS "Found OpenCV with imported targets")

# dlib: prefer using the bundled submodule if present, otherwise fall back to
# find_package
option(DLIB_USE_SUBMODULE
       "Prefer using third_party/dlib submodule when present" ON)
set(_dlib_submodule_dir "${CMAKE_SOURCE_DIR}/third_party/dlib")

set(_dlib_use_find_package OFF)

if(DLIB_USE_SUBMODULE AND EXISTS "${_dlib_submodule_dir}")
  message(STATUS "Using dlib from submodule: ${_dlib_submodule_dir}")
  if(EXISTS "${_dlib_submodule_dir}/dlib/CMakeLists.txt")
    option(DLIB_ENABLE_CUDA "Enable CUDA support when building dlib" OFF)
    option(DLIB_ENABLE_FFMPEG "Enable FFmpeg support in dlib" ON)
    option(DLIB_ENABLE_JPEG "Enable JPEG support in dlib" ON)
    option(DLIB_ENABLE_PNG "Enable PNG support in dlib" ON)
    option(DLIB_ENABLE_BLAS "Enable BLAS/LAPACK support in dlib" ON)
    option(DLIB_ENABLE_ISO_CPP_ONLY
           "Build dlib in ISO C++ only mode (disables some wrappers)" OFF)
    # Optional string to set CUDA architectures explicitly (e.g. "75;80")
    set(DLIB_CUDA_ARCHITECTURES
        ""
        CACHE
          STRING
          "Semicolon-separated list of CUDA architectures for CMAKE_CUDA_ARCHITECTURES (leave empty for auto/native)"
    )

    # Map our options into dlib's expected cache variables and force them so
    # they are visible when dlib's CMake executes.
    set(DLIB_USE_CUDA
        ${DLIB_ENABLE_CUDA}
        CACHE BOOL "(from parent) Enable CUDA support for dlib" FORCE)
    set(DLIB_USE_FFMPEG
        ${DLIB_ENABLE_FFMPEG}
        CACHE BOOL "(from parent) Enable FFmpeg support for dlib" FORCE)
    set(DLIB_JPEG_SUPPORT
        ${DLIB_ENABLE_JPEG}
        CACHE BOOL "(from parent) JPEG support for dlib" FORCE)
    set(DLIB_PNG_SUPPORT
        ${DLIB_ENABLE_PNG}
        CACHE BOOL "(from parent) PNG support for dlib" FORCE)
    set(DLIB_USE_BLAS
        ${DLIB_ENABLE_BLAS}
        CACHE BOOL "(from parent) Use BLAS for dlib" FORCE)
    set(DLIB_USE_LAPACK
        ${DLIB_ENABLE_BLAS}
        CACHE BOOL "(from parent) Use LAPACK for dlib" FORCE)
    set(DLIB_ISO_CPP_ONLY
        ${DLIB_ENABLE_ISO_CPP_ONLY}
        CACHE BOOL "(from parent) Build dlib as ISO C++ only" FORCE)

    # If the user specified CUDA archs, propagate to CMake's CUDA variable
    if(DEFINED DLIB_CUDA_ARCHITECTURES AND NOT DLIB_CUDA_ARCHITECTURES STREQUAL
                                           "")
      set(CMAKE_CUDA_ARCHITECTURES
          ${DLIB_CUDA_ARCHITECTURES}
          CACHE STRING
                "CUDA architectures for project (set by Dependencies.cmake)"
                FORCE)
    endif()

    # Ensure a sane C++ standard is set before invoking dlib's CMake, because
    # dlib calls target_compile_features(dlib PUBLIC cxx_std_14) and CMake may
    # error if CMAKE_CXX_STANDARD is empty. Project-level standard is set later
    # in cmake/ProjectConfig.cmake, but when building the submodule we must
    # provide a default if none is present.
    if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD STREQUAL "")
      # dlib uses target_compile_features(... cxx_std_14), so ensure a valid C++
      # standard is set. Use C++14 as the minimal safe default for dlib.
      set(CMAKE_CXX_STANDARD
          14
          CACHE STRING
                "Default C++ standard for project (set by Dependencies.cmake)"
                FORCE)
      set(CMAKE_CXX_STANDARD_REQUIRED
          ON
          CACHE BOOL "Require C++ standard" FORCE)
      set(CMAKE_CXX_EXTENSIONS
          OFF
          CACHE BOOL "Disable compiler specific extensions" FORCE)
      message(
        STATUS
          "CMAKE_CXX_STANDARD was not set; forcing default ${CMAKE_CXX_STANDARD} for dlib submodule build"
      )
    endif()

    add_subdirectory("${_dlib_submodule_dir}/dlib")
  else()
    message(
      WARNING
        "Found ${_dlib_submodule_dir} but could not locate dlib/CMakeLists.txt - falling back to find_package(dlib)"
    )
    set(_dlib_use_find_package ON)
  endif()
else()
  if(DLIB_USE_SUBMODULE)
    message(
      WARNING
        "DLIB_USE_SUBMODULE is ON but ${_dlib_submodule_dir} does not exist. Falling back to find_package(dlib)."
    )
  endif()
  set(_dlib_use_find_package ON)
endif()

if(_dlib_use_find_package)
  find_package(dlib CONFIG REQUIRED)
  message(STATUS "Found dlib for HumanRecognition backend")
endif()

if(BUILD_MQTT_CLIENT)
  find_package(PahoMqttCpp CONFIG)
  if(PahoMqttCpp_FOUND)
    message(STATUS "Found Paho MQTT C++ client: ${PahoMqttCpp_VERSION}")
  else()
    message(
      FATAL_ERROR
        "PahoMqttCpp not found but BUILD_MQTT_CLIENT is enabled.\nPlease install paho-mqttpp3 via vcpkg (vcpkg install paho-mqttpp3) or disable BUILD_MQTT_CLIENT."
    )
    set(BUILD_MQTT_CLIENT OFF) # 没有找到 mqtt 库，直接禁用
  endif()
endif()
