# DeviceGateway module - part of Core
set(DEVICE_GATEWAY_SOURCES
    src/modules/DeviceGateway/device_gateway.cpp
    include/modules/DeviceGateway/device_gateway.h
    src/modules/DeviceGateway/device_registry.cpp)

# REST server implementation for remote device registration
list(APPEND DEVICE_GATEWAY_SOURCES src/modules/DeviceGateway/rest_server.cpp
     include/modules/DeviceGateway/rest_server.h)

# Create the DeviceGateway module using the project's helper so the
# BUILD_SHARED_MODULES policy is respected and symbol export handling is
# consistent with other modules.
cc_create_module(DeviceGateway ${DEVICE_GATEWAY_SOURCES})

cc_set_output_dirs(DeviceGateway)

# If MQTT client support is enabled at configure time, expose a compile-time
# macro so source files can use #if BUILD_MQTT_CLIENT guards.
if(BUILD_MQTT_CLIENT)
  target_compile_definitions(DeviceGateway PUBLIC BUILD_MQTT_CLIENT=1)
endif()

target_include_directories(
  DeviceGateway
  PUBLIC "${CMAKE_BINARY_DIR}/include"
         "${CMAKE_SOURCE_DIR}/include"
         "${CMAKE_SOURCE_DIR}/include/modules/DeviceGateway"
         "${CMAKE_SOURCE_DIR}/include/modules/Storage"
         "${CMAKE_SOURCE_DIR}/include/modules/Config")

# Link to project libraries
target_link_libraries(DeviceGateway PUBLIC config logging Storage Qt6::Sql
                                           Connect)

# Use drogon for HTTP REST server Drogon discovery is centralized in
# cmake/Dependencies.cmake; link if found and expose a compile-time macro so
# sources can compile Drogon-specific code guarded by #ifdef HAS_DROGON. This
# mirrors the pattern used in Connect.cmake.
if(Drogon_FOUND)
  target_link_libraries(DeviceGateway PUBLIC Drogon::Drogon)
  target_compile_definitions(DeviceGateway PUBLIC HAS_DROGON=1)
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
else()
  message(
    STATUS
      "Drogon not found; DeviceGateway will build without embedded REST server")
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

cc_install_target(DeviceGateway DeviceGateway)
