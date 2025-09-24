# Output and MSVC specific settings for project-local targets
# 该文件负责：
# - 计算按配置区分的 bin/lib 目录
# - 为本项目目标（而非外部导入目标）设置输出目录
# - 在 MSVC 下为目标设置 VS 调试工作目录、/FS 编译选项以及每目标独立的 PDB 名称和输出目录

if(CMAKE_CONFIGURATION_TYPES)
  set(CONFIG_BIN_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/bin")
  set(CONFIG_LIB_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/lib")
else()
  set(CONFIG_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
  set(CONFIG_LIB_DIR "${CMAKE_BINARY_DIR}/lib")
endif()

# 只针对项目本地目标设置输出，避免对第三方导入目标施加属性导致重链接
set(_PROJECT_LOCAL_TARGETS CrossControl logging)
if(BUILD_HUMAN_RECOGNITION)
  list(APPEND _PROJECT_LOCAL_TARGETS HumanRecognition)
endif()
if(BUILD_AUDIO_VIDEO)
  list(APPEND _PROJECT_LOCAL_TARGETS AudioVideo)
endif()

if(BUILD_STORAGE)
  list(APPEND _PROJECT_LOCAL_TARGETS Storage)
endif()

foreach(_tgt IN LISTS _PROJECT_LOCAL_TARGETS)
  if(TARGET ${_tgt})
    get_target_property(_is_imported ${_tgt} IMPORTED)
    if(NOT _is_imported)
      set_target_properties(
        ${_tgt}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CONFIG_BIN_DIR}"
                   LIBRARY_OUTPUT_DIRECTORY "${CONFIG_BIN_DIR}"
                   ARCHIVE_OUTPUT_DIRECTORY "${CONFIG_LIB_DIR}")
      if(MSVC)
        # Visual Studio 会在生成时替换 $<CONFIG> 表达式
        set_target_properties(${_tgt} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
                                                 "${CONFIG_BIN_DIR}")
        # 当多个 cl.exe 并行编译时可能出现 PDB 写入竞争，添加 /FS 允许并发访问
        target_compile_options(${_tgt} PRIVATE /FS)
        # 为每个目标设置独立的 PDB 名称与输出目录，避免共享 vc140.pdb 导致冲突
        set_target_properties(${_tgt}
                              PROPERTIES
                                COMPILE_PDB_NAME "${_tgt}"
                                COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb")
      endif()
    endif()
  endif()
endforeach()
