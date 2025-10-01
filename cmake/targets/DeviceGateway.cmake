# DeviceGateway module - part of Core
set(DEVICE_GATEWAY_SOURCES
    src/modules/DeviceGateway/device_gateway.cpp
    include/modules/DeviceGateway/device_gateway.h
    src/modules/DeviceGateway/device_registry.cpp)

# REST server implementation for remote device registration
list(APPEND DEVICE_GATEWAY_SOURCES src/modules/DeviceGateway/rest_server.cpp
     include/modules/DeviceGateway/rest_server.h)

# DeviceGateway is always built as a shared library to allow optional
# installation and runtime loading as a separate module. Don't change to STATIC.
add_library(DeviceGateway SHARED ${DEVICE_GATEWAY_SOURCES})
set_target_properties(DeviceGateway PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

cc_set_output_dirs(DeviceGateway)

# If MQTT client support is enabled at configure time, expose a compile-time
# macro so source files can use #if BUILD_MQTT_CLIENT guards.
if(BUILD_MQTT_CLIENT)
  target_compile_definitions(DeviceGateway PUBLIC BUILD_MQTT_CLIENT=1)
endif()

target_include_directories(
  DeviceGateway
  PUBLIC ${CMAKE_SOURCE_DIR}/include include/modules/DeviceGateway
         include/modules/Storage include/modules/Config)

# Link to project libraries
target_link_libraries(DeviceGateway PUBLIC config logging Storage Qt6::Sql)

# Use drogon for HTTP REST server Drogon discovery is centralized in
# cmake/Dependencies.cmake; require it here
if(NOT Drogon_FOUND)
  message(
    FATAL_ERROR
      "DeviceGateway requires Drogon but it was not found. Enable/install drogon via vcpkg and re-run configure."
  )
else()
  target_link_libraries(DeviceGateway PUBLIC Drogon::Drogon)
  # Trantor is Drogon's dependency; some enums are referenced directly. Link
  # against several common imported target names to be robust to differences in
  # how drogon/trantor are exported by package managers.
  if(TARGET trantor)
    target_link_libraries(DeviceGateway PUBLIC trantor)
  elseif(TARGET trantor::trantor)
    target_link_libraries(DeviceGateway PUBLIC trantor::trantor)
  elseif(TARGET Trantor::Trantor)
    target_link_libraries(DeviceGateway PUBLIC Trantor::Trantor)
  elseif(TARGET trantor::Trantor)
    target_link_libraries(DeviceGateway PUBLIC trantor::Trantor)
  endif()
endif()
# DeviceGateway depends on Connect for networking functionality
if(TARGET Connect)
  target_link_libraries(DeviceGateway PUBLIC Connect)
endif()

if(BUILD_MQTT_CLIENT)
  # Prefer the CMake target exported by PahoMqttCpp if available
  if(TARGET PahoMqttCpp::paho-mqttpp3)
    target_link_libraries(DeviceGateway PUBLIC PahoMqttCpp::paho-mqttpp3)
  elseif(TARGET PahoMqttCpp)
    target_link_libraries(DeviceGateway PUBLIC PahoMqttCpp)
  else()
    message(
      STATUS
        "BUILD_MQTT_CLIENT is ON but PahoMqttCpp target not found; continuing without MQTT client."
    )
  endif()
endif()

# Ensure the main executable links with DeviceGateway so symbols are available
# at runtime
target_link_libraries(CrossControl PUBLIC DeviceGateway)

# Register installing as a separate optional component named DeviceGateway
cc_install_target(DeviceGateway DeviceGateway)
