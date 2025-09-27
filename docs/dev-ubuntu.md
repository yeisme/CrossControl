## 开发环境 — Ubuntu

本文件列出在 Ubuntu 上为本项目（CrossControl）准备开发环境、构建、打包与常见问题排查的步骤。

### 前提（建议）
- Ubuntu 20.04/22.04 或更高
- CMake >= 3.21 (项目 `CMakeLists.txt` 要求 3.24)
- Ninja
- gcc / g++ 或 clang
- Git、curl

安装示例：

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git curl pkg-config ca-certificates
```

### vcpkg（推荐用于依赖管理）
项目使用 `CMakePresets.json`，默认通过 `VCPKG_ROOT` 指定的 vcpkg toolchain 集成依赖。

安装与常用包（参考 `docs/dev.md`）：

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"
# 示例：安装 OpenCV（按需开启 feature 列表）
$VCPKG_ROOT/vcpkg install opencv4[calib3d,contrib,freetype,fs,highgui,openjpeg,openmp,png,thread,tiff,webp]:x64-linux
# 安装 spdlog（会自动安装 fmt）
$VCPKG_ROOT/vcpkg install spdlog:x64-linux
```

设置环境变量（建议加入 `~/.bashrc` 或 `~/.profile`）：

```bash
export VCPKG_ROOT="$HOME/vcpkg"
export CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

### Qt
推荐使用 Qt Online Installer 安装 Qt 6（本仓库测试过 6.8.x）。安装完成后，设置 `QT6_DIR` 环境变量指向 Qt 安装目录（包含 `lib/cmake/Qt6`）。例如：

```bash
export QT6_DIR="/opt/Qt/6.8.3/gcc_64"
```

### 配置与构建（使用 CMake Presets）
仓库包含 `CMakePresets.json`，预置了常见 configure/build/package preset。常用示例：

配置（Debug）：

```bash
cmake --preset gcc-debug
```

构建：

```bash
cmake --build --preset gcc-debug
```

配置发布并打包（完整 workflow）：

```bash
cmake --workflow gcc-release-workflow
```

可用的 configure presets（部分）：
- `gcc-debug`, `gcc-release`, `clang-debug`, `clang-release`

构建输出位于 `build/<preset-name>/`，例如 `build/gcc-debug/`。

### 运行与测试
- 可执行文件在 `build/gcc-debug/bin/` 或 `build/gcc-release/bin/`。
- 运行已构建的可执行文件：

```bash
./build/gcc-debug/bin/CrossControl
```

如果使用 CTest：

```bash
cd build/gcc-debug
ctest --output-on-failure
```

### 代码格式化
仓库提供 `scripts/format.sh`（Linux）用于格式化代码：

```bash
bash scripts/format.sh
```

### 常见问题与排查
- CMake 找不到依赖（OpenCV / Qt / spdlog）：确认 `VCPKG_ROOT`、`QT6_DIR` 已导出并且 vcpkg 中已安装对应包。
- CMake 报错要求更高版本：请安装 CMake >= 3.24（项目顶层要求 3.24）。
- 找不到编译器：确认 `gcc`/`g++` 或 `clang` 已安装并在 PATH 中。
- 如果使用 Ninja，确保 `ninja-build` 已安装。

### 额外提示
- 在 CI 中可复用 `cmake --preset ...` 和 `cmake --workflow ...` 命令构建并打包。
- 如果需要交叉或自定义 toolchain，请参考 `CMakePresets.json` 并在调用时覆盖 cache 变量：`-D` 或自定义 preset。

更多细节请参见仓库根目录的 `CMakePresets.json` 与 `docs/dev.md`。
