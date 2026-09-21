# openFAD FlipShift — macOS 测试包

支持 macOS 11 或更高版本，包含 arm64 + x86_64 通用二进制：

- VST3：`/Library/Audio/Plug-Ins/VST3/openFAD FlipShift.vst3`
- AU：`/Library/Audio/Plug-Ins/Components/openFAD FlipShift.component`

关闭音频宿主后运行 `.pkg` 安装器，再重新扫描插件。
也可从 ZIP 中的 `Library/Audio/Plug-Ins` 取出完整插件 bundle，
放到本机相同目录，或用户目录 `~/Library/Audio/Plug-Ins` 对应位置。
不要拆开 bundle，也不要将 ZIP 当作插件导入。

此包使用临时 ad-hoc 代码签名；安装器没有 Developer ID 签名，
未经过 Apple 公证，因此不是已公证的正式发行包。
macOS 可能阻止首次打开。仅在确认下载来源可信后，
按系统“隐私与安全性”提供的提示处理；不要关闭全局 Gatekeeper。

GitHub Actions 构建 VST3/AU，检查双架构、代码签名与安装包文件列表，
并在 Apple Silicon runner 执行 DSP 测试。
Intel 原生运行、WKWebView 界面、Logic/Ableton 扫描和声音处理、
预设文件对话框及干净机安装仍需 Mac 实机验收。
本流水线不打包 AUv3 或 iOS 应用。

## 在 GitHub 重新打包

打开 Actions → macOS package → Run workflow。
成功后从运行页面的 Artifacts 下载打包结果，包含 PKG、ZIP、SHA-256、
源码 commit 与安装内容清单。不会自动发布 GitHub Release。

正式公开发行前应配置 Developer ID Application / Installer 证书，
完成插件签名、安装器签名、Apple notarization 和 stapling。
