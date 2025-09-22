# Dependencies
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS CoreTools Widgets
                                                  LinguistTools Network Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS CoreTools Widgets
                                                       LinguistTools Network Multimedia)

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

if(DEFINED ENV{VCPKG_ROOT})
  message(STATUS "Using VCPKG from environment variable: $ENV{VCPKG_ROOT}")
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")

  set(OpenCV_ROOT "${VCPKG_ROOT}/installed/x64-windows/share/opencv4")
  find_package(OpenCV REQUIRED)
else()
  message(STATUS "VCPKG_ROOT environment variable is not set.")
endif()
