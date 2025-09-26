## Config target
if(BUILD_SHARED_MODULES)
    add_library(config SHARED src/modules/Config/config.cpp)
    set_target_properties(config PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    add_library(config STATIC src/modules/Config/config.cpp)
endif()
target_include_directories(config PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/modules/Config)
set(_cfg_bin_dir "${CMAKE_BINARY_DIR}/bin")
set(_cfg_lib_dir "${CMAKE_BINARY_DIR}/lib")
set_target_properties(config PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_cfg_bin_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${_cfg_bin_dir}"
    ARCHIVE_OUTPUT_DIRECTORY "${_cfg_lib_dir}")
if(NOT TARGET Qt6::Core)
    message(FATAL_ERROR "Qt6::Core target not found. Ensure cmake/Dependencies.cmake found Qt6.")
endif()
target_link_libraries(config PUBLIC Qt6::Core)

install(
  TARGETS config
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)
