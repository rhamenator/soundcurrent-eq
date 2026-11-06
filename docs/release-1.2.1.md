# SoundCurrent EQ 1.2.1

- Restore the selected listening preset and its actual frequency, gain and Q settings after restarting. Flat remains the first-start default. Edited curves reopen as Custom, saved preset names are retained, and nine-band curves round-trip correctly. Invalid saved state safely falls back to Flat; removed or changed named presets retain their saved curve as Custom.
- Use embedded PNG logo pixels for the application, window and tray, avoiding desktop theme-name resolution and ICO-plugin dependencies for tray rendering.
- Allow ordinary SoundCurrent DAW track/monitor processing alongside the equalizer. EQ and Studio remain mutually exclusive system-wide processors. Automatic detection of double correction by DAW monitor EQ is not implemented; avoid applying the same monitoring correction twice.
- Improve Windows UI test diagnostics and keep restart-test windows off the limited Windows thread stack.

Install over the existing version after using **Quit app**. Closing the window can leave processing running in the background. Settings and custom equipment profiles are retained. A listening preset lost by an older version must be selected again once; this version preserves subsequent choices.

Linux downloads include the easy `.run` installer, Ubuntu/Debian DEB, Fedora 44 RPM and RHEL 10-compatible RPM. Windows includes an x64 installer using the existing interim VB-CABLE route. Applications/installers remain unsigned; future native-driver code is preserved. See the accompanying installation and audio-driver recovery guides.

All 34 interface catalogs remain structurally complete; linguistic accuracy is unverified. Physical Windows A/V latency and arbitrary-channel output still require hardware verification; the development host has stereo output. Desktop tray-host appearance can vary.
