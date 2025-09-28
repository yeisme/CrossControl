  # HumanRecognition module - always built (no BUILD_HUMAN_RECOGNITION guard)
  set(HUMAN_RECOGNITION_SOURCES
      src/modules/HumanRecognition/humanrecognition.cpp
      src/modules/HumanRecognition/opencv_backend.cpp
      include/modules/HumanRecognition/humanrecognition.h
      include/modules/HumanRecognition/iface_human_recognition.h
      include/modules/HumanRecognition/opencv_backend.h)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
    # Ensure exported symbols on MSVC builds when no explicit export macro
    set_target_properties(HumanRecognition PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS
                                                      ON)
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt6::Widgets Qt6::Network
                                                 Qt6::Sql)
  target_link_libraries(HumanRecognition PRIVATE logging)

  # Ensure Qt6 dependencies are available; fail early with a clear message
  if(NOT TARGET Qt6::Widgets)
    message(
      FATAL_ERROR
        "HumanRecognition requires Qt6 (Qt6::Widgets target not found).\n"
        "Ensure cmake/Dependencies.cmake configured Qt6 or set CMAKE_PREFIX_PATH to your Qt6 installation."
    )
  endif()

  # OpenCV - require it and let find_package populate ${OpenCV_LIBS}
  find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect)
  message(STATUS "HumanRecognition: OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "HumanRecognition: linking with OpenCV libs variable: ${OpenCV_LIBS}")
  target_link_libraries(HumanRecognition PRIVATE ${OpenCV_LIBS})
  target_include_directories(
    HumanRecognition
    PRIVATE ${CMAKE_SOURCE_DIR}/include include/modules/HumanRecognition
            include/modules/Storage)
  target_link_libraries(HumanRecognition PRIVATE Storage)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)

  # Register installation as part of the Core component so the module is
  # installed together with the main application.
  cc_install_target(HumanRecognition Core)
