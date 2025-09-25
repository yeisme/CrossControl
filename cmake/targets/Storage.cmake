if(BUILD_SHARED_MODULES)
  add_library(Storage SHARED ${STORAGE_SOURCES})

  # Ensure MSVC generates an import library even if there are no explicit
  # dllexport symbols
  set_target_properties(Storage PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  add_library(Storage STATIC ${STORAGE_SOURCES})
endif()

# Ensure Storage outputs go to the same bin/lib layout as other project-local
# targets Use single-config paths; Output.cmake normally handles this for other
# targets, but set explicitly here to avoid missing implib issues.
set(_storage_bin_dir "${CMAKE_BINARY_DIR}/bin")
set(_storage_lib_dir "${CMAKE_BINARY_DIR}/lib")
set_target_properties(
  Storage
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_storage_bin_dir}"
  LIBRARY_OUTPUT_DIRECTORY "${_storage_bin_dir}"
  ARCHIVE_OUTPUT_DIRECTORY "${_storage_lib_dir}")

# 确保生成的头文件目录对使用者可用。使用 ${CMAKE_BINARY_DIR}/include 作为生成头文件的公共位置。
target_include_directories(Storage PUBLIC "${CMAKE_BINARY_DIR}/include")

# Link Storage against Qt Sql and logging. TinyORM is no longer required.
find_package(Qt6 COMPONENTS Sql REQUIRED)
target_link_libraries(Storage PRIVATE Qt6::Sql logging)
message(STATUS "Linking Storage module with Qt Sql and logging")

target_include_directories(Storage PUBLIC include/modules/Storage)
target_link_libraries(CrossControl PUBLIC Storage)
