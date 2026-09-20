# FlipShift 界面与预设更新

本次更新：默认 Overlay、默认 High（2048）、1024 点分析器显示、可读文字、中文/英文切换、原生预设保存与文件导入导出。效果名称、模式索引、宿主参数 ID 和插件身份保持稳定。

## 预设格式

扩展名 `.flipshift`，内容为 UTF-8 JSON；导入也接受同结构 `.json`。格式标识 `openFAD.FlipShift.Preset`，版本 `1`。顶层包含 `format`、`version`、`pluginVersion`、`name`、`parameters`。

参数对象使用实际单位，而不是归一化旋钮值：`mode`、`shiftHz`、`scale`、`pivotHz`、`amount`、`widthQ`、`mix`、`outputGainDb`、`quality`、`pitchRoot`、`pitchScale`。模式及选择项使用既有数值索引，High 为 2。

不保存旁路、冻结音频、分析器显示或语言。加载预设保留当前旁路、解除冻结。全部参数完成校验后才应用；缺失、非法范围、非有限值、非整数枚举与未知格式版本均拒绝。最大输入文件 1 MB。旧 Max JSON 不是该格式。

用户库为 JUCE userApplicationDataDirectory 下的 `UnpureBloom/FlipShift/Presets`；Windows 为 `%APPDATA%/UnpureBloom/FlipShift/Presets`。文件使用 UUID 命名，中文名称保存在文件内。导入创建新 ID，不覆盖同名用户预设。保存采用临时文件替换。

原生桥新增 `presetCommand` / `presetState` / `presetResult`，操作为 list/load/save/saveAs/import/export。文件对话框异步；文件读写与 JSON 解析位于消息线程，音频 callback 不执行预设文件操作。窗口隐藏期间的操作结果保留到重新显示。

DAW 继续使用原来的 XML/APVTS 状态，新增可选预设名称、ID 和比较基线。旧工程不要求这些字段，声音参数恢复不依赖外部文件。语言是本机界面偏好，默认中文，存储不可用时回退到当前会话。

## 验证

- 源文件/UI 合同校验；Release CTest：DSP 与真实 WebView2 GUI 集成。
- 原生预设校验覆盖保存重读、中文名称、坏文件拒绝且参数不变、旧工程质量恢复、无外部文件时的工程恢复、修改标记和真实 C++/WebView 保存加载桥。
- 浏览器检查：中英两种语言 × 五种尺寸（1600×800、1000×650、621×844、390×844、320×280），另测 2x DPR、语言重开记忆、预设取消/保存/错误反馈、文本输入、图层与历史隔离。
- 浏览器性能参考：同为 High/Overlay、15 Hz 数据输入，1000×650/1x 与 1600×800/2x 两组。对比 git HEAD 的 192 点版本与当前 1024 点版本；结果位于 `vst3/build/ui-refresh-qa/performance.json`。这不是 DAW CPU 或实时音频 deadline 基准。
- 2026-09-18：Release CTest 2/2、源文件校验、原有 WebUI 回归和中英文扩展回归通过。Release pluginval 严格度 10、Repeat=3 通过；系统安装副本严格度 10 通过。完整 bundle 文件清单与安装副本一致。
- 当前 VST3 SHA-256：`e32317ce39b9b40556cca1e344d8575c8eb11a07202b6ad78c5679fafc878170`。
- 新安装包：`dist/ui-refresh/openFAD-FlipShift-Setup-0.1.0-UI-Refresh-Windows-x64.exe`；已构建，未重新进行安装器全流程/干净机验收。本机使用安装脚本更新并验证 VST3。

## 限制

系统文件对话框的手动选择/覆盖交互和真实 DAW 试听仍需人工验收。浏览器截图为明确标记的演示/模拟桥画面，不作为 DAW 截图。Apple/AUv3、干净机器安装和代码签名仍沿用原有未验证状态。


## 彩色瀑布图清晰度修正（2026-09-18）

瀑布图改用独立的 8192 点 Hann 窗输出分析，约 30 Hz 更新；消息线程计算左右声道频谱的功率平均，再传递 1024 个对数显示点。音频线程只写入预分配的环形缓冲与三缓冲快照，不执行新增 FFT。隐藏时停止采集，恢复与重置拒绝旧代数据。历史重绘与实时绘制均使用一致的离散取样，避免切换配色或尺寸后变糊；数据停止更新时暂停滚动。

处理 FFT、声音参数与宿主报告延迟未改变。48 kHz 时显示频率间隔从 23.44 Hz 改善到 5.86 Hz；代价是显示分析窗口约 171 ms，瞬态在时间方向仍有窗函数展宽，不能理解为所有细节均提升四倍。

新增测试覆盖相邻双音分离、反相立体声、非有限值、静音、重复帧、关闭/重置、并发快照，以及真实 C++ 到 WebView 的 waterfallFrame。Release CTest 2/2、源合同、中英文与 2x DPR 浏览器回归通过；浏览器额外验证专用瀑布数据的峰谷像素分离。pluginval Release 严格度 10 连续三次通过。截图为模拟桥测试图，真实 DAW 试听与主观视觉验收仍未执行。

系统安装副本 pluginval 严格度 10 通过，安装二进制 SHA-256 与构建产物一致。已重新生成同名 UI-Refresh 安装器和 ZIP；未做干净机安装器验收。
