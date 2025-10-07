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

find_package(dlib CONFIG)
message(STATUS "Found dlib for HumanRecognition backend")

if(NOT dlib_FOUND)
  # If dlib wasn't found via vcpkg/system, try local third_party/dlib installs created
  # by the provided CMakePresets under third_party/dlib/install/dlib/*.
  file(GLOB DLIB_INSTALL_DIRS "${CMAKE_SOURCE_DIR}/third_party/dlib/install/dlib/*")
  foreach(_d ${DLIB_INSTALL_DIRS})
    if(EXISTS "${_d}/lib/cmake/dlib" OR EXISTS "${_d}/share/dlib/cmake")
      list(APPEND CMAKE_PREFIX_PATH "${_d}")
    endif()
  endforeach()
  if(NOT CMAKE_PREFIX_PATH STREQUAL "")
    # Try to find again with the augmented CMAKE_PREFIX_PATH
    find_package(dlib CONFIG REQUIRED)
    message(STATUS "Found dlib in local third_party installs")
  else()
    message(STATUS "dlib not found via vcpkg/system or local third_party installs. Continuing without dlib (HumanRecognition backend may be disabled).")
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
