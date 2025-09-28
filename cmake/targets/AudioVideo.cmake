if(BUILD_AUDIO_VIDEO)
  # AudioVideo 模块的源文件
  set(AUDIO_VIDEO_SOURCES src/modules/AudioVideo/audiovideo.cpp
                          include/modules/AudioVideo/audiovideo.h)
  if(BUILD_SHARED_MODULES)
    add_library(AudioVideo SHARED ${AUDIO_VIDEO_SOURCES})
    set_target_properties(AudioVideo PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    add_library(AudioVideo STATIC ${AUDIO_VIDEO_SOURCES})
  endif()
  # 先链接项目的 logging 模块
  target_link_libraries(AudioVideo PRIVATE logging)

  # 使用 Qt Multimedia 作为音视频功能的默认后端。 这使得 Multimedia 成为 AudioVideo 目标的显式依赖。
  find_package(
    Qt6
    COMPONENTS Multimedia
    REQUIRED)
  target_link_libraries(AudioVideo PRIVATE Qt6::Multimedia)
  target_include_directories(AudioVideo PRIVATE ${CMAKE_SOURCE_DIR}/include
                                                include/modules/AudioVideo)

  # Ensure Qt Multimedia target is available
  if(NOT TARGET Qt6::Multimedia)
    message(
      FATAL_ERROR
        "AudioVideo requires Qt6::Multimedia target (Qt6 Multimedia not found).\n"
        "Ensure cmake/Dependencies.cmake configured Qt6 or set CMAKE_PREFIX_PATH to your Qt6 installation."
    )
  endif()
  target_link_libraries(CrossControl PRIVATE AudioVideo)

  cc_install_target(AudioVideo AudioVideo)

endif()
