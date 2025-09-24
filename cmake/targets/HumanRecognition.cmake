if(BUILD_HUMAN_RECOGNITION)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                                 Qt${QT_VERSION_MAJOR}::Network)
  target_link_libraries(HumanRecognition PRIVATE logging)
  target_include_directories(HumanRecognition
                             PRIVATE include/modules/HumanRecognition)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)
endif()
