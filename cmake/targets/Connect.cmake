if(BUILD_SHARED_MODULES)
  add_library(Connect SHARED ${CONNECT_SOURCES})
  set_target_properties(Connect PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  add_library(Connect STATIC ${CONNECT_SOURCES})
endif()

set(_connect_bin_dir "${CMAKE_BINARY_DIR}/bin")
set(_connect_lib_dir "${CMAKE_BINARY_DIR}/lib")
set_target_properties(
  Connect
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_connect_bin_dir}"
             LIBRARY_OUTPUT_DIRECTORY "${_connect_bin_dir}"
             ARCHIVE_OUTPUT_DIRECTORY "${_connect_lib_dir}")

target_include_directories(Connect PUBLIC "${CMAKE_BINARY_DIR}/include")
target_include_directories(Connect PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/widgets)
target_include_directories(Connect PUBLIC include/modules/Connect)

find_package(Qt6 COMPONENTS Network REQUIRED)
target_link_libraries(Connect PRIVATE Qt6::Network logging config)

target_link_libraries(CrossControl PUBLIC Connect)

install(
  TARGETS Connect
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)
