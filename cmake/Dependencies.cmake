# Dependencies
find_package(QT NAMES Qt6 Qt5 REQUIRED
             COMPONENTS CoreTools Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED
             COMPONENTS CoreTools Widgets LinguistTools Network)

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

if(DEFINED ENV{VCPKG_ROOT})
  message(STATUS "Using VCPKG from environment variable: $ENV{VCPKG_ROOT}")
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")

  set(OpenCV_ROOT "${VCPKG_ROOT}/installed/x64-windows/share/opencv4")

else()
  message(STATUS "VCPKG_ROOT environment variable is not set.")
endif()

# Core formatting / logging libraries used across the project
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED) # 使用 spdlog 作为日志库，比 QDebug 快且功能丰富
find_package(reflectcpp CONFIG REQUIRED) # 作为序列化库，用于配置、网络等模块

# Optional module-specific dependencies
if(BUILD_HUMAN_RECOGNITION)
  find_package(OpenCV REQUIRED)
endif()

if(BUILD_MQTT_CLIENT)
  find_package(PahoMqttCpp CONFIG REQUIRED) # Paho MQTT C++ client (or choose another MQTT client)
endif()
