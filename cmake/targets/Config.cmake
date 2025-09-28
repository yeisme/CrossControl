# 配置模块
set(CONFIG_SOURCES src/modules/Config/config.cpp
                   include/modules/Config/config.h)

cc_create_module(config ${CONFIG_SOURCES})
target_include_directories(
  config PUBLIC ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/include/modules/Config)
if(NOT TARGET Qt6::Core)
  message(
    FATAL_ERROR
      "Qt6::Core target not found. Ensure cmake/Dependencies.cmake found Qt6.")
endif()
target_link_libraries(config PUBLIC Qt6::Core)

# Use helper to set consistent output directories (bin/lib)
cc_set_output_dirs(config)

cc_install_target(config Core)
