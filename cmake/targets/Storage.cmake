if(BUILD_STORAGE)
  if(BUILD_SHARED_MODULES)
    add_library(Storage SHARED ${STORAGE_SOURCES})
    # Ensure MSVC generates an import library even if there are no explicit
    # dllexport symbols
    set_target_properties(Storage PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    add_library(Storage STATIC ${STORAGE_SOURCES})
  endif()

  # Ensure Storage outputs go to the same bin/lib layout as other project-local
  # targets Use single-config paths; Output.cmake normally handles this for
  # other targets, but set explicitly here to avoid missing implib issues.
  set(_storage_bin_dir "${CMAKE_BINARY_DIR}/bin")
  set(_storage_lib_dir "${CMAKE_BINARY_DIR}/lib")
  set_target_properties(
    Storage
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_storage_bin_dir}"
               LIBRARY_OUTPUT_DIRECTORY "${_storage_bin_dir}"
               ARCHIVE_OUTPUT_DIRECTORY "${_storage_lib_dir}")

  # Prefer linking to imported TinyORM target if present
  if(TARGET TinyORM::TinyORM)
    target_link_libraries(Storage PRIVATE TinyORM::TinyORM logging)
  else()
    # TinyORM not present as package; if BUILD_TINYORM enabled it may have been
    # added as subdirectory by ProjectConfig.cmake, create dependency alias if
    # available.
    if(TARGET TinyOrm)
      target_link_libraries(Storage PRIVATE TinyOrm logging)
    elseif(TARGET TinyORM)
      target_link_libraries(Storage PRIVATE TinyORM logging)
    else()
      # No TinyORM available: Storage will be built but without ORM linkage.
      target_link_libraries(Storage PRIVATE logging)
      message(
        STATUS
          "Building Storage without TinyORM - some features will be disabled")
    endif()
  endif()

  target_include_directories(Storage PUBLIC include/modules/Storage)
  target_link_libraries(CrossControl PRIVATE Storage)
endif()
