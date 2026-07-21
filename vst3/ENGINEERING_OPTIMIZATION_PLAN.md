# openFAD FlipShift 工程检查与优化方案

本文是稳健性、实时性能、Apple/AUv3 兼容和 WebView 产品化的总执行合同。
DSP 算法细化仍参考 `DSP_OPTIMIZATION_PLAN.md`，Apple 架构背景参考
`WEBVIEW_AUV3.md`。任何优化都必须先证明行为等价或明确记录产品行为变化。

状态更新于 2026-07-20：本轮 Windows 实施和验证已完成；Apple/Xcode/AUv3
真机验证与 Raspberry Pi 4/5 benchmark 仍未完成。

## 1. 产品约束

### 1.1 音频实时约束

- `processBlock()` 及其调用链不得分配或释放堆内存。
- 音频线程不得获取互斥锁、SpinLock，不得等待 UI、文件系统或消息线程。
- 音频线程不得调用宿主通知、日志、文件、网络和系统 UI API。
- 参数、输入、状态恢复中的 NaN/Inf 不得污染环形缓冲、冻结帧或相位历史。
- 质量、模式、Bypass、Freeze 切换必须有确定的状态迁移规则。

### 1.2 WebView 上线约束

WebView 只作为高质量渲染层，不作为网页浏览器产品：

- 不允许插件内部网页导航、新窗口、状态栏、默认错误页或可见浏览器历史。
- 原生插件模式下禁用页面文本选择、拖拽、右键菜单和浏览器刷新/地址栏快捷键。
- 文档页面不得滚动；小窗口只能在明确的工具面板内滚动，或使用视图 Tab。
- 控件必须保持音频插件语义：旋钮、分段开关、模式选择、仪表和分析器。
- 外链必须经过 C++ 精确白名单，并交给系统浏览器；AUv3 Extension 可拒绝外链。
- 动画必须有固定上限，支持 reduced motion，页面隐藏后停止无用刷新。

### 1.3 Apple 约束

- iOS 只生成 AUv3 和 Standalone，不生成 VST3。
- AUv3 target 必须开启 App Extension-safe API 编译检查。
- Standalone 输入总线必须提供 `NSMicrophoneUsageDescription`。
- UI 至少覆盖 320x280 紧凑窗口、iPhone/iPad 旋转和宿主动态 resize。
- 共享代码不得调用 Extension 禁止的 UIApplication/NSWorkspace 路径。
- 最终发布必须在 Xcode、GarageBand 和至少一个额外 AUv3 host 中验证。

## 2. 实施前基线与风险登记

本节保留实施前的问题作为审计记录；其本轮处理状态以第 3、4、5 节为准，
不应再把这些条目解读为当前代码仍然存在的问题。

### P0 发布阻断

1. 质量切换在音频线程重建 FFT 和全部 vector，并在回调末尾调用
   `setLatencySamples()` 通知宿主。
2. `SpectralFrameMemory` 在首个 STFT 帧懒分配，正常播放开始后仍会分配。
3. 分析器在音频线程与 UI 线程之间共享 SpinLock；UI 在锁内复制和扩容 vector。
4. iOS CMake 格式列表仍包含 VST3，且 Standalone 没有麦克风权限说明。
5. 820px 以下使用整页滚动，当前行为具有明显网页页面特征。

### P1 高优先级

1. smoother 从硬编码默认值启动，状态恢复后的首批帧可能使用错误参数。
2. 参数、采样率和输入缺少统一 finite/clamp 防线。
3. 相位累加不回绕，长时间运行后 float 精度下降；phase wrap 使用循环减法。
4. 所有模式无条件查找峰值、更新相位历史；Smear 为 O(N * radius)。
5. 无编辑器消费者时仍持续计算分析器幅度与 dB。
6. WebView 参数事件未拒绝 NaN/Inf，Windows 仍保留状态栏和默认错误页。
7. macOS AUv3 仍可能进入 `NSWorkspace` 外链路径。

### P2 优化候选

1. 每采样执行 dB 到线性增益转换并重复读取声道指针。
2. 环形索引使用 `%`，FFT 热循环包含可避免的常量计算。
3. Oct Stack、Harm Sweep、Phase Ripple 等模式逐 bin 调用大量超越函数。
4. 构建脚本部分原生命令失败后没有立即终止。
5. 缺少逐模式性能 JSON、回调分位数、分配计数和长时相位测试。

## 3. 执行阶段

### Phase 0: 可复现基线

- [ ] 新增离线 benchmark，输出模式、质量、块大小、采样率、吞吐和耗时分位数 JSON。
- [ ] 保存 24 种模式的参考音频和频谱特征，定义峰值、RMS、相位和最大误差容差；Glitch 需覆盖选区外恒等，Pitch Map 需覆盖 12 根音、Major 和使用自然小调音程的 Minor。
- [x] 增加分析器并发压力、长时相位和 NaN/Inf 测试；回调分配 hook 仍待补充。
- [ ] Windows 记录 x86-64 Release 基线；Apple 记录 Intel/ARM64 与 vDSP 基线。
- [ ] Raspberry Pi 4/5 记录 48kHz、64/128 sample block 的 p50/p95/p99/max。

### Phase 1: 实时安全与稳健性

- [x] 质量切换移出音频回调，通过异步消息处理暂停处理、等待当前 callback 结束后重建，并在回调外更新宿主延迟。
- [x] 统一使用 `callback lock -> engine configuration lock`，覆盖 prepare、release、reset 和质量重建，避免锁顺序反转。
- [x] `prepare()` 使用当前恢复状态初始化 smoother，避免默认值启动。
- [x] prepare 阶段预分配全部 spectral memory，reset 只清零不释放。
- [x] 分析器改成带 sequence/generation 校验的 SPSC 固定三缓冲；Editor 不存在时停止，Editor 生命周期内保持启用并将发布频率限制到约 15Hz。
- [x] 统一清洗采样率、枚举、参数和非有限输入；非有限输出归零并隔离状态。
- [x] WebView 参数事件只接受已知 phase，以及有限、范围内的 normalized 值。
- [x] `build-vst3.ps1` 在 vcvars 和非 vcvars 路径检查配置、构建与 DSP 测试退出码。

### Phase 2: 数学等价的低风险性能优化

- [x] 缓存声道指针，线性域平滑输出增益，移除每采样 `pow`。
- [x] 使用 FFT power-of-two mask 替代环形 `%`。
- [x] 只在需要的模式查峰、计算 Gate 峰值和更新相位历史；Freeze 下依赖峰值的模式使用冻结频谱。
- [x] 预计算 bin phase advance，并在每帧回绕相位累加器。
- [x] Smear 使用前缀和，从 O(N * radius) 降到 O(N)。
- [x] Pitch Map 缓存目标 bin 和插值权重，只在根音、调式、采样率或 FFT 配置变化时重建；稳态处理不分配、不加锁，也不逐 bin 调用 `log2`/`pow`。
- [x] Glitch 使用确定性哈希与有界帧保持，左右声道事件一致，选区外 bin 恒等且不依赖可变 RNG 状态。
- [ ] 把每帧常量移出 bin 循环，并为 Off/Amount=0/Shift=0/Scale=1 建立 fast path。

### Phase 2.5: WebView 原生插件表面

- [x] 文档根节点固定到插件视口，无整页滚动；最小窗口为 320x280。
- [x] 紧凑窗口使用 ANALYZE/CONTROL Tab，仅参数工具区域允许内部滚动。
- [x] 原生模式禁用文本选择、右键菜单、拖拽、内部导航和浏览器刷新/源码/缩放快捷键。
- [x] 编辑器隐藏时仅暂停前端主 `requestAnimationFrame`、手势和旋钮粒子；分析器消费者保持到 Editor 析构，重新显示时保留瀑布历史。
- [x] `tests/validate-webui.py` 覆盖 1000x650、621x844、390x844 和 320x280，断言瀑布像素/亮度与仪表填充/读数，并生成视觉验收截图。
- [x] Glitch 使用独立 DOM 选区覆盖层，不能写入瀑布 Canvas；Pitch Map 在固定五列参数条内用 ROOT/SCALE 原位替换 AXIS，不能合成分析器能量。

### Phase 3: 状态切换质量

- [ ] 评估三套质量状态预构建，避免显式质量切换时的重建停顿。
- [ ] 比较动态宿主延迟与固定最大延迟策略，不在未评估听感和工作流前提高默认延迟。
- [ ] Mode、Bypass、Freeze 和质量切换加入有界 crossfade/预热。
- [ ] 明确 tail length，避免宿主过早截断 STFT 或冻结残留。

### Phase 4: 模式专用内核与数据布局

- [ ] 按模式族拆分 kernel，避免每个 bin 执行大 switch。
- [ ] 为 Oct Stack、Harmonics/Harm Sweep 和 Phase Ripple 系列建立缓存表或有误差上限的近似。
- [ ] 单独测量 Glitch 哈希成本和 Pitch Map scatter 写入局部性，确认 Low/Normal 质量在 Pi 级缓存预算内。
- [ ] 对比 complex AoS 与 split real/imag SoA。
- [ ] 先保证标量循环可自动向量化，再评估 JUCE SIMD、ARM64 NEON 和替代 FFT。
- [ ] 禁止未经数值、听感和 Apple vDSP 对照验证的 fast-math。

### Phase 5: Apple 发布闭环

- [ ] Xcode 配置、签名、AUv3/Standalone 构建和 App Store Extension-safe 检查。
- [ ] GarageBand 与额外 AUv3 host：mono/stereo、所有质量、状态、自动化、离线导出。
- [ ] iPhone/iPad：320x280 起的 view configuration、旋转、前后台和 memory warning。
- [ ] Apple Silicon Universal 2、最低部署版本、沙箱、麦克风权限和 bundle ID 验证。
- [ ] Instruments 检查 audio deadline、allocation、WKWebView 进程、能耗和内存峰值。

## 4. 本轮首批实施范围

本轮已经实施并在 Windows 上验证以下不会主动改变算法产品含义的项目：

1. 移除音频线程质量重建和宿主延迟通知。
2. 预分配 spectral memory；增加 finite/clamp 防线和当前状态 smoother 初始化。
3. 无锁、固定容量、按消费者启停并限速的分析器快照。
4. 缓存指针、线性增益、环形 mask、条件查峰、Smear 前缀和、Pitch Map 映射缓存、Glitch 确定性帧保持和相位回绕。
5. WebView 原生化策略、固定插件视口和紧凑窗口 ANALYZE/CONTROL Tab。
6. iOS/macOS 格式选择、麦克风权限、设备族、App Extension-safe 编译属性和 Apple 外链保护的 CMake/源码配置。Apple 产物本身尚未编译或真机验证。
7. 扩展 DSP/合同/构建测试，并重新运行 Release、pluginval 和系统安装校验。

预构建三套质量状态、切换 crossfade、模式 kernel 拆分、SIMD/NEON 和 Pi 实机调优
保留到有 benchmark 与参考输出后的后续阶段。

## 5. 自动化验收矩阵

### DSP

- 采样率：44.1、48、88.2、96、192 kHz。
- 块大小：0、1、7、31、64、127、128、511、1024、4096。
- 声道：mono、stereo。
- 质量：Low、Normal、High。
- 状态：24 模式、Bypass、Freeze、质量切换、恢复、快速参数变化，以及 Pitch Map 的 12 根音、Major 和使用自然小调音程的 Minor。
- 异常：NaN/Inf 参数和输入、空 buffer、超范围枚举。
- 输出：finite、峰值有界、延迟正确、参考峰值/RMS 不越界。

### UI/WebView

- 视口：1000x650、621x844、390x844、320x280。
- 文档根节点无滚动；紧凑模式只有明确工具面板可滚动。
- native 模式无文本选择、右键、拖拽、浏览器快捷键和内部导航。
- ANALYZE/CONTROL Tab 不改变插件参数、瀑布历史或宿主自动化。
- Glitch 覆盖层和 Pitch Map 读数不改写分析器数据；Pitch Map 模块替换不改变五列布局。
- reduced motion 和页面隐藏时无空闲前端动画；Editor 关闭/析构后停止分析器生产。

### Host/构建

- [x] `tests/validate.ps1`、JSON/JS 静态检查、四视口 WebView 运行时检查和 `git diff --check`。
- [x] Windows Release 构建和 CTest 2/2：完整 DSP tests，以及使用同一 Processor/Editor/WebUI 源码、确定性音频和真实 WebView2 DOM 的 GUI 集成测试。代表性结果为 46 个 analyzerFrame、190 个瀑布列、192+192 点和 -10.5 dB 输入/输出仪表。
- [x] GUI 集成测试的边界已记录：它不加载系统安装的 VST3 bundle，也不覆盖 Ableton wrapper、插件扫描或旧 UI 缓存。
- [x] 当前 24 模式 Release 已安装到系统 VST3 目录；Release 与安装副本的 SHA-256 均为 `345B61AB31D6B7575712065F7D51B16845479C4A5B8DD60D08A77260214EC211`。
- [x] 系统安装副本通过 pluginval 1.0.4 strictness 10、`Repeat=3`。
- Apple 专属项在未经过 macOS/Xcode/真机前必须保持“未验证”，不能写成已完成。

## 6. 完成定义

优化不以“代码更快”作为单独完成条件。只有当以下材料可复现时才算完成：

- 变更前后 benchmark 与 callback 分位数。
- 参考音频、频谱和数值误差结果。
- 零回调分配、零实时锁等待的自动化证据。
- Windows pluginval、Apple AUv3 host 和 Pi 4/5 实机结果。
- UI 固定插件表面和 Apple 紧凑窗口截图/像素检查。
- 每个未完成项目在本文件中保留明确状态和下一步。
