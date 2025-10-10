# CrossControl

、

## 简介

CrossControl 是一个跨平台的桌面应用，旨在提供多功能的设备管理与控制界面。它集成了人脸识别技术，支持通过摄像头进行用户身份验证和交互。此外，应用还具备网络通信能力，能够通过 TCP/UDP/HTTP 协议与其他设备进行数据交换。日志系统确保所有操作都有迹可循，而导入/导出功能则方便用户备份和恢复设置。整体设计注重用户体验，界面友好且易于操作。

本仓库使用 CMake 构建，主要基于 Qt 提供 UI 与多媒体支持，并集成 dlib 作为可选的人脸识别依赖。仓库内包含若干 CMake preset、打包（CPack）配置与辅助脚本，便于在 Windows（MSVC）与 Linux（gcc/clang）上构建与分发。

## 主要特性

- Qt 桌面 UI（使用 Qt Widgets 与多媒体模块）。
- 人脸识别（基于 dlib，可选，提供模型下载脚本）。
- 网络操作（TCP/UDP/HTTP）、设备管理、日志系统与导出/导入功能。
- CMake + CPack 支持多平台构建与打包。

## 核心页面

- 主界面：显示设备状态、日志与控制选项。
- 人脸识别界面：通过摄像头捕捉并识别人脸。
- 设备管理界面：添加、删除与配置受控设备。
- 设备操作界面：执行设备相关操作与命令（TCP/UDP/HTTP/串口/MQTT 等）。
- 日志查看界面：显示操作日志与系统信息。
- 设置界面：配置应用参数与选项。
- 登录/注册界面：用户身份验证与管理。

## 依赖

- CMake 3.25+（推荐使用与预置匹配的版本）
- Qt 6（建议 6.8+，项目使用 Qt Multimedia 等模块；参见 `CMakeLists.txt` 中的具体模块）
- 需要使用支持 C++20 的编译器
- （可选）dlib 与其依赖（若启用人脸识别模块）
- Bash / PowerShell（用于运行仓库内的辅助脚本，例如模型下载）

仓库内包含 `CMakePresets.json` 和 `CMakeUserPresets.json`，已配置常用的构建预置（例如 `msvc-debug`、`msvc-release`）。

## 快速开始 — Windows (MSVC)

推荐在已安装 Qt 与 MSVC 的环境中使用。仓库提供了 CMake preset，可以直接使用 CMake 或 VS Code 的 CMake 插件/任务。

1. 打开 PowerShell（或在 VS Code 中打开本项目）。
2. 配置（使用 preset）：

```powershell
cmake --preset msvc-debug
```

3. 构建：

```powershell
cmake --build --preset msvc-debug
```

或者使用 VS Code 提供的任务：

- `CMake: Configure  MSVC 2022 Debug`
- `CMake: Build  MSVC 2022 Debug`

构建产物默认位于 `build/msvc-debug/`（二进制在 `build/msvc-debug/bin/`）。

在 Windows 平台上，gcc 和 clang 未经测试，建议使用 MSVC。

## 快速开始 — Linux (gcc/clang)

在 Linux 下可以使用相应的 preset 或典型的 CMake 流程：

使用 `cmake --preset <preset-name>`（`gcc-release` 或 `clang-release`）。

```bash
cmake --preset gcc-release
```

```bash
cmake --build --preset gcc-release
```

之后使用对于的 build preset 进行构建，例如：

```bash
cmake --build --preset gcc-release
```

```bash
cmake --build --preset clang-release
```

## 启用/安装 dlib 与人脸识别模型

项目将 dlib 放在 `third_party/dlib/`，并提供安装脚本：

- Windows (PowerShell)：

```powershell
.
# 运行仓库脚本来安装 dlib（示例）
pwsh ./scripts/install_dlib.ps1
# 下载 dlib 所需的人脸模型
pwsh ./scripts/download_dlib_model.ps1
```

- Linux (bash)：

```bash
./scripts/install_dlib.sh
./scripts/download_dlib_model.sh
```

请根据 `scripts/` 下的脚本与 `install_dlib.json` 指示调整命令和选项。

## 运行与调试

- 可在 VS Code 或 Qt Creator 中打开工程（推荐使用 CMake 支持的 IDE）。
- 可在启动项中设置断点并运行 `build/.../bin/CrossControl` 或相应可执行文件。

## 打包

项目提供 CPack 预置（见 workspace tasks）：

- 创建 MSVC 发布包（使用已有 preset）：

```powershell
cpack --preset msvc-release-package
```

- 其它可用 preset：`clang-release-package`, `gcc-release-package`（视平台而定）。

打包配置位于 `cmake/Packaging.cmake` 与顶层 `CMakeLists.txt` 的 CPack 章节。

## 代码格式化与检查

仓库提供格式化脚本（`scripts/format.ps1` 和 `scripts/format.sh`）。使用示例：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/format.ps1
```

或在 Linux：

```bash
./scripts/format.sh
```

## 开发者指南与文档

- Windows 开发说明：`docs/dev-windows.md`
- Ubuntu / Linux 开发说明：`docs/dev-ubuntu.md`
- 人脸识别接口说明：`docs/human_recognition_interface.md`

在开始贡献前，请先阅读这些文档，它们包含系统依赖、常见问题与平台特定说明。

## 常见问题 & 排障

- 如果找不到 Qt 模块（例如 Qt Multimedia），请确保安装了对应 Qt 组件并将 `CMAKE_PREFIX_PATH` 指向 Qt 安装目录。
- 如果启用 dlib 时报错，请先执行 `scripts/install_dlib.*` 并确认编译器与依赖工具链（CMake、Python、CMake generator）匹配。

## 贡献

欢迎贡献：提交 issue、提交 PR。请遵循以下基本流程：

1. Fork 仓库并新建分支（feature/xxx 或 fix/yyy）。
2. 保持代码风格一致并通过本地格式化脚本。
3. 添加测试（若添加/修改功能）。
4. 提交 PR，简要描述变更并关联 issue（如适用）。

## 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

## 联系方式

维护者: ye (仓库 owner: yeisme)
