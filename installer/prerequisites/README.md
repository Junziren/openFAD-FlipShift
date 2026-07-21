# Offline prerequisites

The single-file installer embeds these official Microsoft x64 redistributables:

- Microsoft Edge WebView2 Evergreen Standalone Installer:
  `https://go.microsoft.com/fwlink/?linkid=2124701`
- Microsoft Visual C++ 2015-2022 Redistributable:
  `https://aka.ms/vc14/vc_redist.x64.exe`

Validated package files for the 2026-07-20 build:

| File | Bytes | SHA-256 |
|---|---:|---|
| `MicrosoftEdgeWebView2RuntimeInstallerX64.exe` | 203841232 | `9915C97304977E2D11877B03E07A7FCDD10D4277B2297C5A5113D414FC6AE6B3` |
| `VC_redist.x64.exe` | 18731856 | `843068991DAAA1F73AD9F6239BCE4D0F6A07A51F18C37EA2A867E9BECA71295C` |

Both files were Authenticode-verified as signed by Microsoft Corporation before
the installer was compiled. Re-download and re-verify them for future releases;
do not reuse these hashes as permanent download pins.
