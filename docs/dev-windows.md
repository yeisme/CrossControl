## 开发环境 — Windows

本文件列出在 Windows（MSVC）上为本项目（CrossControl）准备开发环境、构建、打包与常见问题排查的步骤。

### 前提（建议）
- Windows 10/11
- Visual Studio 2022（含 C++ 工作负载）或等效 MSVC 工具集
- CMake >= 3.21（项目要求 3.24）
- Ninja（项目默认 generator 为 Ninja）
- Git

建议安装方式：
- 安装 Visual Studio 2022 并选择 “使用 C++ 的桌面开发” 工作负载。
- 安装 CMake（可通过 winget 或官网安装）并确保在 PATH 中。
- 安装 Ninja（可通过 winget install ninja）。

### vcpkg（推荐用于依赖管理）
仓库通过 `CMakePresets.json` 中的 `CMAKE_TOOLCHAIN_FILE` 使用 vcpkg。推荐在 PowerShell 中安装并注册 vcpkg。示例：

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
# 可选：注册到 PATH 或使用 VCPKG_ROOT 环境变量
[Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\tools\vcpkg', 'User')
```

安装常用包（在管理员 PowerShell 或普通 PowerShell 中都可以）：

```powershell
&C:\tools\vcpkg\vcpkg.exe install opencv4[calib3d,contrib,dnn,dshow,ffmpeg,freetype,fs,highgui,jpeg,msmf,openjpeg,openmp,png,thread,tiff,webp]:x64-windows
&C:\tools\vcpkg\vcpkg.exe install spdlog:x64-windows
```

确保 `VCPKG_ROOT` 已设置（重启 PowerShell 后生效），或在当前会话中导出：

```powershell
$env:VCPKG_ROOT = 'C:\tools\vcpkg'
```

### Qt
推荐使用 Qt Online Installer 安装 Qt 6（仓库测试过 6.8.3）。安装完成后，设置 `QT6_DIR` 环境变量，示例：

```powershell
[Environment]::SetEnvironmentVariable('QT6_DIR', 'C:\Qt\6.8.3\msvc_64', 'User')
$env:QT6_DIR = 'C:\Qt\6.8.3\msvc_64'
```

注意：`CMakePresets.json` 已将 `CMAKE_PREFIX_PATH` 指向 `$env{QT6_DIR}`，因此确保该变量正确指向 Qt 安装路径。

### 使用 CMake Presets（MSVC）
仓库内 `CMakePresets.json` 提供了 `msvc-debug` 与 `msvc-release` preset，generator 为 Ninja，构建输出位于 `build/msvc-debug/` 或 `build/msvc-release/`。

在 PowerShell 中配置（Debug）：

```powershell
# 在仓库根目录
cmake --preset msvc-debug
```

构建：

```powershell
cmake --build --preset msvc-debug
```

完整工作流（配置、构建、测试、打包）：

```powershell
cmake --workflow msvc-release-workflow
```

或者使用 VSCode 的 Tasks（workspace 中已包含几个 CMake tasks），或在 IDE 中直接选择对应 preset。

### 运行可执行文件
可执行文件位于 `build/msvc-debug/bin/` 或 `build/msvc-release/bin/`。直接双击或在 PowerShell 中运行：

```powershell
& "build\msvc-debug\bin\CrossControl.exe"
```

### 代码格式化
仓库提供 `scripts/format.ps1`（Windows），在 PowerShell 中运行：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/format.ps1
```

在 VSCode 中，可配置保存时自动格式化与 clang-format 扩展，项目使用的格式规则可在仓库中查找 `.clang-format`（如存在）。

### 打包与 CPack
`CMakePresets.json` 中有 package presets（msvc-release-package），可通过 CMake workflow 触发打包步骤或直接运行：

```powershell
cmake --preset msvc-release
cmake --build --preset msvc-release --target package
# 或 cmake --workflow msvc-release-workflow
```

生成的安装包会放在 `build/msvc-release/`，例如 `CrossControl-0.1-win64.exe` 和 `.zip`。

### 常见问题与排查
- CMake 找不到 Qt：确认 `QT6_DIR` 设置正确并包含 `lib/cmake/Qt6`。
- CMake 找不到 vcpkg 包：确认 `VCPKG_ROOT` 设置且对应包已安装（注意 triplet：`x64-windows`）。
- Ninja/Cl.exe 未找到：确保 Visual Studio 的命令行工具已正确安装；如果使用 VS Developer 命令提示符，环境会自动设置；在普通 PowerShell 中，可运行 `& "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`（路径依据安装版本）或打开 VS Native Tools PowerShell。
- 构建失败并报 PDB 或链接错误：确认 Debug/Release 匹配、依赖库的 runtime（static/dynamic）与编译选项一致。

### 额外提示
- 如果你在 VSCode 中工作，使用 CMake Tools 扩展并选择对应 preset，可以避免手动设置环境。
- 在 CI 或脚本中，使用 `cmake --preset` 与 `cmake --workflow` 使构建流程可重复。

更多细节请参见仓库根目录的 `CMakePresets.json` 与 `docs/dev.md`。
