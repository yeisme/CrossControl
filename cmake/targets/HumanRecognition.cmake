if(BUILD_HUMAN_RECOGNITION)
  # HumanRecognition 模块的源文件
  set(HUMAN_RECOGNITION_SOURCES
      src/modules/HumanRecognition/humanrecognition.cpp
      src/modules/HumanRecognition/opencv_backend.cpp
      include/modules/HumanRecognition/humanrecognition.h
      include/modules/HumanRecognition/iface_human_recognition.h
      include/modules/HumanRecognition/opencv_backend.h)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
    # 允许在未显式标注导出符号时仍输出所有符号（已有显式宏时也不冲突）
    set_target_properties(HumanRecognition PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS
                                                      ON)
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(HumanRecognition PRIVATE Qt6::Widgets Qt6::Network
                                                 Qt6::Sql)
  target_link_libraries(HumanRecognition PRIVATE logging)

  # 确保 Qt6 目标可用（Dependencies.cmake 应当提供这些）
  if(NOT TARGET Qt6::Widgets)
    message(
      FATAL_ERROR
        "HumanRecognition requires Qt6 (Qt6::Widgets target not found).\n"
        "Ensure cmake/Dependencies.cmake configured Qt6 or set CMAKE_PREFIX_PATH to your Qt6 installation."
    )
  endif()
  # OpenCV - delegate all link choices to ${OpenCV_LIBS} provided by
  # find_package. Do NOT hardcode component target names here; let the
  # find_package implementation (FindOpenCV or OpenCVConfig) populate
  # ${OpenCV_LIBS} appropriately.
  find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect)
  message(
    STATUS "HumanRecognition: OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
  message(
    STATUS "HumanRecognition: linking with OpenCV libs variable: ${OpenCV_LIBS}"
  )
  target_link_libraries(HumanRecognition PRIVATE ${OpenCV_LIBS})
  target_include_directories(
    HumanRecognition
    PRIVATE ${CMAKE_SOURCE_DIR}/include include/modules/HumanRecognition
            include/modules/Storage)
  target_link_libraries(HumanRecognition PRIVATE Storage)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)

  # register installation with component (output dirs are handled by
  # Output.cmake)
  cc_install_target(HumanRecognition HumanRecognition)

endif()
