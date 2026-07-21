# Windows installer

`OpenFADFlipShift.iss` builds the single-file Windows x64 installer for the
Release VST3 bundle.

The installer contains the complete Microsoft Edge WebView2 Evergreen offline
installer and the Microsoft Visual C++ 2015-2022 x64 Redistributable. The
plugin's HTML, CSS, and JavaScript are already embedded in the VST3 binary, so
the installed plugin does not require Node.js, Python, JUCE, the source tree, a
local web server, or internet access.

The prerequisite executables in `installer/prerequisites` are intentionally
ignored by Git. Download them from the official URLs documented in
`installer/prerequisites/README.md` before compiling the Inno Setup script.

The compiler output is written to `dist/installer` and is also ignored by Git.
The distributable ZIP should contain the generated Setup executable, the root
`README.md`, `README.zh-CN.md`, `LICENSING.md`, and a SHA-256 checksum file.

Do not distribute an installer until the project and JUCE licensing route in
`LICENSING.md` has been resolved.
