## Logging target
if(BUILD_SHARED_MODULES)
    add_library(logging SHARED src/logging/logging.cpp)
    set_target_properties(logging PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    add_library(logging STATIC src/logging/logging.cpp)
endif()
target_include_directories(logging PUBLIC include/logging)
set(_log_bin_dir "${CMAKE_BINARY_DIR}/bin")
set(_log_lib_dir "${CMAKE_BINARY_DIR}/lib")
set_target_properties(logging PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_log_bin_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${_log_bin_dir}"
    ARCHIVE_OUTPUT_DIRECTORY "${_log_lib_dir}")
target_link_libraries(logging PUBLIC spdlog::spdlog fmt::fmt
                                     Qt${QT_VERSION_MAJOR}::Widgets)

                                     
install(
  TARGETS logging
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)
