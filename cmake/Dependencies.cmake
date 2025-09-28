# Dependencies.cmake: 管理项目依赖项的查找和基本校验

# Enable Qt auto tools so qt_add_executable / qt_create_translation are available
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 via vcpkg-installed packages. Adjust components as needed.
find_package(Qt6 CONFIG REQUIRED COMPONENTS
  Core
  Gui
  Widgets
  Network
  Sql
  Multimedia
  LinguistTools
)

# 项目使用的核心格式化/日志库
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED) # 使用 spdlog 作为日志库，比 QDebug 更快且功能更丰富

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

# 人脸识别功能需要 OpenCV。倾向于快速失败并给出有帮助的提示信息。
if(BUILD_HUMAN_RECOGNITION)
  find_package(OpenCV CONFIG QUIET)
  if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
  # 将包含目录和库以变量形式暴露，供 Targets.cmake 使用
    set(PROJECT_HAS_OPENCV
        ON
        CACHE BOOL "OpenCV found for project" FORCE)
  else()
    message(
      FATAL_ERROR
        "OpenCV not found but BUILD_HUMAN_RECOGNITION is enabled.\nPlease enable the 'opencv' feature in vcpkg (ensure vcpkg.json lists opencv4) or install opencv4 via vcpkg.\nIf you don't need the human recognition module, set -DBUILD_HUMAN_RECOGNITION=OFF when configuring."
    )
  endif()
endif()
