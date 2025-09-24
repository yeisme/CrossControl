## Logging target
add_library(logging STATIC src/logging/logging.cpp)
target_include_directories(logging PUBLIC include/logging)
target_link_libraries(logging PUBLIC spdlog::spdlog fmt::fmt
                                     Qt${QT_VERSION_MAJOR}::Widgets)
