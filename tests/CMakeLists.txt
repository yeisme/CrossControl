cmake_minimum_required(VERSION 3.24)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(HumanRecognitionTests humanrecognition/backend_impl_tests.cpp)

add_executable(CheckModelCompat humanrecognition/check_model_compat.cpp)
target_link_libraries(CheckModelCompat PRIVATE dlib::dlib Qt6::Core)
target_include_directories(CheckModelCompat PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
target_compile_definitions(CheckModelCompat PRIVATE HAS_DLIB=1 HAS_OPENCV=1)

target_link_libraries(
  HumanRecognitionTests
  PRIVATE GTest::gtest
          GTest::gtest_main
          HumanRecognition
          dlib::dlib
          Qt6::Core
          Qt6::Gui
          Qt6::Widgets
          Qt6::Sql)

target_include_directories(
  HumanRecognitionTests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})

target_compile_features(HumanRecognitionTests PRIVATE cxx_std_20)

target_compile_definitions(HumanRecognitionTests PRIVATE HAS_DLIB=1
                                                         HAS_OPENCV=1
                                                         PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set_target_properties(
  HumanRecognitionTests
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
             RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
             RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

gtest_discover_tests(
  HumanRecognitionTests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
                                          DISCOVERY_TIMEOUT 60)
