# AutoChess 独立最小项目副本

本目录是以当前工作区最新 `build/msvc-release/AutoChessGame.exe` 所对应的源代码和 `data/` 为基准整理出的独立副本。它不引用原项目的 `dist/`、旧构建目录、测试代码、下载缓存或过程记录。

## 编译环境

- Windows x64。
- Visual Studio 2022 或更新版本，安装 **Desktop development with C++** 工作负载，并包含 MSVC x64 工具集和 Windows SDK。
- CMake 3.25 或更新版本。
- Ninja（CMake 能够找到 `ninja.exe`）。
- 运行程序需要 Windows 图形环境；本副本已随程序复制 SFML DLL 和 MSVC C++ 运行时 DLL。

当前项目使用 MSVC 和 C++17，不支持使用 MinGW 替代 MSVC 编译。

## 配置和编译

在 `cmd.exe` 中执行以下完整命令（请按实际 Visual Studio 安装位置调整第一行路径）：

```bat
cd /d D:\1\new
call "D:\VisualStudio2026\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake --fresh --preset msvc-release
cmake --build --preset msvc-release --parallel
```

也可以不使用预设，执行：

```bat
cd /d D:\1\new
call "D:\VisualStudio2026\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake -S . -B build\msvc-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl.exe
cmake --build build\msvc-release --parallel
```

`cmake --fresh` 会重新生成 `new/build/msvc-release/`，不会触碰原项目的 `build/`。

## 生成位置和启动方式

编译成功后，目标文件为：

```text
D:\1\new\build\msvc-release\AutoChessGame.exe
```

请从生成目录启动，或直接双击该 exe：

```bat
cd /d D:\1\new\build\msvc-release
AutoChessGame.exe
```

程序按 exe 所在目录查找 `data/`，因此不要从运行目录中移走 `data`。从源码根目录启动也可以，但推荐使用上面的 exe 所在目录启动方式。

## 必须和 exe 一起保留的运行时文件

在 `build/msvc-release/` 中，以下文件或目录必须保持与 `AutoChessGame.exe` 同级：

- `sfml-graphics-2.dll`
- `sfml-window-2.dll`
- `sfml-system-2.dll`
- `msvcp140.dll`
- `vcruntime140.dll`
- `vcruntime140_1.dll`
- `data/`
  - `game.cfg`、`units.cfg`、`factions.cfg`
  - `maps/map_01.map` 至 `maps/map_04.map`
  - `fonts/NotoSansSC-VF.ttf`
  - `animations/units/` 下 5 个单位的两套 Spine `.atlas`、`.png`、`.skel` 文件

CMake 会在每次成功链接后自动复制这些 DLL 和完整 `data/` 到 exe 目录。源码目录中的 `third_party/` 则是重新配置和编译所需的本地依赖：SFML 头文件、导入库，完整 Spine 3.8 C++ runtime 源码，以及 MSVC runtime 的副本。

## 许可证

- 项目许可：根目录 `LICENSE`。
- SFML：`third_party/SFML/license.md`。
- Spine Runtimes：`third_party/licenses/SPINE-RUNTIMES-LICENSE.txt`。
- 内置 Noto Sans SC 字体：`third_party/licenses/OFL-1.1.txt`。
- MSVC runtime 文件说明：`third_party/licenses/MSVC-REDIST-NOTICE.txt`。

## 资源路径说明

程序优先使用 exe 旁的 `data/`；只有在开发目录缺少相邻 `data/` 时才回退到本次构建时的源码 `data/`。因此将整个 `new/` 目录复制到另一位置后，应在该位置重新配置和编译，或者始终把 exe 与同级 `data/` 一起移动。
