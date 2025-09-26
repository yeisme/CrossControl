if(BUILD_AUDIO_VIDEO)
  if(BUILD_SHARED_MODULES)
    add_library(AudioVideo SHARED ${AUDIO_VIDEO_SOURCES})
    set_target_properties(AudioVideo PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    add_library(AudioVideo STATIC ${AUDIO_VIDEO_SOURCES})
  endif()
  # Link project logging first
  target_link_libraries(AudioVideo PRIVATE logging)

  # Use Qt Multimedia as the default backend for audio/video functionality.
  # This makes Multimedia an explicit dependency of the AudioVideo target.
  find_package(Qt6 COMPONENTS Multimedia REQUIRED)
  target_link_libraries(AudioVideo PRIVATE Qt6::Multimedia)
  target_include_directories(AudioVideo PRIVATE include/modules/AudioVideo)
  target_link_libraries(CrossControl PRIVATE AudioVideo)

  set(_av_bin_dir "${CMAKE_BINARY_DIR}/bin")
  set(_av_lib_dir "${CMAKE_BINARY_DIR}/lib")
  set_target_properties(
    AudioVideo
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_av_bin_dir}"
               LIBRARY_OUTPUT_DIRECTORY "${_av_bin_dir}"
               ARCHIVE_OUTPUT_DIRECTORY "${_av_lib_dir}")

  install(
    TARGETS AudioVideo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)

endif()
