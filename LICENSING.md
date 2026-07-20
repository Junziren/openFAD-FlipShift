# Licensing Status

This document records the current licensing state. It is not a project
license and does not grant permission to copy, modify, or redistribute the
FlipShift source or binaries.

## FlipShift source

As of 2026-07-20, this repository does not contain a `LICENSE`, `LICENSE.md`,
`LICENSE.txt`, or `COPYING` file. Source availability alone does not grant an
open-source licence. A maintainer must choose and add a project licence before
public distribution.

## Upstream openFAD

The separate upstream openFAD project publishes its code under GNU AGPLv3:

https://github.com/willren5/openFAD/blob/main/LICENSE

That licence does not automatically apply to this FlipShift repository.

## JUCE 8.0.12

FlipShift links JUCE modules. JUCE 8.0.12 is dual-licensed under GNU AGPLv3
or the commercial JUCE licence:

https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md

This repository does not yet record which JUCE licensing route applies. A
release must not be published until the maintainer has selected, documented,
and complied with the applicable route.

## Other bundled dependencies

- The Windows build uses Microsoft WebView2 SDK loader components. Release
  materials must preserve the package's `LICENSE.txt` and `NOTICE.txt` terms.
- JUCE includes additional third-party components. For the formats built here,
  these can include the VST3 SDK under MIT terms and AudioUnitSDK under Apache
  2.0 terms. The notices shipped with the pinned JUCE source must be reviewed
  and preserved as required.

## Release gate

Before publishing source or binaries:

1. Choose and add the FlipShift project licence.
2. Record the selected JUCE AGPLv3 or commercial licensing route.
3. Assemble complete third-party notices for every distributed format.
4. Update the About UI, `vst3/WebUI/ui-spec.json`, and validation assertions.
