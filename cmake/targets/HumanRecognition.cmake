if(BUILD_HUMAN_RECOGNITION)
  if(BUILD_SHARED_MODULES)
    add_library(HumanRecognition SHARED ${HUMAN_RECOGNITION_SOURCES})
    # 允许在未显式标注导出符号时仍输出所有符号（已有显式宏时也不冲突）
    set_target_properties(HumanRecognition PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS
                                                      ON)
  else()
    add_library(HumanRecognition STATIC ${HUMAN_RECOGNITION_SOURCES})
  endif()
  target_link_libraries(
    HumanRecognition
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::Sql)
  target_link_libraries(HumanRecognition PRIVATE logging)
  # OpenCV - ensure we locate OpenCV and provide include dirs to the target
  find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect)
  message(STATUS "HumanRecognition: OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
  target_link_libraries(HumanRecognition PRIVATE ${OpenCV_LIBS})
  target_include_directories(HumanRecognition PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_include_directories(
    HumanRecognition
    PRIVATE ${CMAKE_SOURCE_DIR}/include include/modules/HumanRecognition
            include/modules/Storage)
  target_link_libraries(HumanRecognition PRIVATE Storage)
  target_link_libraries(CrossControl PRIVATE HumanRecognition)

  # 统一输出目录（与 Storage 模块类似），便于打包
  set(_hr_bin_dir "${CMAKE_BINARY_DIR}/bin")
  set(_hr_lib_dir "${CMAKE_BINARY_DIR}/lib")
  set_target_properties(
    HumanRecognition
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_hr_bin_dir}"
               LIBRARY_OUTPUT_DIRECTORY "${_hr_bin_dir}"
               ARCHIVE_OUTPUT_DIRECTORY "${_hr_lib_dir}")
endif()

install(
  TARGETS HumanRecognition
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime)
