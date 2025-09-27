# Packaging
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "yefun2004@gmail.com")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

if(WIN32)
  # TODO: 当前打包仍然无法运行
  set(CPACK_GENERATOR "NSIS64;7Z")

  # 明确指定要打包的组件 (确保包含Runtime组件)
  set(CPACK_COMPONENTS_ALL Runtime)
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

# 允许可选地从 NSIS 安装程序中排除某些运行时文件。CPACK_RUNTIME_EXCLUDE_PATTERN 是以分号分隔的全局模式列表，相对于安装路径
# ${CMAKE_INSTALL_BINDIR}，这些文件将在安装时从打包的运行时中删除。默认情况下，我们会移除 MSVC 可再发行安装程序以及常见的
# FFmpeg DLL 名称，这些通常会被 vcpkg 引入。
if(WIN32 AND NOT DEFINED CPACK_RUNTIME_EXCLUDE_PATTERN)
  # By default we only exclude MSVC redistributable installers from the Runtime
  # payload (they should not be embedded in the application's runtime
  # directory). Do NOT exclude FFmpeg/OpenCV DLLs by default because many users
  # expect the installer to deploy these runtime libraries.
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

# If automatic shlibdeps is not suitable (cross-builds, missing dpkg tools),
# projects can still explicitly provide a dependency list by setting
# CPACK_DEBIAN_PACKAGE_DEPENDS. Provide a sensible conservative fallback so that
# packages built on systems without shlibdeps are still usable.
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_DEPENDS)
  # conservative baseline: libc and Qt6 runtime packages commonly needed by a
  # Qt6/OpenCV application. Adjust or override for your distribution as needed.
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
      "libc6 (>= 2.29), libqt6core6, libqt6widgets6, libqt6network6, libqt6sql6, libqt6multimedia6"
  )
endif()
include(CPack)
