# WebView and AUv3 architecture

## Reference review

The implementation was checked against these public projects on 2026-07-17:

- Sound-Field at commit `19946026e51ec3ea725b5d1a4985ede319367e3c`:
  JUCE 8 native integration, frontend event subscriptions, parameter relays,
  timer-batched analysis data, and a WebGL-capable audio visualization.
- sfizioso-player at commit `1bd9dda01ebcbb59b1722f9c10e92053ab5ff35f`:
  a clean separation between processor, WebView editor, shared bridge code,
  embedded Vite resources, and reusable UI components.
- JUCE 8.0.12 `WebViewPluginDemo.h`: explicit WebView2 selection, a resource
  provider, native integration, host-to-JavaScript events, and WebView2 user
  data outside the plugin bundle.
- Cmajor Pro54 at commit `44c2120d126af01b2a518e811c0d73f7b8341cab`:
  a declarative view manifest, parameter metadata as the UI contract, and a
  frontend kept independent from the DSP implementation.

The resulting design uses the same strong boundaries without taking on a
React runtime or a Node-based production build. The three frontend files are
embedded by `juce_add_binary_data`, so release plugins do not access a server,
the filesystem, or the public network.

## Native plug-in surface policy

The WebView is used only as a high-quality renderer for an audio plug-in
surface. It must not expose ordinary web-page behavior:

- The document root is fixed to the editor viewport and never scrolls. The
  minimum supported editor is 320x280; compact sizes use ANALYZE/CONTROL tabs,
  with scrolling restricted to the explicit parameter tool region.
- Native mode disables text selection, context menus, browser drag/drop,
  internal navigation, new windows, and refresh/source/location/zoom shortcuts.
- About links are exact-allow-listed native commands. They never navigate the
  embedded document.
- Hiding the native editor sends `surfaceVisibility` to stop the frontend's main
  `requestAnimationFrame` loop, active gestures, and knob particles. Analyzer
  production remains enabled for the full Editor lifetime and is disabled only
  by Editor destruction. Showing the surface resumes from preserved waterfall
  history without changing host parameters.
- PROCESS exposes 24 concise transforms. Glitch renders its selected
  frequency band as a separate DOM overlay that never writes into the waterfall
  Canvas. Pitch Map exposes 12 roots plus Major/Minor, with Minor using
  natural-minor intervals; its ROOT/SCALE module replaces AXIS in the same fixed
  five-column slot and its key readout never synthesizes analyzer energy.
- `tests/validate-webui.py` exercises 1000x650, 621x844, 390x844, and 320x280,
  including fixed root bounds, compact tabs, text fit, waterfall pixel and
  brightness changes, meter fills/readouts, native-surface event suppression,
  and viewport/analyzer screenshots.
- Windows CTest also runs a real WebView2 GUI integration executable built from
  the same Processor, Editor, parameter, DSP, and WebUI sources. It drives
  deterministic audio and reads the actual DOM; a representative pass records
  46 analyzerFrame events, 190 waterfall columns, 192 input plus 192 output
  points, and -10.5 dB input/output meters. It does not load the installed VST3
  bundle and cannot validate Ableton's wrapper, scanner, or cached editor.

## Browser backends

- Windows: `WebBrowserComponent::Options::Backend::webview2` is selected
  explicitly. `NEEDS_WEBVIEW2 TRUE` and static WebView2 loader linking prevent
  JUCE from falling back to the legacy Internet Explorer backend.
- macOS and iOS: JUCE uses the platform WKWebView implementation.
- Linux: JUCE uses its WebKit backend when the required WebKitGTK development
  and runtime packages are available.

The native bridge exposes three frontend-to-C++ events:

- `uiReady`: requests the initial parameter and analyzer snapshots.
- `parameterGesture`: carries `begin`, `value`, and `end` phases so host
  automation records a coherent gesture instead of disconnected values.
- `openExternal`: accepts only the exact About-link allow list. Desktop builds
  pass approved URLs to the system browser; iOS AUv3 builds ignore the request
  because browser launching through `UIApplication` is unavailable to app
  extensions.

C++ emits `parameterState` only after values change; the snapshot includes the
appended `pitchRoot` and `pitchScale` parameters without renumbering existing
parameter IDs or the original mode values 0-21. Analyzer frames are
logarithmically reduced to 192 points and sent at 15 Hz. The
`surfaceVisibility` event stops and resumes the frontend animation lifecycle
with the native editor. Rendering is performed by two lightweight Canvas 2D
surfaces; no graphics work runs on the audio thread.

## AUv3 configuration

`OPENFAD_BUILD_AUV3` defaults to `ON`. iOS builds expose only `AUv3` and
`Standalone`; macOS builds expose `VST3`, `AUv3`, and `Standalone`. Enabling an
AUv3 format with a non-Xcode generator fails during configuration. JUCE embeds
the AUv3 app extension into the standalone app, which acts as the required
installable container. The target is compiled with
`APPLICATION_EXTENSION_API_ONLY=YES`; the standalone wrapper has microphone
permission text and targets iPhone/iPad device families. The processor remains
an audio effect, accepts mono or stereo audio, has no MIDI dependency, restores
APVTS state, and uses only sandbox-safe embedded UI resources.

Example iOS configuration from macOS with Xcode and an iOS-capable JUCE tree:

```sh
cmake -S vst3 -B vst3/build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DOPENFAD_BUILD_AUV3=ON \
  -DJUCE_PATH=/path/to/JUCE
cmake --build vst3/build-ios --config Release \
  --target OpenFADFlipShift_Standalone OpenFADFlipShift_AUv3
```

Signing needs an Apple Development team configured in Xcode. AUv3 production
acceptance still requires a macOS runner and physical iPhone/iPad testing; a
Windows build can validate shared C++ and WebView2 code but cannot compile or
sign an Apple app extension. Therefore the CMake and source configuration is
implemented, but Apple/Xcode compilation, signing, GarageBand, additional AUv3
hosts, WKWebView behavior, and physical-device results are all unverified.

## AUv3 acceptance checklist

1. Open the standalone container on iPhone and iPad and confirm the embedded
   UI loads without network access, document scrolling, selection, context
   menus, navigation, or browser shortcuts at sizes down to 320x280.
2. Validate the extension in GarageBand and at least one additional AUv3 host.
3. Exercise mono and stereo buses, all three quality settings, all 24 PROCESS
   values, Pitch Map roots C through B in Major/Minor (natural-minor intervals),
   state restore, host automation, bypass, freeze, rotation,
   background/foreground, and sample rates exposed by the device.
4. Confirm memory warnings do not unload state and that reopening the editor
   does not leak WKWebView processes or duplicate bridge listeners. Verify Band
   Glitch keeps its effect and overlay inside the selected band, and that neither
   the overlay nor the Pitch Map readout alters stored waterfall pixels.
5. Profile the High quality mode on the oldest supported device before setting
   the final deployment target.
6. Confirm About links remain inert inside the AUv3 extension and do not call
   extension-prohibited application APIs.

All six items above remain open until they are run on macOS/Xcode and physical
Apple hardware. Windows Release, WebView2 runtime tests, and pluginval results
must not be presented as AUv3 validation.
