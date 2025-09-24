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
    install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/i18n COMPONENT Runtime)
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
