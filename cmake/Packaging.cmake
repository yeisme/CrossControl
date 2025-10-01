# Packaging 打包配置 可选项：控制是否将第三方运行时库（来自 vcpkg 或本地构建）打包到生成的安装程序中。 在使用 vcpkg manifest
# 模式进行开发时，通常不会将运行时库打包进安装程序（开发机器会通过 vcpkg 提供这些库）。 设置 -DENABLE_BUNDLE_RUNTIME=ON
# 可以生成包含第三方共享库的自包含安装程序。 为 CPack/NSIS 定义安装程序组件。每个组件对应一组已安装的目标和文件。 这些组件会在 NSIS
# 安装界面中展示给用户，并可以在安装时选择。 Core 应用程序始终为必选项 可选模块：默认的开/关状态取决于构建选项 以下模块为应用运行所必需，会与
# Core 应用组件一并安装（不会在安装程序中作为单独可选组件展示）： 将计算得到的组件列表暴露给 CPack。如果 CPACK_COMPONENTS_ALL
# 已经定义（例如在 ENABLE_BUNDLE_RUNTIME=ON 时包含 Runtime）， 我们将避免重复地追加组件，否则直接设置该变量。 在
# Linux（非 Apple）平台上，我们可能希望将第三方运行时库打包进生成的 .deb 中，而不是依赖发行版的 shlibdeps。 为此我们会禁用
# dpkg-shlibdeps（以防其基于系统库改写 Depends），并保留一个非常保守的 Depends 列表（如 libc）。 实际的运行时文件应由
# install 规则安装到 ${CMAKE_INSTALL_BINDIR}（见 cmake/Install.cmake），这样它们会被打包进 Debian
# 包中。 允许可选地从 NSIS 安装程序中排除某些运行时文件。CPACK_RUNTIME_EXCLUDE_PATTERN 是一个以分号分隔的全局模式列表，
# 相对于安装路径 ${CMAKE_INSTALL_BINDIR}，这些文件将在安装时从打包的运行时中被删除。默认情况下我们会移除 MSVC
# 可再发行安装程序及常见的 FFmpeg DLL 名称，这些通常会被 vcpkg 引入。 如果构建 NSIS 安装程序，在安装时注入额外的 NSIS
# 命令以删除运行时目录中匹配的文件。 这样可以保持安装程序体积小，并避免将可再发行安装程序嵌入到应用运行时中。 默认情况下确保包含 Runtime 文件
# 默认情况下从 Runtime 载荷中排除 MSVC 可再发行安装程序。当 ENABLE_BUNDLE_RUNTIME=OFF
# 时，这些模式通常无效，因为我们不会生成 Runtime 组件。 构建额外的 NSIS
# 命令以在安装时删除不需要的运行时文件。跳过任何空的列表项并去除空白，以避免生成没有参数的 'Delete' 导致 makensis 失败。
# 转义模式中的双引号（尽管不太可能出现）以保持 NSIS 语法有效。 对反斜杠和双引号进行转义，这样该值就可以安全地写入生成的
# CPackConfig.cmake，而不会产生无效的 CMake 字符串字面量（此前可能生成类似 Delete "$INSTDIR\bin..."
# 的未转义行并导致解析错误）。 确保系统运行时库被包含 Packaging
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "yefun2004@gmail.com")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

# 选项：控制是否将第三方运行时库（来自 vcpkg 或本地构建）捆绑到生成的安装程序中。在使用 vcpkg
# 清单模式开发时，通常不会将运行时库捆绑到安装程序中（它们由开发者机器上的 vcpkg 提供）。设置 -DENABLE_BUNDLE_RUNTIME=ON
# 可生成包含第三方共享库的自包含安装程序。
option(ENABLE_BUNDLE_RUNTIME
       "Bundle third-party runtime libraries into installers" OFF)

# Enable running platform deployment tools
# (windeployqt/macdeployqt/linuxdeployqt) after build. Default ON to make local
# development and debugging convenient.
option(
  ENABLE_DEPLOY_QT
  "Run Qt deployment tools (windeployqt / macdeployqt / linuxdeployqt) after build"
  ON)

if(WIN32)
  # Default Windows generators
  set(CPACK_GENERATOR "NSIS64;7Z")

  # Only include a Runtime component if we're bundling runtimes. In development
  # with vcpkg manifest mode we typically don't bundle runtime libraries into
  # installers, so avoid creating the Runtime component by default.
  if(ENABLE_BUNDLE_RUNTIME)
    set(CPACK_COMPONENTS_ALL Runtime)
  endif()
endif()

# 为 CPack/NSIS 定义安装程序组件。每个组件对应一组已安装的目标和文件。 这些组件会通过 NSIS 安装程序界面展示给用户，
# 并可以在安装时进行选择。
set(_cc_components)

# Core application is always required
list(APPEND _cc_components Core)

# Optional DeviceGateway component (MQTT gateway/back-end for devices)
list(APPEND _cc_components DeviceGateway)

if(BUILD_MQTT_CLIENT)
  list(APPEND _cc_components Connect)
  set(CPACK_COMPONENT_Connect_DISPLAY_NAME "Connect (MQTT/Network)")
  set(CPACK_COMPONENT_Connect_DESCRIPTION
      "Network connectivity plugins and connectors.")
  set(CPACK_COMPONENT_Connect_SELECTED TRUE)
endif()

# DeviceGateway optional component
set(CPACK_COMPONENT_DeviceGateway_DISPLAY_NAME "Device Gateway (MQTT Gateway)")
set(CPACK_COMPONENT_DeviceGateway_DESCRIPTION
    "Optional Device Gateway module providing MQTT device connectivity.")
set(CPACK_COMPONENT_DeviceGateway_SELECTED FALSE)

# The following modules are required for the application to run and are
# installed together with the Core application component (they are not presented
# as separate selectable components in the installer):
set(CPACK_COMPONENT_Core_DISPLAY_NAME "Core Application")
set(CPACK_COMPONENT_Core_DESCRIPTION
    "Main application plus required runtime modules (Storage, Logging, Config, HumanRecognition, AudioVideo)."
)
set(CPACK_COMPONENT_Core_REQUIRED TRUE)

# Expose the computed component list to CPack. If CPACK_COMPONENTS_ALL was
# already defined (for example Runtime when ENABLE_BUNDLE_RUNTIME=ON), append
# our components while avoiding duplicates. Otherwise set it directly.
if(DEFINED CPACK_COMPONENTS_ALL)
  foreach(_c IN LISTS _cc_components)
    list(FIND CPACK_COMPONENTS_ALL ${_c} _found)
    if(_found EQUAL -1)
      list(APPEND CPACK_COMPONENTS_ALL ${_c})
    endif()
  endforeach()
else()
  set(CPACK_COMPONENTS_ALL ${_cc_components})
endif()

# On Linux (non-Apple) we may want to bundle third-party runtime libraries
# inside the generated .deb instead of relying on the distribution's shlibdeps.
# To do that we disable dpkg-shlibdeps (so shlibdeps doesn't rewrite Depends
# based on system libs) and keep a very small conservative Depends list (libc).
# The actual runtime files are expected to be installed into
# ${CMAKE_INSTALL_BINDIR} by the install rules (see cmake/Install.cmake) so they
# will be packaged into the Debian payload.
if(UNIX AND NOT APPLE)
  # Use DEB as primary generator for Linux packaging
  set(CPACK_GENERATOR "DEB;TGZ")

  # Disable automatic shlibdeps resolution so the .deb will contain the bundled
  # shared libraries from the install tree instead of trying to reference
  # system-wide ones. This allows distributing a self-contained deb that
  # includes libraries from vcpkg / local build.
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

  # Conservative baseline dependency; adjust if you intentionally want the
  # package to depend on distro Qt packages instead of shipping them.
  if(NOT DEFINED CPACK_DEBIAN_PACKAGE_DEPENDS)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.29)")
  endif()

  # Ensure we include Runtime files by default
  if(NOT DEFINED CPACK_COMPONENTS_ALL)
    set(CPACK_COMPONENTS_ALL Runtime)
  endif()
endif()

# Enable component-based DEB packages so Connect can be produced as a separate
# Debian package when BUILD_MQTT_CLIENT is enabled. This makes DEB installers
# modular: core and optional Connect package(s).
if(UNIX AND NOT APPLE)
  # Tell CPack to generate one DEB per component instead of a single package
  set(CPACK_DEB_COMPONENT_INSTALL ON)

  if(BUILD_MQTT_CLIENT)
    # Configure the Connect component Debian package metadata
    set(CPACK_DEBIAN_Connect_PACKAGE_NAME "${PROJECT_NAME}-connect")
    set(CPACK_DEBIAN_Connect_PACKAGE_DESCRIPTION
        "${PROJECT_NAME} optional MQTT/connectivity components (Paho MQTT client libraries)")
    # By default, make the connect package suggest the core package; adjust
    # dependencies if you want a strict Depends relationship.
    set(CPACK_DEBIAN_Connect_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}")
  endif()
endif()

if(UNIX AND NOT APPLE AND BUILD_MQTT_CLIENT)
  # DeviceGateway DEB package metadata
  set(CPACK_DEBIAN_DeviceGateway_PACKAGE_NAME "${PROJECT_NAME}-devicegateway")
  set(CPACK_DEBIAN_DeviceGateway_PACKAGE_DESCRIPTION
      "${PROJECT_NAME} Device Gateway module (optional MQTT gateway functionality)")
  # DeviceGateway should depend on main package so it's clear core is required
  set(CPACK_DEBIAN_DeviceGateway_PACKAGE_DEPENDS "${PROJECT_NAME} (${PROJECT_VERSION})")
endif()

# 允许可选地从 NSIS 安装程序中排除某些运行时文件。CPACK_RUNTIME_EXCLUDE_PATTERN 是以分号分隔的全局模式列表，相对于安装路径
# ${CMAKE_INSTALL_BINDIR}，这些文件将在安装时从打包的运行时中删除。默认情况下，我们会移除 MSVC 可再发行安装程序以及常见的
# FFmpeg DLL 名称，这些通常会被 vcpkg 引入。
if(WIN32 AND NOT DEFINED CPACK_RUNTIME_EXCLUDE_PATTERN)
  # By default exclude MSVC redistributable installers from the Runtime payload.
  # When ENABLE_BUNDLE_RUNTIME=OFF these patterns are mostly irrelevant because
  # we won't produce a Runtime component.
  set(CPACK_RUNTIME_EXCLUDE_PATTERN "vc_redist*.exe")
endif()

# If building NSIS installer, inject extra NSIS commands to delete matching
# files from the Runtime directory at install-time. This keeps the installer
# small and avoids shipping redistributable installers embedded in the app
# runtime.
if(WIN32 AND CPACK_GENERATOR MATCHES "NSIS")
  # Build extra NSIS commands to delete unwanted runtime files at install-time.
  # Skip any empty list entries and strip whitespace to avoid emitting 'Delete'
  # with no parameter which causes makensis to fail.
  set(_nsis_extra_commands "")
  string(REPLACE "\\" "\\\\" _esc_bindir "${CMAKE_INSTALL_BINDIR}")
  foreach(_pat IN LISTS CPACK_RUNTIME_EXCLUDE_PATTERN)
    if(NOT DEFINED _pat)
      continue()
    endif()
    string(STRIP "${_pat}" _pat_stripped)
    if(_pat_stripped STREQUAL "")
      # skip empty/blank patterns
      continue()
    endif()
    # escape any double-quotes inside pattern (unlikely) to keep NSIS syntax
    # valid
    string(REPLACE "\"" "\\\"" _pat_escaped "${_pat_stripped}")
    set(_nsis_extra_commands
        "${_nsis_extra_commands}\n  Delete \"$INSTDIR\\${_esc_bindir}\\${_pat_escaped}\""
    )
  endforeach()

  if(NOT _nsis_extra_commands STREQUAL "")
    # Escape backslashes and double-quotes so the value can safely be written
    # into the generated CPackConfig.cmake without creating invalid CMake string
    # literals (which previously produced lines like: Delete "$INSTDIR\bin..."
    # that weren't escaped and caused parsing errors).
    string(REPLACE "\\" "\\\\" _nsis_extra_commands_esc_back
                   "${_nsis_extra_commands}")
    string(REPLACE "\"" "\\\"" _nsis_extra_commands_esc
                   "${_nsis_extra_commands_esc_back}")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${_nsis_extra_commands_esc}")
  else()
    # Ensure variable is unset when there are no extra commands to avoid
    # generating malformed NSIS script lines.
    unset(CPACK_NSIS_EXTRA_INSTALL_COMMANDS)
  endif()
endif()

# 确保系统运行时库被包含
include(InstallRequiredSystemLibraries)

# Debian/Ubuntu 打包额外设置： - Debian 包需要维护者字段；优先使用 CPACK_DEBIAN_PACKAGE_MAINTAINER，
# 如果未设置，则使用 CPACK_PACKAGE_CONTACT 作为默认值。 - 尝试启用
# shlibdeps（自动计算共享库依赖），以避免手工填写大量依赖。 如果目标系统没有 dpkg-shlibdeps，可通过设置
# CPACK_DEBIAN_PACKAGE_SHLIBDEPS=OFF 并显式指定 CPACK_DEBIAN_PACKAGE_DEPENDS 来覆盖。

if(NOT DEFINED CPACK_DEBIAN_PACKAGE_MAINTAINER)
  if(DEFINED CPACK_PACKAGE_CONTACT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
  else()
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Unknown <unknown@localhost>")
  endif()
endif()

# Enable automatic shared-library dependency resolution for DEB packages when
# dpkg-shlibdeps is available on the build host. This produces a realistic
# dependency list instead of leaving the package with no dependencies.
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_SHLIBDEPS)
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

# TODO: 待测试
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_DEPENDS)

  set(CPACK_DEBIAN_PACKAGE_DEPENDS
      "libc6 (>= 2.29), libqt6core6, libqt6widgets6, libqt6network6, libqt6sql6, libqt6multimedia6"
  )
endif()
# 确保放置在构建树的 bin/ 目录中的任何生成的运行时文件都记录在 Core 组件中，以便像 NSIS 这样的生成器能够将它们包含在包中。当部署工具（如
# windeployqt）在构建过程中将 DLL 放置在 ${CMAKE_BINARY_DIR}/bin 与生成的 EXE 同目录下时，这一点尤为重要。
if(WIN32)
  install(
    DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Core
    FILES_MATCHING
    PATTERN "*.exe"
    PATTERN "*.dll"
    PATTERN "*.pdb"
    # Exclude MQTT / Paho runtime DLLs from Core so they can be offered as a
    # separate optional component (Connect) in the installer when BUILD_MQTT_CLIENT is enabled.
    PATTERN "*paho*.dll" EXCLUDE
    PATTERN "*paho*.*" EXCLUDE)
endif()

if(WIN32 AND BUILD_MQTT_CLIENT)
  # Install any paho/mqtt runtime DLLs into the Connect component so NSIS
  # presents them as an optional feature. We intentionally match common
  # naming patterns; if your installed runtime uses different names adjust
  # the patterns accordingly.
  install(
    DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Connect
    FILES_MATCHING
    PATTERN "*paho*.dll"
    PATTERN "*paho*.pdb"
    PATTERN "*paho*.*")
endif()

# Ensure DeviceGateway's third-party runtime libraries (Drogon/Trantor) are
# packaged with the DeviceGateway component so the optional module can be
# installed standalone. Match common naming patterns for Windows DLLs and
# Unix shared objects. These installs intentionally mirror the Connect
# handling above but target the DeviceGateway component.
if(WIN32 AND TARGET DeviceGateway)
  install(
    DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT DeviceGateway
    FILES_MATCHING
    PATTERN "drogon*.dll"
    PATTERN "trantor*.dll"
    PATTERN "drogon*.pdb"
    PATTERN "trantor*.pdb")
endif()

if(UNIX AND NOT APPLE AND TARGET DeviceGateway)
  install(
    DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT DeviceGateway
    FILES_MATCHING
    PATTERN "libdrogon*.so"
    PATTERN "libdrogon*.so.*"
    PATTERN "libtrantor*.so"
    PATTERN "libtrantor*.so.*")
endif()

if(UNIX AND NOT APPLE AND BUILD_MQTT_CLIENT)
  # Install any libpaho shared libraries into the Connect component for DEB
  # packaging. Match soname and versioned shared objects (libpaho*.so*).
  install(
    DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT Connect
    FILES_MATCHING
    PATTERN "libpaho*.so"
    PATTERN "libpaho*.so.*"
  )
endif()

include(CPack)
