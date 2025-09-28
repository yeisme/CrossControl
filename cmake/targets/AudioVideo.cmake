  # AudioVideo module - always built and part of Core
  set(AUDIO_VIDEO_SOURCES src/modules/AudioVideo/audiovideo.cpp
                          include/modules/AudioVideo/audiovideo.h)
  if(BUILD_SHARED_MODULES)
    add_library(AudioVideo SHARED ${AUDIO_VIDEO_SOURCES})
    set_target_properties(AudioVideo PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    add_library(AudioVideo STATIC ${AUDIO_VIDEO_SOURCES})
  endif()
  # Link project logging
  target_link_libraries(AudioVideo PRIVATE logging)

  # Use Qt Multimedia for audio/video functionality
  find_package(Qt6 COMPONENTS Multimedia REQUIRED)
  target_link_libraries(AudioVideo PRIVATE Qt6::Multimedia)
  target_include_directories(AudioVideo PRIVATE ${CMAKE_SOURCE_DIR}/include
                                                include/modules/AudioVideo)

  if(NOT TARGET Qt6::Multimedia)
    message(
      FATAL_ERROR
        "AudioVideo requires Qt6::Multimedia target (Qt6 Multimedia not found).\n"
        "Ensure cmake/Dependencies.cmake configured Qt6 or set CMAKE_PREFIX_PATH to your Qt6 installation."
    )
  endif()
  target_link_libraries(CrossControl PRIVATE AudioVideo)

  # Install the AudioVideo module as part of the Core application component
  cc_install_target(AudioVideo Core)
