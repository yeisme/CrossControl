# Connect 模块的源文件
set(CONNECT_SOURCES
    src/modules/Connect/tcp_connect.cpp
  src/modules/Connect/iface_connect.cpp
    src/modules/Connect/udp_connect.cpp
    src/modules/Connect/mqtt_connect.cpp
    src/modules/Connect/serial_connect.cpp
    src/modules/Connect/connect_factory.cpp
    include/modules/Connect/iface_connect.h
    include/modules/Connect/tcp_connect.h
    include/modules/Connect/connect_factory.h
    include/modules/Connect/connect_wrapper.h
    include/widgets/connectwidget.h
    src/widgets/connectwidget.cpp)

if(BUILD_SHARED_MODULES)
  add_library(Connect SHARED ${CONNECT_SOURCES})
  set_target_properties(Connect PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  add_library(Connect STATIC ${CONNECT_SOURCES})
endif()

# 使用 helper 将输出目录设置为一致的 bin/lib 布局
cc_set_output_dirs(Connect)

target_include_directories(
  Connect PUBLIC "${CMAKE_BINARY_DIR}/include" "${CMAKE_SOURCE_DIR}/include"
                 "${CMAKE_SOURCE_DIR}/include/widgets" include/modules/Connect)

# 确保需要的 Qt6 组件存在
find_package(
  Qt6
  COMPONENTS Network Widgets SerialPort
  REQUIRED)
if(NOT TARGET Qt6::Network OR NOT TARGET Qt6::Widgets)
  message(
    FATAL_ERROR
      "Connect module requires Qt6::Network, Qt6::Widgets and Qt6::SerialPort targets. Ensure cmake/Dependencies.cmake configured Qt6."
  )
endif()

target_link_libraries(Connect PUBLIC Qt6::Network Qt6::Widgets Qt6::SerialPort
                                     logging config)

<<<<<<< HEAD
# Provide drogon-based HTTP client helpers Drogon discovery is centralized in
# cmake/Dependencies.cmake; if found enable helpers
if(Drogon_FOUND)
  target_link_libraries(Connect PUBLIC Drogon::Drogon)
  target_compile_definitions(Connect PUBLIC HAS_DROGON=1)
else()
  message(
    STATUS
      "Drogon not found; Connect will not expose drogon HTTP client helpers")
endif()

=======
>>>>>>> parent of afbbae8 (feat: 集成 Drogon HTTP 框架，重构 DeviceGateway 模块的 REST 服务器实现)
if(BUILD_MQTT_CLIENT)
  if(TARGET PahoMqttCpp::paho-mqttpp3)
    target_link_libraries(Connect PUBLIC PahoMqttCpp::paho-mqttpp3)
  elseif(TARGET PahoMqttCpp)
    target_link_libraries(Connect PUBLIC PahoMqttCpp)
  else()
    message(
      STATUS
        "BUILD_MQTT_CLIENT is ON but PahoMqttCpp target not found; mqtt support in Connect will be limited."
    )
  endif()
  target_compile_definitions(Connect PUBLIC BUILD_MQTT_CLIENT=1)
endif()

# 仅当 CrossControl 目标存在时才将 Connect 链接到它
if(TARGET CrossControl)
  target_link_libraries(CrossControl PUBLIC Connect)
endif()

# 使用 helper 安装目标并分配到 Core 组件（Connect 功能作为 Core 的一部分） Install Connect as its own
# CPACK component when MQTT client support is enabled
cc_install_target(Connect Connect)
