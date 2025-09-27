# 依赖项：只支持 Qt6，Qt Multimedia 已列为必需组件
find_package(Qt6 REQUIRED COMPONENTS CoreTools Widgets LinguistTools Network
                                     Multimedia)
set(QT_VERSION_MAJOR 6)
message(STATUS "Found and using Qt6 (QT_VERSION_MAJOR=${QT_VERSION_MAJOR})")

# 输出 QT 版本相关信息和库路径
message(STATUS "Using Qt version: ${Qt${QT_VERSION_MAJOR}_VERSION}")
message(
  STATUS
    "Qt${QT_VERSION_MAJOR} CoreTools library: ${Qt${QT_VERSION_MAJOR}_CoreTools_LIBRARIES}"
)
message(
  STATUS
    "Qt${QT_VERSION_MAJOR} Widgets library: ${Qt${QT_VERSION_MAJOR}_Widgets_LIBRARIES}"
)
message(
  STATUS
    "Qt${QT_VERSION_MAJOR} LinguistTools library: ${Qt${QT_VERSION_MAJOR}_LinguistTools_LIBRARIES}"
)
message(
  STATUS
    "Qt${QT_VERSION_MAJOR} Network library: ${Qt${QT_VERSION_MAJOR}_Network_LIBRARIES}"
)

message(
  STATUS
    "Qt${QT_VERSION_MAJOR} Multimedia library: ${Qt${QT_VERSION_MAJOR}_Multimedia_LIBRARIES}"
)

# message 状态：CMAKE_TOOLCHAIN_FILE
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

# # 允许用户通过环境变量或 CMake 缓存变量指定系统 OpenCV 根目录。 OpenCV 查找优先级： 1) CMake 变量
# `OpenCV_ROOT`（例如 -DOpenCV_ROOT=...） 2) 环境变量 OPENCV_ROOT 3) 使用 find_package
# 在系统位置找到的 OpenCV 4) 如果存在 VCPKG_ROOT，则使用 vcpkg 安装的 OpenCV
if(DEFINED ENV{OPENCV_ROOT} AND NOT DEFINED OpenCV_ROOT)
  message(STATUS "Using OPENCV_ROOT environment variable: $ENV{OPENCV_ROOT}")
  set(OpenCV_ROOT
      "$ENV{OPENCV_ROOT}"
      CACHE PATH "Path to system OpenCV (share/opencv4 or lib/cmake/opencv4)")
endif()

if(DEFINED ENV{VCPKG_ROOT})
  message(STATUS "Using VCPKG from environment variable: $ENV{VCPKG_ROOT}")
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")

  # 仅在用户/系统未提供 OpenCV_ROOT 时设置 vcpkg 的 OpenCV 候选路径
  if(NOT DEFINED OpenCV_ROOT)
    if(WIN32)
      # Windows 下 vcpkg 通常安装在 VCPKG_ROOT 下的 installed/x64-windows 目录中，OpenCV 的
      # cmake 配置文件位于 share/opencv4
      set(_VCPKG_OPENCV_ROOT
          "$ENV{VCPKG_ROOT}/installed/x64-windows/share/opencv4")
    elseif(LINUX)
      # Linux 下 vcpkg 通常安装在 VCPKG_ROOT 下的 installed/x64-linux 目录中，OpenCV 的 cmake
      # 配置文件位于 share/opencv4
      set(_VCPKG_OPENCV_ROOT
          "$ENV{VCPKG_ROOT}/installed/x64-linux/share/opencv4")
    endif()

    # store candidate in OpenCV_ROOT so later find_package can use it as a hint
    set(OpenCV_ROOT "${_VCPKG_OPENCV_ROOT}")
  endif()
else()
  message(STATUS "VCPKG_ROOT environment variable is not set.")
endif()

# 项目中使用的核心格式化/日志库
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED) # 使用 spdlog 作为日志库，比 QDebug 快且功能丰富

# Optional module-specific dependencies
if(BUILD_HUMAN_RECOGNITION)
  # 按照上面描述的优先级尝试定位 OpenCV。
  set(_opencv_found FALSE)
  # When searching for OpenCV prefer CONFIG mode (imported targets) so that
  # vcpkg-provided opencv_world or exported targets are selected instead of
  # falling back to older FindOpenCV behavior which populates ${OpenCV_LIBS}
  # with component -lopencv_* flags.
  if(DEFINED CMAKE_FIND_PACKAGE_PREFER_CONFIG)
    set(_old_find_pref "${CMAKE_FIND_PACKAGE_PREFER_CONFIG}")
  else()
    set(_old_find_pref "")
  endif()
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

  # 1) If vcpkg is available, try common vcpkg install paths first
  # (linux/windows)
  if(DEFINED ENV{VCPKG_ROOT})
    set(_vcpkg_candidates
        "$ENV{VCPKG_ROOT}/installed/x64-linux/share/opencv4"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/share/opencv4")
    foreach(_cand IN LISTS _vcpkg_candidates)
      file(TO_CMAKE_PATH "${_cand}" _cand_path)
      message(STATUS "Trying vcpkg OpenCV candidate: ${_cand_path}")
      find_package(OpenCV QUIET CONFIG PATHS "${_cand_path}")
      if(OpenCV_FOUND)
        set(_opencv_found TRUE)
        set(OpenCV_ROOT
            "${_cand}"
            CACHE PATH "Path to OpenCV used (vcpkg)")
        message(
          STATUS "Found OpenCV (vcpkg): ${OpenCV_VERSION} @ ${OpenCV_DIR}")
        message(STATUS "OpenCV components/targets: ${OpenCV_LIBS}")
        break()
      endif()
    endforeach()
  endif()

  # 2) If user provided OpenCV_ROOT explicitly, try it next (config-mode)
  if(NOT _opencv_found AND DEFINED OpenCV_ROOT)
    file(TO_CMAKE_PATH "${OpenCV_ROOT}" _OpenCV_ROOT_PATH)
    message(STATUS "Trying OpenCV from OpenCV_ROOT: ${_OpenCV_ROOT_PATH}")
    find_package(
      OpenCV
      QUIET
      CONFIG
      PATHS
      "${_OpenCV_ROOT_PATH}/share/opencv4"
      "${_OpenCV_ROOT_PATH}/lib/cmake/opencv4"
      "${_OpenCV_ROOT_PATH}")
    if(OpenCV_FOUND)
      set(_opencv_found TRUE)
      message(
        STATUS
          "Found OpenCV (from OpenCV_ROOT): ${OpenCV_VERSION} @ ${OpenCV_DIR}")
    endif()
  endif()

  # 3) Finally try system default search (still with prefer_config ON so any
  # exported config package will be preferred)
  if(NOT _opencv_found)
    message(STATUS "Trying system-installed OpenCV (default search paths)")
    find_package(OpenCV QUIET)
    if(OpenCV_FOUND)
      set(_opencv_found TRUE)
      message(STATUS "Found OpenCV (system): ${OpenCV_VERSION} @ ${OpenCV_DIR}")
    endif()
  endif()

  # restore previous preference
  if(NOT _old_find_pref STREQUAL "")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG "${_old_find_pref}")
  else()
    unset(CMAKE_FIND_PACKAGE_PREFER_CONFIG)
  endif()

  if(NOT _opencv_found)
    message(
      FATAL_ERROR
        "OpenCV not found. Please install OpenCV system-wide or set OPENCV_ROOT / -DOpenCV_ROOT to the OpenCV config directory, or enable vcpkg and let it install OpenCV."
    )
  endif()
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
