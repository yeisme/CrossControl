# 安装 - 组织成多个小函数以提高可维护性
include(GNUInstallDirs)

# 基本 install：程序目标
install(
  TARGETS CrossControl
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# ----------------------------------------------------------------------------
# 选项：windeployqt 行为控制
# ----------------------------------------------------------------------------
option(ENABLE_WINDEPLOYQT_INSTALL "Run windeployqt during install" ON)
option(INSTALL_WINDEPLOYQT_OUTPUT_FULL
       "Install the full windeployqt_output directory (no exclusions)" OFF)
option(INSTALL_WINDEPLOYQT_EXCLUDE_PDB
       "Exclude .pdb files from windeployqt output installation" ON)
option(
  INSTALL_WINDEPLOYQT_EXCLUDE_VC_REDIST
  "Exclude MSVC redist (vc_redist*.exe) from windeployqt output installation"
  ON)

# ----------------------------------------------------------------------------
# Helpers: windeployqt
# ----------------------------------------------------------------------------
function(cc_find_windeployqt OUT_VAR)
  # 尝试常见位置和环境变量以提高命中率
  find_program(
    _cc_windeployqt
    NAMES windeployqt.exe windeployqt
    HINTS ${Qt6_DIR}/../../../bin ${Qt6_DIR}/bin $ENV{QTDIR}/bin)
  if(_cc_windeployqt)
    set(${OUT_VAR}
        "${_cc_windeployqt}"
        PARENT_SCOPE)
  else()
    set(${OUT_VAR}
        ""
        PARENT_SCOPE)
  endif()
endfunction()

function(cc_install_windeployqt_run WINDEPLOYQT_EXECUTABLE)
  if(NOT WINDEPLOYQT_EXECUTABLE)
    install(CODE "message(STATUS \"windeployqt not found; skipping run.\")")
    return()
  endif()

  # 在 install 时运行 windeployqt。执行结果在 install 时检查并报告。
  install(
    CODE "message(STATUS \"windeployqt found: ${WINDEPLOYQT_EXECUTABLE}; running to gather Qt runtimes before other install steps.\")"
  )
  install(
    CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --dir \"${CMAKE_BINARY_DIR}/windeployqt_output\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/CrossControl.exe\" RESULT_VARIABLE _cc_windeployqt_rv OUTPUT_QUIET ERROR_QUIET)"
  )
  install(
    CODE "
if(NOT \${_cc_windeployqt_rv} EQUAL 0)
  message(WARNING \"windeployqt returned non-zero exit code: \${_cc_windeployqt_rv}\")
endif()")
endfunction()

function(cc_install_windeployqt_output)
  # 仅当用户允许将 windeployqt 输出包含到安装中时执行复制操作
  if(NOT ENABLE_WINDEPLOYQT_INSTALL)
    return()
  endif()

  if(NOT EXISTS "${CMAKE_BINARY_DIR}/windeployqt_output")
    install(
      CODE "message(STATUS \"windeployqt_output directory does not exist; skipping copy to install tree.\")"
    )
    return()
  endif()

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
endfunction()

# ----------------------------------------------------------------------------
# Helpers: translations (QM files)
# ----------------------------------------------------------------------------
function(cc_install_qm_files)
  if(QM_FILES)
    install(
      FILES ${QM_FILES}
      DESTINATION ${CMAKE_INSTALL_BINDIR}/i18n
      COMPONENT Runtime)
  endif()
endfunction()

# ----------------------------------------------------------------------------
# Helpers: MinGW runtime copy
# ----------------------------------------------------------------------------
function(cc_install_mingw_runtimes)
  if(MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    install(
      FILES ${MINGW_BIN_DIR}/libc++.dll ${MINGW_BIN_DIR}/libunwind.dll
            ${MINGW_BIN_DIR}/libwinpthread-1.dll
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT Runtime)
  endif()
endfunction()

# ----------------------------------------------------------------------------
# Helpers: third-party runtime DLLs (spdlog, fmt)
# ----------------------------------------------------------------------------
function(cc_install_thirdparty_dll TARGET_NAME)
  if(TARGET ${TARGET_NAME})
    install(
      FILES $<TARGET_FILE:${TARGET_NAME}>
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT Runtime
      OPTIONAL)
  else()
    message(
      STATUS
        "${TARGET_NAME} target not available at install-time; skipping explicit DLL install"
    )
  endif()
endfunction()

# ----------------------------------------------------------------------------
# Helpers: OpenCV runtime installation (imported targets first, then fallbacks)
# ----------------------------------------------------------------------------
function(cc_install_opencv_runtime)
  if(NOT
     (BUILD_HUMAN_RECOGNITION
      AND DEFINED OpenCV_FOUND
      AND OpenCV_FOUND))
    return()
  endif()

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

  # 回退：基于 OpenCV_DIR / VCPKG_ROOT 查找 bin 目录并复制匹配的共享库
  if(NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "")
  endif()

  set(_opencv_bin_candidates "")
  if(OpenCV_DIR)
    get_filename_component(_od_parent "${OpenCV_DIR}" DIRECTORY)
    list(APPEND _opencv_bin_candidates "${_od_parent}/bin"
         "${OpenCV_DIR}/../bin" "${OpenCV_DIR}/../../bin")
  endif()

  if(DEFINED ENV{VCPKG_ROOT})
    if(WIN32)
      list(APPEND _opencv_bin_candidates
           "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
    elseif(UNIX)
      list(APPEND _opencv_bin_candidates
           "$ENV{VCPKG_ROOT}/installed/x64-linux/bin")
    endif()
  endif()

  foreach(_cand IN LISTS _opencv_bin_candidates)
    if(EXISTS "${_cand}")
      install(
        DIRECTORY "${_cand}/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
        OPTIONAL FILES_MATCHING
        PATTERN "*opencv*.dll"
        PATTERN "*opencv*.so"
        PATTERN "*opencv*.dylib"
        # common additional libs sometimes colocated with OpenCV
        PATTERN "*sharpyuv*.dll"
        PATTERN "*libsharpyuv*.so*"
        PATTERN "*openjp2*.so*"
        PATTERN "*openjpeg*.so*"
        PATTERN "*szip*.so*"
        PATTERN "*tesseract*.so*"
        PATTERN "*absl*.so*"
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
        PATTERN "*webp*.dll"
        PATTERN "*gif*.dll"
        PATTERN "*leptonica*.dll"
        PATTERN "*leptonica-*.dll"
        PATTERN "*leptonica*.so*")

      if(DEFINED ENV{VCPKG_ROOT} AND WIN32)
        set(_vcpkg_bin "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
        if(EXISTS "${_vcpkg_bin}")
          file(GLOB _webp_dlls "${_vcpkg_bin}/libwebp*.dll"
               "${_vcpkg_bin}/webp*.dll" "${_vcpkg_bin}/libwebpdecoder*.dll"
               "${_vcpkg_bin}/libwebpdemux*.dll")
          if(_webp_dlls)
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

      # 额外：显式包含常见的 OpenCV 运行时依赖（例如 tiff, hdf5, harfbuzz, abseil 等），以防这些库 分开存放于
      # vcpkg 或其它 bin 目录，未被上面的 PATTERN 捕获到。
      set(_extra_dep_patterns
          "*tiff*.dll"
          "*libtiff*.dll"
          "*hdf5*.dll"
          "*harfbuzz*.dll"
          "*abseil*.dll"
          "*absl*.dll"
          "*openjp2*.dll"
          "*openjpeg*.dll"
          "*szip*.dll"
          "*libszip*.dll"
          "*tesseract*.dll"
          "*libtesseract*.dll"
          "*tesseract55*.dll"
          "*sharpyuv*.dll"
          "*libsharpyuv*.dll"
          # FFmpeg and multimedia related runtimes
          "*avcodec*.dll"
          "*avformat*.dll"
          "*avutil*.dll"
          "*swresample*.dll"
          "*swscale*.dll"
          "*libcrypto*.dll"
          "*libssl*.dll"
          "*libcurl*.dll"
          # Linux/Unix .so patterns
          "*tiff*.so*"
          "*libtiff*.so*"
          "*hdf5*.so*"
          "*harfbuzz*.so*"
          "*abseil*.so*"
          "*absl*.so*"
          "*openjp2*.so*"
          "*openjpeg*.so*"
          "*szip*.so*"
          "*libszip*.so*"
          "*gif*.so*"
          "*tesseract*.so*"
          "*libtesseract*.so*"
          "*libsharpyuv*.so*")
      set(_extra_deps_found "")
      foreach(_pat IN LISTS _extra_dep_patterns)
        # expand pattern variable when globbing the candidate directory
        file(GLOB _matches "${_cand}/${_pat}")
        if(_matches)
          foreach(_m IN LISTS _matches)
            string(REPLACE "\\" "/" _m_norm "${_m}")
            list(APPEND _extra_deps_found "${_m_norm}")
          endforeach()
        endif()
      endforeach()

      if(DEFINED ENV{VCPKG_ROOT} AND WIN32)
        # check both windows and linux vcpkg bin directories for common deps
        set(_vcpkg_bin_win "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
        set(_vcpkg_bin_linux "$ENV{VCPKG_ROOT}/installed/x64-linux/bin")
        set(_vcpkg_globs "")
        list(
          APPEND
          _vcpkg_globs
          "${_vcpkg_bin}/tiff*.dll"
          "${_vcpkg_bin}/libtiff*.dll"
          "${_vcpkg_bin}/hdf5*.dll"
          "${_vcpkg_bin}/harfbuzz*.dll"
          "${_vcpkg_bin}/abseil*.dll"
          "${_vcpkg_bin}/absl*.dll"
          "${_vcpkg_bin}/openjp2*.dll"
          "${_vcpkg_bin}/openjpeg*.dll"
          "${_vcpkg_bin}/szip*.dll"
          "${_vcpkg_bin}/libszip*.dll"
          "${_vcpkg_bin}/tesseract*.dll"
          "${_vcpkg_bin}/libtesseract*.dll"
          "${_vcpkg_bin}/tesseract55*.dll"
          "${_vcpkg_bin}/sharpyuv*.dll"
          "${_vcpkg_bin}/libsharpyuv*.dll")
        list(APPEND _vcpkg_globs "${_vcpkg_bin}/leptonica*.dll"
             "${_vcpkg_bin}/libleptonica*.dll" "${_vcpkg_bin}/leptonica-*.dll")
        if(EXISTS "${_vcpkg_bin_linux}")
          list(
            APPEND
            _vcpkg_globs
            "${_vcpkg_bin_linux}/tiff*.so*"
            "${_vcpkg_bin_linux}/libtiff*.so*"
            "${_vcpkg_bin_linux}/hdf5*.so*"
            "${_vcpkg_bin_linux}/harfbuzz*.so*"
            "${_vcpkg_bin_linux}/absl*.so*"
            "${_vcpkg_bin_linux}/openjp2*.so*"
            "${_vcpkg_bin_linux}/openjpeg*.so*"
            "${_vcpkg_bin_linux}/szip*.so*"
            "${_vcpkg_bin_linux}/libszip*.so*"
            "${_vcpkg_bin_linux}/tesseract*.so*"
            "${_vcpkg_bin_linux}/libtesseract*.so*"
            "${_vcpkg_bin_linux}/libsharpyuv*.so*")
          list(APPEND _vcpkg_globs "${_vcpkg_bin_linux}/leptonica*.so*"
               "${_vcpkg_bin_linux}/libleptonica*.so*")
        endif()
        file(GLOB _vcpkg_extra ${_vcpkg_globs})
        if(_vcpkg_extra)
          foreach(_p IN LISTS _vcpkg_extra)
            string(REPLACE "\\" "/" _p_norm "${_p}")
            list(APPEND _extra_deps_found "${_p_norm}")
          endforeach()
        endif()
      endif()

      if(_extra_deps_found)
        # 去重
        list(REMOVE_DUPLICATES _extra_deps_found)
        install(
          FILES ${_extra_deps_found}
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT Runtime
          OPTIONAL)
      endif()

      break()
    endif()
  endforeach()
endfunction()

# ----------------------------------------------------------------------------
# 执行各模块（按功能顺序，减少嵌套）
# ----------------------------------------------------------------------------

if(QT_VERSION_MAJOR EQUAL 6)
  # 仅在 Windows 上才运行 windeployqt（windeployqt 是 Windows 工具），避免在 Unix 系统上触发错误
  if(WIN32)
    # 查找并在 install 时运行 windeployqt（若可用）以收集 Qt 运行时
    cc_find_windeployqt(_CC_WINDEPLOYQT)
    # 使用带引号的参数传递，确保即使变量为空也作为单个参数传递给函数，避免 CMake 参数解析问题
    cc_install_windeployqt_run("${_CC_WINDEPLOYQT}")

    # 将 windeployqt 的输出并入安装（受 ENABLE_WINDEPLOYQT_INSTALL 等选项控制）
    cc_install_windeployqt_output()
  else()
    install(
      CODE "message(STATUS \"Non-Windows system detected; skipping windeployqt run and output merge.\")"
    )
  endif()

  # 安装翻译文件（跨平台）
  cc_install_qm_files()
endif()

# MinGW 特定 runtime
cc_install_mingw_runtimes()

# 第三方运行时（spdlog, fmt）
cc_install_thirdparty_dll(spdlog::spdlog)
cc_install_thirdparty_dll(fmt::fmt)

# OpenCV 运行时（HumanRecognition 模块回退逻辑）
cc_install_opencv_runtime()

# 无论是否运行 windeployqt，都尝试通过安装我们依赖的 Qt6 目标文件来确保必要的 Qt 运行时库存在。
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
