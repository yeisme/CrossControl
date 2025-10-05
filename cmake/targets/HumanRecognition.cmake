# HumanRecognition module - always built (no BUILD_HUMAN_RECOGNITION guard)
set(HUMAN_RECOGNITION_SOURCES
    include/modules/HumanRecognition/humanrecognition.h
    include/modules/HumanRecognition/factory.h
    include/modules/HumanRecognition/ihumanrecognitionbackend.h
    include/modules/HumanRecognition/types.h
    src/modules/HumanRecognition/humanrecognition.cpp
    src/modules/HumanRecognition/factory.cpp)
if(BUILD_SHARED_MODULES)
  add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
  # Ensure exported symbols on MSVC builds when no explicit export macro
  set_target_properties(HumanRecognition PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS
                                                    ON)
else()
  add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
endif()

if(NOT TARGET Qt6::Widgets)
  message(
    FATAL_ERROR
      "HumanRecognition requires Qt6 (Qt6::Widgets target not found).\n"
      "Ensure cmake/Dependencies.cmake configured Qt6 or set CMAKE_PREFIX_PATH to your Qt6 installation."
  )
endif()

target_include_directories(
  HumanRecognition PRIVATE ${CMAKE_SOURCE_DIR}/include
                           ${CMAKE_SOURCE_DIR}/include/modules/HumanRecognition)

target_link_libraries(HumanRecognition PRIVATE Qt6::Widgets Qt6::Network
                                               Qt6::Sql)
target_link_libraries(HumanRecognition PRIVATE Storage config logging)
target_link_libraries(CrossControl PRIVATE HumanRecognition)

# Register installation as part of the Core component so the module is installed
# together with the main application.
cc_install_target(HumanRecognition Core)
