# Third-party components

SoundCurrent EQ source is GPL-3.0-only. Components keep their own licenses.

## Speaker correction data

The bundled numerical corrections are derived from Pierre F. Aubert's
[Spinorama](https://github.com/pierreaubert/spinorama) AutoEQ files at commit
`acc757bb98d63327092ee537bde25d9c227811f3`, published under GPL-3.0.
The original EQ files, license, exact source URLs, source SHA-256 values and
measurement attribution accompany the derived dataset in `data/speakers/`.
Corrections omit positive filters below 80 Hz, limit individual gains to ±6 dB,
and bound Q to 0.1–6. They are not manufacturer endorsements or room measurements.
Original review links identify the measurement authors; their articles and
plots have not been copied into this app.

## Qt

Windows packages dynamically link the official Qt 6.12.0 Qt Base libraries.
Qt is copyright The Qt Company Ltd. and other contributors. The included Qt
license texts are installed in `licenses/`. Qt Core, Network, GUI and Widgets
are available under LGPL-3.0 / GPL-3.0 as specified by Qt's license files;
third-party libraries incorporated by Qt retain their accompanying licenses.
You may replace the installed Qt DLLs with compatible modified versions.

Exact corresponding Qt Base source:
https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz
Source is also provided alongside the Windows release. The app's public source
and build instructions are at https://github.com/rhamenator/soundcurrent-eq.
Linux packages use their distribution's Qt libraries.

## Microsoft Visual C++ runtime

Windows packages include unmodified x64 Visual Studio 2022 redistributable
runtime DLLs, under Microsoft's redistribution terms. The Windows Universal
CRT is provided by Windows. App-local DLLs are updated with application releases.
https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

## VB-CABLE

The optional standard signed VB-CABLE driver retains VB-Audio's separate
license. See `packaging/windows/VB-CABLE-NOTICE.txt`. Paid second-cable packages
are not bundled and must be obtained and installed separately by the user.

## Expanded equipment profile catalog

`data/equipment/spinorama.json` adapts 1,087 generated AutoEQ profiles from
pierreaubert/spinorama commit acc757bb98d63327092ee537bde25d9c227811f3, GPL-3.0.
The upstream license is included in `data/equipment/LICENSE`. Every profile
records its exact source URL/hash, measurement origin when provided, and gain/Q
adaptation. The collector is `scripts/collect-equipment-profiles.py`. Original
review articles and plots are not included. Family references and gaps are
explained in `docs/equipment-profiles.md`.

The Pyle reference transcribes approximate factual electrical readings from
Stash's January 29, 2025 Parts Express forum post, supplied by the user as a PDF.
It retains source attribution, PDF hash, measurement conditions and uncertainty.
No original PDF pages, photographs or forum prose are redistributed. Other
microphone/amplifier source links are research metadata, not bundled curves.
