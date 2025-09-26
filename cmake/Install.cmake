# Installation
include(GNUInstallDirs)
install(
  TARGETS CrossControl
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

if(QT_VERSION_MAJOR EQUAL 6)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
  if(WINDEPLOYQT_EXECUTABLE)
    install(
      CODE "
            execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} --dir ${CMAKE_BINARY_DIR}/windeployqt_output \${CMAKE_INSTALL_PREFIX}/bin/CrossControl.exe)
        ")
    install(
      DIRECTORY ${CMAKE_BINARY_DIR}/windeployqt_output/
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT Runtime)
  endif()

  # Install translation files
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

# 安装常用的第三方运行时 DLL（例如 vcpkg 提供的库）。
# 这确保了非系统运行时 DLL，如 spdlog/fmt，被复制到安装树中，从而被 CPack 包含。
# 使用生成器表达式使得无论目标是导入的还是在树内构建的，都能正常工作。
# OPTIONAL 防止在某些配置中目标不可用时导致配置失败。
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
