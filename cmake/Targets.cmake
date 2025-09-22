# Targets
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(CrossControl MANUAL_FINALIZATION ${PROJECT_SOURCES})
  qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
  if(ANDROID)
    add_library(CrossControl SHARED ${PROJECT_SOURCES})
  else()
    add_executable(CrossControl ${PROJECT_SOURCES})
  endif()
  qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# Module libraries
if(BUILD_HUMAN_RECOGNITION)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                                 Qt${QT_VERSION_MAJOR}::Network)
  target_include_directories(HumanRecognition
                             PRIVATE include/modules/HumanRecognition)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)
endif()

if(BUILD_AUDIO_VIDEO)
  if(BUILD_SHARED_MODULES)
    add_library(AudioVideo SHARED ${AUDIO_VIDEO_SOURCES})
  else()
    add_library(AudioVideo STATIC ${AUDIO_VIDEO_SOURCES})
  endif()
  target_link_libraries(AudioVideo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                           Qt${QT_VERSION_MAJOR}::Network
                                           Qt${QT_VERSION_MAJOR}::Multimedia)
  target_include_directories(AudioVideo PRIVATE include/modules/AudioVideo)
  target_link_libraries(CrossControl PRIVATE AudioVideo)
endif()

target_link_libraries(CrossControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                           Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(CrossControl PRIVATE include/widgets)

set_target_properties(CrossControl PROPERTIES WIN32_EXECUTABLE TRUE)
