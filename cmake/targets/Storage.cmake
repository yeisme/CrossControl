## 存储模块
## Storage module
set(STORAGE_SOURCES
  src/modules/Storage/storage.cpp
  include/modules/Storage/storage.h
  src/modules/Storage/dbmanager.cpp
  include/modules/Storage/dbmanager.h)

cc_create_module(Storage ${STORAGE_SOURCES})

# 确保 Storage 的输出与其它项目本地目标使用相同的 bin/lib 布局
# 使用单配置路径；Output.cmake 通常会为其它目标处理这些，但在这里显式设置以避免缺失导入库的问题。
set(_storage_bin_dir "${CMAKE_BINARY_DIR}/bin")
set(_storage_lib_dir "${CMAKE_BINARY_DIR}/lib")

# 确保生成的头文件目录对使用者可用。使用 ${CMAKE_BINARY_DIR}/include 作为生成头文件的公共位置。
target_include_directories(Storage PUBLIC "${CMAKE_BINARY_DIR}/include")

# Link Storage against Qt Sql and logging. TinyORM is no longer required.
find_package(
  Qt6
  COMPONENTS Sql
  REQUIRED)
target_link_libraries(Storage PUBLIC Qt6::Sql logging config)
message(STATUS "Linking Storage module with Qt Sql and logging")

target_include_directories(Storage PUBLIC include/modules/Storage)
target_include_directories(Storage PUBLIC include/modules/Config)
target_link_libraries(CrossControl PUBLIC Storage)

cc_install_target(Storage Core)
