# Logging 模块（通过 helper 创建）
cc_create_module(logging src/logging/logging.cpp)
target_include_directories(logging PUBLIC include/logging)
if(NOT TARGET Qt6::Widgets)
  message(
    FATAL_ERROR
      "Qt6::Widgets target not found. Ensure cmake/Dependencies.cmake found Qt6."
  )
endif()
target_link_libraries(logging PUBLIC spdlog::spdlog fmt::fmt Qt6::Widgets)

cc_install_target(logging Core)
