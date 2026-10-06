# Native Windows driver bookmark — 2026-10-06

The native SoundCurrent driver is deferred until publisher enrollment and signing are affordable. Preserve `native/windows/virtual-driver`, `native/windows/driver-manager`, `packaging/windows/native-audio-setup.ps1`, and the native installer variant. Do not discard the WASAPI route, guardian, volume, hotplug, or mutual-exclusion improvements: these also serve VB-CABLE.

Frozen source and build checksums: `.cache/windows-review-source/review-handoff.json`. The source review archive in that directory includes the uncommitted native implementation before the cable adaptation. Keep this immutable archive and its manifest. Nothing has been published or pushed.

Resume native delivery using `docs/windows-signing.md` and `native/windows/virtual-driver/COMPLETION-AUDIT.md`. The driver remains unsigned and its own installed kernel runtime has not been verified. The VB-CABLE build is an interim route, not proof of native driver completion.

Local Git bookmark: `bookmark/windows-native-driver-2026-10-06` (`97885ab907512e0f6851704792911f7da192fbe5`). It was created from the frozen source archive with a temporary index; the current branch, real index, and working files were left in place. No push was performed.
