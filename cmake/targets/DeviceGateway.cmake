# DeviceGateway module - part of Core
set(DEVICE_GATEWAY_SOURCES
    src/modules/DeviceGateway/devicegateway.cpp
    include/modules/DeviceGateway/devicegateway.h)

# DeviceGateway is always built as a shared library to allow optional
# installation and runtime loading as a separate module.
add_library(DeviceGateway SHARED ${DEVICE_GATEWAY_SOURCES})
set_target_properties(DeviceGateway PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# If MQTT client support is enabled at configure time, expose a compile-time
# macro so source files can use #if BUILD_MQTT_CLIENT guards.
if(BUILD_MQTT_CLIENT)
  target_compile_definitions(DeviceGateway PRIVATE BUILD_MQTT_CLIENT=1)
endif()

target_include_directories(DeviceGateway PRIVATE ${CMAKE_SOURCE_DIR}/include
                                                    include/modules/DeviceGateway
                                                    include/modules/Storage
                                                    include/modules/Config)

# Link to project libraries
target_link_libraries(DeviceGateway PRIVATE config logging Storage)
# DeviceGateway depends on Connect for networking functionality
if(TARGET Connect)
  target_link_libraries(DeviceGateway PRIVATE Connect)
endif()

if(BUILD_MQTT_CLIENT)
  # Prefer the CMake target exported by PahoMqttCpp if available
  if(TARGET PahoMqttCpp::paho-mqttpp3)
    target_link_libraries(DeviceGateway PRIVATE PahoMqttCpp::paho-mqttpp3)
  elseif(TARGET PahoMqttCpp)
    target_link_libraries(DeviceGateway PRIVATE PahoMqttCpp)
  else()
    message(STATUS "BUILD_MQTT_CLIENT is ON but PahoMqttCpp target not found; continuing without MQTT client.")
  endif()
endif()

# Ensure the main executable links with DeviceGateway so symbols are available at runtime
target_link_libraries(CrossControl PRIVATE DeviceGateway)

# Register installing as a separate optional component named DeviceGateway
cc_install_target(DeviceGateway DeviceGateway)
