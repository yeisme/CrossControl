# 用于目标、输出目录和安装规则的集中辅助函数

# 将输出目录设置为统一的 bin/lib 布局
function(cc_set_output_dirs target)
  set(_cc_bin_dir "${CMAKE_BINARY_DIR}/bin")
  set(_cc_lib_dir "${CMAKE_BINARY_DIR}/lib")
  set_target_properties(
    ${target}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_cc_bin_dir}"
               LIBRARY_OUTPUT_DIRECTORY "${_cc_bin_dir}"
               ARCHIVE_OUTPUT_DIRECTORY "${_cc_lib_dir}")
endfunction()

# Collect and install Qt runtime shared libraries (DLL/.so) into the install
# tree so they are included in the Runtime CPack component when
# ENABLE_BUNDLE_RUNTIME is ON. This inspects imported Qt targets for their
# imported locations and only installs files that actually exist.
function(cc_bundle_qt_runtime)
  # Install Qt runtime files (DLLs, plugins, translations) into the install
  # tree so they are picked up by CPack. Intended for non-QML Qt6 projects.
  if(NOT TARGET Qt6::Core)
    message(STATUS "cc_bundle_qt_runtime: Qt6::Core not found; skipping")
    return()
  endif()

  # Locate Qt bin directory from imported Qt6::Core
  get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
  if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
  endif()
  if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core LOCATION)
  endif()
  if(NOT _qt_core_loc)
    message(WARNING "cc_bundle_qt_runtime: cannot determine Qt6::Core location")
    return()
  endif()

  get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
  # Qt root usually one level above bin
  get_filename_component(_qt_root "${_qt_bin_dir}/.." ABSOLUTE)

  # Collect DLLs from Qt bin and install into bindir
  file(GLOB _qt_bin_dlls "${_qt_bin_dir}/Qt6*.dll" "${_qt_bin_dir}/*.dll")
  if(_qt_bin_dlls)
    install(FILES ${_qt_bin_dlls} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
  endif()

  # Common plugin categories to include
  set(_plugin_categories platforms imageformats styles iconengines multimedia sqldrivers)
  foreach(_cat IN LISTS _plugin_categories)
    set(_pdir "${_qt_root}/plugins/${_cat}")
    if(EXISTS "${_pdir}")
      install(DIRECTORY "${_pdir}" DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins" FILES_MATCHING PATTERN "*.dll" PATTERN "*.qm")
    endif()
    # some distributions put plugins under lib/plugins
    set(_pdir2 "${_qt_root}/lib/plugins/${_cat}")
    if(EXISTS "${_pdir2}")
      install(DIRECTORY "${_pdir2}" DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins" FILES_MATCHING PATTERN "*.dll" PATTERN "*.qm")
    endif()
  endforeach()

  # Translations
  set(_trans_dir "${_qt_root}/translations")
  if(EXISTS "${_trans_dir}")
    install(DIRECTORY "${_trans_dir}" DESTINATION ${CMAKE_INSTALL_BINDIR}/i18n FILES_MATCHING PATTERN "*.qm")
  endif()

  message(STATUS "cc_bundle_qt_runtime: Qt files scheduled for install from ${_qt_bin_dir}")
endfunction()

# 安装目标并指定 CPACK 组件
function(cc_install_target target component)
  if(NOT component)
    message(
      FATAL_ERROR
        "cc_install_target requires a component name: cc_install_target(<target> <component>)"
    )
  endif()
  # 安装运行时（exe/dll）、库文件（.so/.dll）和存档文件（.lib）。在 Windows 上，DLL
  # 是运行时组件的一部分，应放置在二进制目录中，以便 NSIS 组件清单将它们与可执行文件一起包含。在其他平台上，请将 LIBRARY 文件放在常规的
  # lib 目录中。
  if(WIN32)
    install(
      TARGETS ${target}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  else()
    install(
      TARGETS ${target}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  endif()
endfunction()

# 创建模块目标（遵循 BUILD_SHARED_MODULES；在 MSVC 下设置导出符号以便生成导入库）
function(cc_create_module target)
  if(ARGC LESS 2)
    message(
      FATAL_ERROR
        "cc_create_module requires at least a target name and one source: cc_create_module(<target> <sources...>)"
    )
  endif()
  if(BUILD_SHARED_MODULES)
    add_library(${target} SHARED ${ARGN})
    set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  else()
    add_library(${target} STATIC ${ARGN})
  endif()
endfunction()

# Deploy Qt runtime using windeployqt on Windows. This adds a POST_BUILD step to
# the given executable target which runs windeployqt to copy required Qt DLLs
# and plugins into the target's runtime directory. The function is a no-op on
# non-Windows platforms.
function(cc_deploy_qt_runtime target)
  # Deploy Qt runtime using platform tools. Simplified for non-QML projects.
  if(NOT TARGET ${target})
    message(WARNING "cc_deploy_qt_runtime: target '${target}' not found")
    return()
  endif()

  if(WIN32)
    # Locate windeployqt (prefer Qt bin adjacent to Qt6::Core)
    set(_windeployqt_exe)
    if(TARGET Qt6::Core)
      get_target_property(_qt_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
      if(NOT _qt_loc)
        get_target_property(_qt_loc Qt6::Core IMPORTED_LOCATION)
      endif()
      if(NOT _qt_loc)
        get_target_property(_qt_loc Qt6::Core LOCATION)
      endif()
      if(_qt_loc)
        get_filename_component(_qt_bin_dir "${_qt_loc}" DIRECTORY)
        find_program(_windeployqt_exe NAMES windeployqt.exe windeployqt PATHS "${_qt_bin_dir}" NO_DEFAULT_PATH)
      endif()
    endif()
    if(NOT _windeployqt_exe)
      find_program(_windeployqt_exe NAMES windeployqt.exe windeployqt)
    endif()

    if(NOT _windeployqt_exe)
      message(WARNING "cc_deploy_qt_runtime: windeployqt not found; skipping Qt deployment on Windows")
      return()
    endif()

    # Use generator expressions to select --debug for Debug configuration and
    # --release for other configurations. This ensures a Debug build of the
    # executable receives debug Qt DLLs (Qt6Multimediad.dll, etc.) and avoids
    # copying only release DLLs which would leave a debug exe missing symbols.
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${_windeployqt_exe}" $<$<CONFIG:Debug>:--debug> $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:--release> --dir "$<TARGET_FILE_DIR:${target}>" "$<TARGET_FILE:${target}>"
      COMMENT "windeployqt: deploying Qt runtime for ${target}")

  elseif(APPLE)
    find_program(_macdeployqt_exec NAMES macdeployqt)
    if(NOT _macdeployqt_exec)
      message(WARNING "cc_deploy_qt_runtime: macdeployqt not found; skipping Qt deployment on macOS")
      return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${_macdeployqt_exec}" "$<TARGET_FILE:${target}>"
      COMMENT "macdeployqt: deploying Qt runtime for ${target}")

  elseif(UNIX)
    find_program(_linuxdeployqt_exec NAMES linuxdeployqt)
    if(NOT _linuxdeployqt_exec)
      message(STATUS "cc_deploy_qt_runtime: linuxdeployqt not found; skipping Qt deployment on Linux")
      return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${_linuxdeployqt_exec}" "$<TARGET_FILE:${target}>"
      COMMENT "linuxdeployqt: deploying Qt runtime for ${target}")

  else()
    message(STATUS "cc_deploy_qt_runtime: unsupported platform")
  endif()
endfunction()
