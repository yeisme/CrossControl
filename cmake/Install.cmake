# 安装 - 组织成多个小函数以提高可维护性
include(GNUInstallDirs)

# 基本 install：程序目标 每个目标的 install 规则保存在对应的 cmake/targets/*.cmake 文件中。 这样可以让模块级别的
# install() 与目标定义保持在同一处，并为每个目标分配 CPACK 组件。

include(${CMAKE_SOURCE_DIR}/cmake/Helper.cmake)

# 可选择将第三方运行时库（DLL/.so）捆绑到安装目录中。当 ENABLE_BUNDLE_RUNTIME 关闭（默认）时，我们会避免复制 vcpkg
# 提供的运行时工件；启用它可创建自包含安装程序。
if(ENABLE_BUNDLE_RUNTIME)
  message(
    STATUS
      "ENABLE_BUNDLE_RUNTIME=ON: bundling third-party runtime libraries into install tree"
  )
  # Bundle Qt runtime files (DLL/.so) into the install tree so they are
  # packaged into the Runtime CPack component. Helper function will only
  # install files that exist for the imported Qt targets.
  cc_bundle_qt_runtime()
  # 示例：在 Windows 上，我们可以将所需的 DLL 模式复制到 ${CMAKE_INSTALL_BINDIR}（作为一个挂钩 — 项目可以通过设置
  # CPACK_RUNTIME_EXCLUDE_PATTERN 来排除文件）。 如果需要，可以在此处添加 copy 命令或额外的 install(FILES
  # ...) 命令。
else()
  message(
    STATUS
      "ENABLE_BUNDLE_RUNTIME=OFF: not bundling vcpkg / third-party runtime libraries (developer mode)"
  )
endif()
