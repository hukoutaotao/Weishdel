# 文件清单

本副本只保留能够重新编译和运行当前游戏所需的内容。测试、旧构建产物、`dist/`、下载缓存、截图、过程记录和无关文档均未复制。

| 目录/文件 | 用途 |
| --- | --- |
| `CMakeLists.txt` | 独立 CMake 构建定义；只生成 `AutoChessGame`、核心静态库和 Spine 静态库。 |
| `CMakePresets.json` | MSVC x64 Release + Ninja 的配置和构建预设。 |
| `src/core/` | 游戏规则、配置解析、地图、战斗、经济、回合、AI 和单位核心逻辑。 |
| `src/game/` | SFML 窗口、界面、地图绘制、单位绘制、Spine 资源加载、动画和运行时路径逻辑。 |
| `data/*.cfg` | 游戏、单位和阵营的正式配置。 |
| `data/maps/` | 当前正式使用的 4 张地图。 |
| `data/animations/units/` | 5 个单位的两套 Spine 3.8 skeleton、atlas 和贴图资源，共 30 个核心资源文件。 |
| `data/fonts/NotoSansSC-VF.ttf` | 独立副本的中文字体 fallback；系统字体仍按原优先级优先尝试。 |
| `third_party/SFML/include/` | 实际 include 递归所需的 SFML graphics/window/system 头文件。未包含 audio、network、Debug 头文件或文档。 |
| `third_party/SFML/lib/` | Release 动态链接所需的 `sfml-graphics.lib`、`sfml-window.lib`、`sfml-system.lib`。 |
| `third_party/SFML/bin/` | 运行时所需的三个 SFML Release DLL。 |
| `third_party/SFML/license.md` | SFML 及其外部库许可说明。 |
| `third_party/spine-runtimes-3.8/spine-cpp/` | 完整 Spine 3.8 C++ runtime 的头文件和 61 个实现文件。 |
| `third_party/spine-runtimes-3.8/spine-sfml/` | Spine 3.8 的 SFML adapter 源码和头文件。 |
| `third_party/licenses/SPINE-RUNTIMES-LICENSE.txt` | Spine runtime 许可证。 |
| `third_party/licenses/OFL-1.1.txt` | Noto Sans SC 使用的 SIL Open Font License 1.1。 |
| `third_party/msvc-runtime/` | 与当前 MSVC/SFML 动态链接匹配的 `msvcp140.dll`、`vcruntime140.dll`、`vcruntime140_1.dll`；构建后复制到 exe 旁。 |
| `third_party/licenses/MSVC-REDIST-NOTICE.txt` | MSVC runtime 可再发行文件说明。 |
| `LICENSE` | 原项目许可证文本。 |

## 生成后的运行目录

`build/msvc-release/` 是构建生成目录，不作为源码副本的一部分。成功编译后，CMake 会把 exe、3 个 SFML DLL、3 个 MSVC runtime DLL 和完整 `data/` 放在该目录，使其可以独立启动。
