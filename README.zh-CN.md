# openFAD FlipShift / VectorShift 中文工作说明

本仓库包含两个相关实现：

- `vst3/`：当前主线，使用 JUCE 8.0.12 和 CMake 开发的 openFAD FlipShift，目标格式为 VST3 和 AUv3。
- `patchers/`、`code/`：早期 VectorShift Max for Live 原型，用于保留 Max 9 / Ableton Live 12 的实验结构和算法参考。

openFAD FlipShift 是一款频谱创意效果器，围绕频率平移、镜像、缩放、谐波重映射、选区故障、音高映射、冻结和相位变换工作。开发者和插件厂商名称为 `UnpureBloom`。

## 本次界面与预设更新

- 新实例默认 Overlay 叠加布局、High（2048）FFT；旧工程仍恢复其已保存的质量。
- 默认中文，可用顶部 `中文 / EN` 切换；效果名保留英文。语言偏好不写入宿主自动化。
- 输入/输出频谱传输提升至 1024 点，实时与历史显示使用连续采样；常规文字与下拉框加大。
- 顶部预设栏支持保存、另存为、导入、导出；`.flipshift` 文件内部为 UTF-8 JSON，也可导入同结构 `.json`。
- 用户预设位于 `%APPDATA%/UnpureBloom/FlipShift/Presets`，不要求管理员权限。初始设置只读。
- 预设保存 11 个声音参数，保留当前旁路并在加载时解除冻结；不保存显示偏好或冻结音频。切换前会提示保存未保存的参数修改。
- DAW 工程保存完整参数和预设元数据，移走外部预设文件不会影响工程恢复。
- 验证细节和预设格式见 `UI_REFRESH.md`。

## 当前状态

当前 Windows Release 版本已经完成：

- 24 种频谱变换使用简短、常见的公开名称：Off、Bend、Smear、Spread、Harmonics、Subharm、Gate、Zero Phase、Shift、Mirror、Peak Push、Peak x2、Peak /2、Peak Expand、Peak Compress、Harm Sweep、Oct Stack、Wide Oct Stack、Comb、Pitch Blend、Spectral Scale、Phase Ripple、Glitch、Pitch Map。PROCESS 在悬停、键盘聚焦、宿主自动化和状态恢复后都会显示完整效果说明，相关旋钮提示也会随模式更新。
- Glitch 只在 Band Center 和 Band Q 选定的频段内执行确定性、随时间变化的随机频谱偏移；Offset 控制位移，Density 同时控制事件概率、湿度和刷新节奏。选区外频谱保持不变，左右声道共享同一事件布局。
- Pitch Map 可选择 C 至 B 共 12 个根音，以及 Major 或 Minor；其中 Minor 使用自然小调音程，Map Depth 在原始频谱与按最近音阶音归并的频谱之间混合。
- Low、Normal、High 三档 FFT 质量。
- 与频谱路径延迟对齐的干声/湿声混合。
- JUCE 8.0.12 WebView 前端。
- Windows 明确使用 WebView2，不允许回退到旧 Internet Explorer 内核。
- macOS/iOS 使用 WKWebView。
- 内嵌 HTML、CSS、JavaScript，无生产服务器和外部网络依赖。
- WebView 仅作为原生插件的高质量渲染层：文档根节点固定且无整页滚动，原生模式禁用文本选择、右键、拖拽、内部导航和浏览器快捷键。
- 实时输入/输出频谱、瀑布图、Pivot 和目标频率引导线；瀑布提供 VISION、PULSE、PHOSPHOR、XRAY 四套 LOOK。
- 参数双向同步、宿主自动化手势和状态恢复。
- 320x280 最小插件窗口；紧凑窗口通过 ANALYZE/CONTROL Tab 保持固定插件表面。
- 编辑器隐藏时通过 `surfaceVisibility` 暂停前端主 `requestAnimationFrame`、手势和旋钮粒子；原生分析器消费者在整个 Editor 生命周期内保持启用，仅在 Editor 析构时禁用，重新显示时继续使用保留的瀑布历史。
- 首轮实时稳健性和低风险性能优化：质量异步切换，统一 `callback -> configuration` 锁顺序，完整 `reset()`，分析器 SPSC 固定三缓冲和 generation 隔离，prepare 阶段预分配，Smear O(N)，以及参数、采样率、输入和输出的 finite/clamp 防线。
- Windows VST3 Release 构建和 CTest 2/2 通过，包含 DSP 测试和真实 WebView2 GUI 集成测试；WebView 四视口运行时测试也通过。
- 当前 Release 与系统安装副本的 SHA-256 均为 `D69DFD4C17EB8259322D78B671567A98FDB2FA099BA11252694242E38816E359`。
- 系统安装副本已通过 pluginval 1.0.4 严格度 10、`Repeat=3`；该结果只验证 Windows VST3，不能替代 AUv3 验证。

AUv3 的 CMake 配置已经加入，但 Apple/Xcode 编译、签名、GarageBand、额外 AUv3 宿主和 iPhone/iPad 真机测试仍未验证。树莓派 4/5 的 callback 分位数和实机性能基准也仍未验证。

## Windows 安装程序

Windows x64 版本提供单一 Setup 安装程序。安装前关闭所有 DAW，以管理员身份运行安装程序，然后在宿主中重新扫描 VST3。插件会安装到：

`C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3`

Setup 已内置 Microsoft Edge WebView2 Evergreen x64 完整离线运行时和 Microsoft Visual C++ 2015-2022 x64 运行库。HTML、CSS、JavaScript 已嵌入 VST3 二进制，因此安装后的界面不依赖本仓库、开发环境、本地 HTTP 服务、Node.js、Python 或互联网连接。

安装程序面向 64 位 Windows 10 1809 及更高版本。发布 ZIP 会同时包含 Setup、`README.md`、本中文说明、`LICENSING.md` 和 SHA-256 校验文件。当前二进制没有代码签名，因此 Windows 可能显示 SmartScreen 或未知发布者警告。正式公开分发前仍需解决 `LICENSING.md` 记录的项目协议和 JUCE 授权路径。

## 许可证状态

当前仓库根目录尚未包含 FlipShift 的 `LICENSE` 或 `COPYING` 文件，因此源码公开可见不代表已经授权复制、修改或分发。openFAD 主项目的 GNU AGPLv3 不会自动覆盖本仓库。

工程固定使用 JUCE 8.0.12。JUCE 模块可按 GNU AGPLv3 或商业 JUCE 许可证使用，但本仓库尚未记录采用哪条路径。公开发布源码或二进制前，维护者必须先选择 FlipShift 项目许可证、记录 JUCE 许可路径，并整理 WebView2、VST3 SDK、AudioUnitSDK 等实际分发组件要求的第三方声明。完整状态和发布门槛见 `LICENSING.md`。

## 目录速查

| 路径 | 用途 |
| --- | --- |
| `vst3/Source/PluginProcessor.*` | JUCE 处理器、参数读取、状态保存和总处理入口 |
| `vst3/Source/DSP/FlipShiftEngine.*` | STFT、FFT、环形缓冲、平滑和分析器数据 |
| `vst3/Source/DSP/SpectralModes.*` | 24 种频谱变换的具体算法 |
| `vst3/Source/Parameters.*` | 参数 ID、范围、默认值、模式名称和控件说明 |
| `vst3/Source/PluginEditor.*` | WebView2/WKWebView 容器、资源提供和原生事件桥接 |
| `vst3/WebUI/index.html` | WebView 页面结构 |
| `vst3/WebUI/styles.css` | 品牌样式和响应式布局 |
| `vst3/WebUI/app.js` | 控件逻辑、参数映射、Canvas 分析器和 JUCE 事件 |
| `vst3/WebUI/ui-spec.json` | 前端交接合同，包含全部控件、模式、事件和验收规则 |
| `vst3/Tests/DSPTests.cpp` | VST3 DSP 自动测试 |
| `tests/` | 构建、安装、pluginval 和 DAW 验收脚本 |
| `LICENSING.md` | 当前许可证状态、JUCE 路径和第三方声明发布门槛 |
| `previews/` | 历史 UI 预览和比较材料 |
| `branding/openfad/` | openFAD 品牌资产与视觉规范 |

## 开发环境

Windows 推荐环境：

- Windows 10/11 x64。
- Visual Studio 2022 Build Tools 或完整 Visual Studio 2022。
- CMake 3.24 或更高版本。
- Ninja。
- JUCE 8.0.12；当前机器使用 `D:\JUCE`。
- Microsoft Edge WebView2 Runtime。
- pluginval，用于 VST3 宿主级验证。

如果未传入 `JUCE_PATH`，CMake 会尝试从 GitHub FetchContent 下载固定的 JUCE 8.0.12。国内网络环境下优先使用本地 JUCE，避免配置阶段受 GitHub 连接影响。

## Windows 快速工作流

### 1. 校验仓库源文件

```powershell
powershell -ExecutionPolicy Bypass -File tests/validate.ps1
```

该脚本会检查 Max patch、JSON、WebView 文件、UI 规范和关键文档是否存在，并跳过 `vst3/build*` 生成目录。WebView 的浏览器运行时检查由 `tests/validate-webui.py` 单独执行，覆盖 1000x650、621x844、390x844 和 320x280 四种视口。

```powershell
python tests/validate-webui.py --screenshots-dir vst3/build/webui-qa
```

该脚本需要 Python Playwright，并使用本机 Chrome 或 Edge；它验证根文档无滚动、紧凑 Tab、文本适配、瀑布像素确实随频谱变化、能量区域亮度、仪表填充和峰值读数，以及原生模式的右键、选择、拖拽和浏览器快捷键拦截。传入 `--screenshots-dir` 后会输出四种视口、紧凑 CONTROL、无信号、信号瀑布和信号仪表截图。

### 2. 构建 VST3 并运行 DSP 测试

```powershell
powershell -ExecutionPolicy Bypass -File tests/build-vst3.ps1
```

该构建脚本运行 CTest；当前 Windows 结果为 2/2。新增的 `FlipShiftGUIIntegrationTests` 使用与插件相同的 Processor、Editor、参数、DSP 和内嵌 WebUI 源码，向真实 WebView2 输入确定性音频并读取实际 DOM。一次代表性通过结果为 46 个 `analyzerFrame` 事件、190 个瀑布列、输入/输出各 1024 点，以及输入/输出均约 -10.5 dB 的仪表读数。它是测试可执行程序，不会加载系统安装的 VST3 bundle，也不能覆盖 Ableton 的 VST3 wrapper、扫描缓存或旧 UI 缓存问题。

显式指定 JUCE 或 MSVC 环境时：

```powershell
powershell -ExecutionPolicy Bypass -File tests/build-vst3.ps1 `
  -JucePath D:\JUCE `
  -VcvarsPath D:\VS2022BuildTools\VC\Auxiliary\Build\vcvars64.bat
```

默认 Release 产物位于：

```text
vst3/build/OpenFADFlipShift_artefacts/Release/VST3/openFAD FlipShift.vst3
```

其他 `vst3/build*` 目录可能来自专项构建。安装和 pluginval 脚本默认只使用上述规范 `vst3/build` 产物；测试其他目录时必须显式传入 `-PluginPath`，不会按时间自动选择所谓“最新”构建。

### 3. 运行 pluginval

```powershell
powershell -ExecutionPolicy Bypass -File tests/run-pluginval.ps1
```

明确测试某个 bundle：

```powershell
powershell -ExecutionPolicy Bypass -File tests/run-pluginval.ps1 `
  -PluginPath "vst3/build/OpenFADFlipShift_artefacts/Release/VST3/openFAD FlipShift.vst3"
```

当前要求为严格度 10。需要重点通过编辑器、边处理边打开编辑器、自动化、状态恢复、参数线程安全、总线布局和参数模糊测试。

### 4. 安装到系统 VST3 目录

```powershell
powershell -ExecutionPolicy Bypass -File tests/install-vst3.ps1 -Scope System
```

系统安装位置：

```text
C:\Program Files\Common Files\VST3\openFAD FlipShift.vst3
```

仅当前用户安装：

```powershell
powershell -ExecutionPolicy Bypass -File tests/install-vst3.ps1 -Scope User
```

## Ableton Live 中仍显示旧 UI

旧 UI 通常不是 WebView 代码失效，而是 DAW 仍在使用旧 bundle 或进程缓存。

按以下顺序处理：

1. 完全退出 Ableton Live，确认任务管理器中没有残留 Live 进程。
2. 重新执行系统安装脚本。
3. 再次打开 Live。
4. 打开 `Preferences > Plug-Ins`。
5. 按住 `Alt` 点击 `Rescan`，执行深度重新扫描。
6. 删除轨道上的旧插件实例并重新插入 `UnpureBloom > openFAD FlipShift`。

DAW 听感和工作流验收清单位于 `tests/DAW_ACCEPTANCE.md`，测试音频位于 `tests/daw-audio/`。

## WebView UI 工作方式

Release 插件通过 `juce_add_binary_data` 把以下文件编译进插件：

- `vst3/WebUI/index.html`
- `vst3/WebUI/styles.css`
- `vst3/WebUI/app.js`

C++ 通过资源提供器响应 WebView 内部请求，不读取用户磁盘上的前端文件，也不访问公网。上线形态不是普通网页：页面根节点固定到插件视口，不提供网页导航、整页滚动、文本选择、右键菜单、拖放或刷新/查看源码/缩放等浏览器快捷键；About 外链只能经 C++ 精确白名单交给系统浏览器。

### JUCE 桥接事件

前端发送给 C++：

- `uiReady`：页面加载完成，请求完整参数快照。
- `parameterGesture`：发送 `begin`、`value`、`end`，确保宿主记录完整自动化手势。
- `openExternal`：About 中的外链请求。C++ 只接受 openFAD、UnpureBloom、当前源码仓库和 openFAD 主项目许可证四类精确白名单地址。桌面插件交给系统浏览器打开；iOS AUv3 App Extension 会静默拒绝，避免调用扩展环境禁止的系统 API。

C++ 发送给前端：

- `parameterState`：参数值对象和当前采样率。
- `analyzerFrame`：输入和输出频谱，各 192 个对数采样 dB 点。
- `surfaceVisibility`：宿主编辑器显隐提示，仅用于暂停或恢复前端主动画、手势和粒子生命周期。

分析器数据以约 15 Hz 生成，Canvas 绘制不在音频线程运行。Editor 存在期间原生分析器消费者始终启用，即使宿主暂时隐藏窗口也不会停产；隐藏提示只停止前端主 `requestAnimationFrame`、结束手势并清除旋钮粒子。只有 Editor 析构才禁用分析器消费者。

## 根据参考网站重做 UI

视觉重构时，以 `vst3/WebUI/ui-spec.json` 为功能合同。该文件定义：

- 14 个参数的精确 ID、范围、默认值、单位和映射。
- 24 个模式下哪些控件启用、控件标签如何变化，以及每个 PROCESS 选项的效果说明和模式专用参数提示。
- Glitch 的选区覆盖层只读取 Band Center、Band Q、质量和采样率，不写入、重着色或重算瀑布 Canvas 历史。
- Pitch Map 选中时，ROOT/SCALE 控件在五列参数条中原位替换 AXIS，并显示当前根音与调式；离开该模式后恢复 AXIS。
- WF 和 FFT 两个显示图层开关，内部仍映射 Spectrum、Waterfall、Both 三种分析器视图。
- 1x、2x、4x、8x 四档瀑布滚动速度，约对应 15、30、60、120 个视觉列/秒；真实分析数据仍为 15 Hz。
- VISION、PULSE、PHOSPHOR、XRAY 四套本地瀑布 LOOK。LOOK 只使用不同的 dB 显示范围、gamma、色阶和网格强调色重新着色同一份历史数据，不增加宿主参数。
- STACK、OVERLAY、SPLIT 三种本地分析器排布，不增加宿主参数。
- 瀑布使用独立环形历史缓冲；参数变化不会重算过去历史，也不会直接生成或改写热图能量，只有后续真实分析器输出可以改变未来列。
- 切换 LOOK 允许从环形缓冲执行一次完整的像素重着色，但不得重新填充、追加、清空或重排历史，不得改变时间位置、事件标记、声音、延迟或分析器传输。
- 参数事件标记位于瀑布工具栏下方的独立事件轨，短时读数位于工具栏；两者都不能覆盖、染色、遮挡或触发重绘热图像素。
- 旋钮本地拖动、键盘调整和双击重置使用一个复用 Canvas 与固定 24 粒子对象池提供短拖尾；宿主自动化、工程恢复和初始参数快照不产生粒子，粒子结束后不保留动画帧循环，减少动态效果对低性能 WebView 的负担。
- About 弹层介绍 openFAD、UnpureBloom 和主要功能。许可证区域明确说明：当前 FlipShift 仓库尚未包含许可证文件，因此公开可见源码不等于已经授权复制、修改或分发；openFAD 主项目采用的 GNU AGPLv3 仅作为上游项目信息单独列出；JUCE 8 的 AGPLv3 或商业许可路径仍须在发布前声明。弹层支持 Tab 焦点约束、Escape 关闭和焦点返回。
- 浏览器预览使用固定频率位置的时间变化，不得生成规则移动的深蓝色斜纹；整幅频谱不能为了预览动画沿频率轴平移。
- WebView 原生事件格式。
- openFAD 颜色和排版 token。
- 320 px 最小宽度、620/621 px 分界和 820 px 紧凑工作区规则；运行时测试固定覆盖 1000x650、621x844、390x844、320x280。
- 无文本溢出、键盘操作、焦点、状态恢复等验收条件。

拿到参考网站后，只替换布局、视觉语言、动画和交互表现，不应改变参数 ID、宿主自动化协议、DSP 参数范围或模式控制关系。将参考地址写入：

```json
{
  "visualReference": {
    "status": "ready",
    "referenceUrl": "https://example.com"
  }
}
```

## 参数概览

主要可自动化参数：

- `mode`
- `shiftHz`
- `scale`
- `pivotHz`
- `amount`
- `widthQ`
- `pitchRoot`
- `pitchScale`
- `mix`
- `outputGainDb`
- `bypass`

非自动化工作流参数：

- `quality`
- `analyzerView`
- `freeze`

仅存在于当前 WebView 文档中的本地界面控件：

- `waterfallRate`：瀑布滚动速度。
- `waterfallVisual`：四套 LOOK，默认 VISION；新建 WebView 文档时恢复默认值。
- `analyzerLayout`：STACK、OVERLAY、SPLIT 排布。

这些本地控件没有 `data-param`，不会加入宿主参数列表、发送 `parameterGesture`，也不会创建瀑布参数事件标记或工具栏参数读数。

完整范围、模式映射、效果说明和 LOOK 色阶不要在 README 中手工复制维护。WebView 合同以 `vst3/WebUI/ui-spec.json` 为准，实际算法以 `vst3/Source/DSP/SpectralModes.cpp` 和 `vst3/Source/Parameters.cpp` 为准。

## AUv3

Apple 平台默认启用：

```text
OPENFAD_BUILD_AUV3=ON
```

CMake 会增加 `AUv3` 和 `Standalone`。Standalone 是用于安装 AUv3 App Extension 的容器。

macOS 上的示例：

```sh
cmake -S vst3 -B vst3/build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DOPENFAD_BUILD_AUV3=ON \
  -DJUCE_PATH=/path/to/JUCE

cmake --build vst3/build-ios --config Release \
  --target OpenFADFlipShift_Standalone OpenFADFlipShift_AUv3
```

详细说明和真机验收项见 `vst3/WEBVIEW_AUV3.md`。

## DSP 优化原则

本轮已经完成第一批不改变产品语义的 DSP/实时线程优化：质量重建和延迟通知移出音频回调，锁顺序统一为 `callback -> configuration`，补齐处理器/引擎 `reset()`，prepare 阶段预分配频谱状态，分析器改为带 generation 的 SPSC 固定三缓冲，Smear 从邻域重复求和降为 O(N) 前缀和，并加入 finite/clamp 防线、线性增益平滑、环形 mask、条件查峰和相位回绕。Pitch Map 的映射表只在根音、调式、采样率或 FFT 配置变化时重建；Glitch 使用确定性哈希和帧保持，不在音频回调中分配内存。完整后续计划位于 `vst3/DSP_OPTIMIZATION_PLAN.md`，目标仍是按树莓派 4/5 性能等级约束代码。

优先顺序：

1. 建立可重复的性能基线和参考音频。
2. 消除音频线程内的分配、锁和宿主调用。
3. 优化环形缓冲、参数平滑和重复计算。
4. 按模式拆分频谱内核并优化高成本数学函数。
5. 最后处理数据布局、SIMD 和 ARM64 NEON。

当前 Windows Release、DSP 测试和 CTest 已通过，但这不能替代性能基准。树莓派 4/5 的 p50/p95/p99/max callback 测量、ARM64/NEON 对照、长时间 deadline 测试，以及 Apple/Xcode/AUv3 真机验证仍未完成，不能写成已支持或已优化完成。

## LTO 注意事项

`OPENFAD_ENABLE_LTO` 默认关闭。当前 MSVC 19.44、JUCE 8.0.12 组合在开启 `/GL` 后曾在 pluginval 中稳定触发堆损坏，因此当前 Release 使用正常 `/Ox` 优化，不启用 LTO。

如需重新评估：

```powershell
cmake -S vst3 -B vst3/build-lto -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DOPENFAD_ENABLE_LTO=ON `
  -DJUCE_PATH=D:\JUCE
```

必须重新跑 DSP 测试、pluginval 多次随机测试和 DAW 长时间处理，不能仅以构建成功作为启用依据。

## Max for Live 原型

打开 `VectorShift.maxproj`，再打开 `patchers/VectorShift.maxpat`。仓库保存的是可编辑 Max 源码，不伪造二进制 `.amxd`；最终设备应在 Ableton Live 的 Max Audio Effect 编辑器中另存生成。

Max 原型与当前 JUCE 插件并非完全相同的发布路径。新功能默认应先判断属于：

- VST3/AUv3 主线；或
- Max for Live 实验原型。

不要在两个实现中同时盲目复制改动，除非已经明确参数、算法和状态兼容要求。

## 提交前检查

至少运行：

```powershell
powershell -ExecutionPolicy Bypass -File tests/validate.ps1
python tests/validate-webui.py --screenshots-dir vst3/build/webui-qa
powershell -ExecutionPolicy Bypass -File tests/build-vst3.ps1
powershell -ExecutionPolicy Bypass -File tests/run-pluginval.ps1
```

涉及安装或 DAW UI 时，再运行：

```powershell
powershell -ExecutionPolicy Bypass -File tests/install-vst3.ps1 -Scope System
```

然后完全重启 DAW、强制 Rescan，并按照 `tests/DAW_ACCEPTANCE.md` 完成听感和状态恢复检查。
