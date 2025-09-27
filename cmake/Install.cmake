# 安装
include(GNUInstallDirs)
install(
  TARGETS CrossControl
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# 控制 windeployqt 以及其输出中哪些内容会被安装。默认情况下（如果可用）会运行 windeployqt， 但会避免将 MSVC
# 可再发行安装程序和调试符号 (.pdb) 文件复制到安装目录中。
option(ENABLE_WINDEPLOYQT_INSTALL "Run windeployqt during install" ON)
option(INSTALL_WINDEPLOYQT_OUTPUT_FULL
       "Install the full windeployqt_output directory (no exclusions)" OFF)
option(INSTALL_WINDEPLOYQT_EXCLUDE_PDB
       "Exclude .pdb files from windeployqt output installation" ON)
option(
  INSTALL_WINDEPLOYQT_EXCLUDE_VC_REDIST
  "Exclude MSVC redist (vc_redist*.exe) from windeployqt output installation"
  ON)

if(QT_VERSION_MAJOR EQUAL 6)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
  if(WINDEPLOYQT_EXECUTABLE AND ENABLE_WINDEPLOYQT_INSTALL)
    # 运行 windeployqt，将 Qt 运行时文件收集到安装前缀附近的目录中。
    install(
      CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} --dir ${CMAKE_BINARY_DIR}/windeployqt_output \${CMAKE_INSTALL_PREFIX}/bin/CrossControl.exe)"
    )

    # 安装 windeployqt 的输出。默认会排除已知的大文件或不希望包含的文件 （如 vc_redist*.exe，以及可选的 .pdb），以避免把
    # MSVC 可再发行安装程序 和调试符号包含到运行时包中。如果需要原始行为，可设置
    # INSTALL_WINDEPLOYQT_OUTPUT_FULL=ON。
    if(INSTALL_WINDEPLOYQT_OUTPUT_FULL)
      install(
        DIRECTORY ${CMAKE_BINARY_DIR}/windeployqt_output/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime)
    else()
      install(
        DIRECTORY ${CMAKE_BINARY_DIR}/windeployqt_output/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
        PATTERN "vc_redist*.exe" EXCLUDE
        PATTERN "*~" EXCLUDE
        PATTERN "*.pdb" EXCLUDE)

      # 额外安全检查：在安装时尝试删除通过其它机制（如 InstallRequiredSystemLibraries） 放到安装前缀中的任何
      # vc_redist 文件。
      if(INSTALL_WINDEPLOYQT_EXCLUDE_VC_REDIST)
        install(
          CODE "
          file(GLOB __cc_vc_redist \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/vc_redist*.exe\")
          foreach(_cc_vc \${__cc_vc_redist})
            if(EXISTS \${_cc_vc})
              file(REMOVE \${_cc_vc})
            endif()
          endforeach()")
      endif()

      # 可选：移除 .pdb 文件（调试符号）以减小包体积。
      if(INSTALL_WINDEPLOYQT_EXCLUDE_PDB)
        install(
          CODE "
          file(GLOB __cc_pdbs \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/*.pdb\")
          foreach(_cc_pdb \${__cc_pdbs})
            if(EXISTS \${_cc_pdb})
              file(REMOVE \${_cc_pdb})
            endif()
          endforeach()")
      endif()
    endif()
  endif()

  # 安装翻译文件
  if(QM_FILES)
    install(
      FILES ${QM_FILES}
      DESTINATION ${CMAKE_INSTALL_BINDIR}/i18n
      COMPONENT Runtime)
  endif()
endif()

if(MINGW)
  get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
  install(
    FILES ${MINGW_BIN_DIR}/libc++.dll ${MINGW_BIN_DIR}/libunwind.dll
          ${MINGW_BIN_DIR}/libwinpthread-1.dll
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime)
endif()

# 安装常用的第三方运行时 DLL（例如 vcpkg 提供的库）。这确保非系统运行时 的 DLL（如 spdlog/fmt）会被复制到安装树，从而被 CPack
# 打包。使用生成器表达式 可确保无论目标是导入目标还是在同一项目内构建的目标都能正确解析。OPTIONAL 选项避免在某些配置下目标不可用导致配置失败。
if(TARGET spdlog::spdlog)
  install(
    FILES $<TARGET_FILE:spdlog::spdlog>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
    OPTIONAL)
else()
  message(
    STATUS
      "spdlog target not available at install-time; skipping explicit spdlog DLL install"
  )
endif()

if(TARGET fmt::fmt)
  install(
    FILES $<TARGET_FILE:fmt::fmt>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
    OPTIONAL)
else()
  message(
    STATUS
      "fmt target not available at install-time; skipping explicit fmt DLL install"
  )
endif()

# 当启用 HumanRecognition 模块且找到 OpenCV 时，安装 OpenCV 的运行时库（DLL / 共享对象）。
# 优先使用导入目标（imported targets），这样无需硬编码库文件名。作为回退，会尝试 基于 OpenCV_DIR 或 vcpkg 路径的一些常见
# bin 目录。
if(BUILD_HUMAN_RECOGNITION
   AND DEFINED OpenCV_FOUND
   AND OpenCV_FOUND)
  if(DEFINED OpenCV_LIBS)
    foreach(_oclib IN LISTS OpenCV_LIBS)
      if(TARGET ${_oclib})
        install(
          FILES $<TARGET_FILE:${_oclib}>
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT Runtime
          OPTIONAL)
      endif()
    endforeach()
  endif()

  # 回退方案：如果没有通过导入目标安装文件，则尝试从可能的 OpenCV bin 目录 （vcpkg 或系统路径）复制共享库。根据 OpenCV_DIR 和
  # VCPKG_ROOT 计算候选 bin 目录。
  if(NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "")
  endif()

  set(_opencv_bin_candidates "")
  if(OpenCV_DIR)
    # OpenCV_DIR often looks like <prefix>/lib/cmake/opencv4 or
    # <prefix>/share/opencv4 Try a couple of relative locations to reach the
    # 'bin' directory.
    get_filename_component(_od_parent "${OpenCV_DIR}" DIRECTORY)
    list(APPEND _opencv_bin_candidates "${_od_parent}/bin"
         "${OpenCV_DIR}/../bin" "${OpenCV_DIR}/../../bin")
  endif()

  if(DEFINED ENV{VCPKG_ROOT})
    # vcpkg usually places runtime DLLs in installed/<triplet>/bin
    if(WIN32)
      list(APPEND _opencv_bin_candidates
           "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
    elseif(UNIX)
      list(APPEND _opencv_bin_candidates
           "$ENV{VCPKG_ROOT}/installed/x64-linux/bin")
    endif()
  endif()

  # Try candidates and install matching shared libs
  foreach(_cand IN LISTS _opencv_bin_candidates)
    if(EXISTS "${_cand}")
      # 仅复制与 OpenCV 相关的共享库，避免打包不相关的二进制文件。
      # Copy OpenCV-related runtime files from the candidate bin directory.
      install(
        DIRECTORY "${_cand}/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
        OPTIONAL FILES_MATCHING
        # OpenCV runtime libs
        PATTERN "*opencv*.dll"
        PATTERN "*opencv*.so"
        PATTERN "*opencv*.dylib"
        # 常见的 OpenCV 第三方依赖可能和 OpenCV DLL 放在同一目录（如 protobuf、jpeg、zlib、tbb、webp
        # 等）。 这些 PATTERN 是回退方案；当 CMake 版本支持时，我们会在下方尝试更稳健的运行时依赖解析。
        PATTERN "*protobuf*.dll"
        PATTERN "*libprotobuf*.dll"
        PATTERN "*jpeg*.dll"
        PATTERN "*jpeg62*.dll"
        PATTERN "*turbojpeg*.dll"
        PATTERN "*zlib*.dll"
        PATTERN "*zlib1*.dll"
        PATTERN "*png*.dll"
        PATTERN "*tbb*.dll"
        PATTERN "*ipp*.dll"
        PATTERN "*webp*.dll")

      # 试图解析并安装 OpenCV DLL 的实际二进制运行时依赖（如 libwebp*、libprotobuf、jpeg 等）。 原先计划使用
      # file(GET_RUNTIME_DEPENDENCIES) 来检查 DLL 并返回解析出的依赖路径，
      # 这种方式比基于文件名模式更准确。但由于该命令在部分环境中不可用或行为不一致， 所以这里也保留了基于路径/模式的回退机制。
      # 作为稳健回退（并且考虑到 file(GET_RUNTIME_DEPENDENCIES) 在某些环境中可能不可用或行为不一致）， 如果存在则显式从
      # vcpkg 的 bin 目录安装常见的 webp 相关 DLL。上面的 PATTERN "*webp*.dll" 在 DLL 与 OpenCV
      # 放在同一目录时通常能捕获它们；但当 vcpkg 将这些库放在单独的 bin 目录时， 这个额外步骤可以确保 libwebp* 文件被包含。
      if(DEFINED ENV{VCPKG_ROOT} AND WIN32)
        set(_vcpkg_bin "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
        if(EXISTS "${_vcpkg_bin}")
          file(GLOB _webp_dlls "${_vcpkg_bin}/libwebp*.dll"
               "${_vcpkg_bin}/webp*.dll" "${_vcpkg_bin}/libwebpdecoder*.dll"
               "${_vcpkg_bin}/libwebpdemux*.dll")
          if(_webp_dlls)
            # Normalize paths to use forward-slashes to avoid generating
            # unescaped backslashes in the generated cmake_install.cmake (which
            # causes warnings about invalid escape sequences like \U).
            set(_webp_dlls_normalized "")
            foreach(_p IN LISTS _webp_dlls)
              string(REPLACE "\\" "/" _p_norm "${_p}")
              list(APPEND _webp_dlls_normalized "${_p_norm}")
            endforeach()
            install(
              FILES ${_webp_dlls_normalized}
              DESTINATION ${CMAKE_INSTALL_BINDIR}
              COMPONENT Runtime
              OPTIONAL)
          endif()
        endif()
      endif()

      break()
    endif()
  endforeach()
endif()

# 无论是否运行 windeployqt，都尝试通过安装我们依赖的 Qt6 目标文件来确保必要的 Qt 运行时库存在。
# 使用 OPTIONAL 防止在某些环境下未找到导入目标时导致配置失败。
if(QT_VERSION_MAJOR EQUAL 6)
  foreach(_qtcomp IN ITEMS Core Widgets Network Sql Multimedia)
    if(TARGET Qt6::${_qtcomp})
      install(
        FILES $<TARGET_FILE:Qt6::${_qtcomp}>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
        OPTIONAL)
    endif()
  endforeach()
endif()
