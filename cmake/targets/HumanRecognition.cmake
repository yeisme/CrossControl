# HumanRecognition module - always built (no BUILD_HUMAN_RECOGNITION guard)
set(HUMAN_RECOGNITION_SOURCES
    include/modules/HumanRecognition/humanrecognition.h
    include/modules/HumanRecognition/factory.h
    include/modules/HumanRecognition/ihumanrecognitionbackend.h
    include/modules/HumanRecognition/types.h
  include/modules/HumanRecognition/impl/opencv_dlib/backend.h
    src/modules/HumanRecognition/humanrecognition.cpp
  src/modules/HumanRecognition/factory.cpp
  src/modules/HumanRecognition/impl/opencv_dlib/backend.cpp
  src/modules/HumanRecognition/impl/opencv_dlib/backend_impl_core.cpp
  src/modules/HumanRecognition/impl/opencv_dlib/backend_impl_recognition.cpp)
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

if(TARGET dlib::dlib)
  target_link_libraries(HumanRecognition PRIVATE dlib::dlib)
  target_compile_definitions(HumanRecognition PRIVATE HAS_DLIB=1)
else()
  message(FATAL_ERROR "dlib::dlib target not found. Ensure dlib is available via your package manager")
endif()

if(TARGET opencv_world)
  target_link_libraries(HumanRecognition PRIVATE opencv_world)
  target_compile_definitions(HumanRecognition PRIVATE HAS_OPENCV=1)
elseif(TARGET opencv::opencv)
  target_link_libraries(HumanRecognition PRIVATE opencv::opencv)
  target_compile_definitions(HumanRecognition PRIVATE HAS_OPENCV=1)
elseif(TARGET opencv_core)
  target_link_libraries(HumanRecognition PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)
  target_compile_definitions(HumanRecognition PRIVATE HAS_OPENCV=1)
else()
  message(
    FATAL_ERROR
      "OpenCV targets not found. Expected opencv_world / opencv::opencv / opencv_core. Ensure OpenCV is available")
endif()
target_link_libraries(CrossControl PRIVATE HumanRecognition)

# Register installation as part of the Core component so the module is installed
# together with the main application.
cc_install_target(HumanRecognition Core)
