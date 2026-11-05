# SoundCurrent EQ 1.1.2

- Adds 59 correction profiles shared by EQ and Studio: 42 microphone curves (including lavaliers and USB) and 17 amplifier/load-state curves. Model variants and measurement conditions remain explicit; models without correction curves are excluded.
- Imported profiles visibly replace the previous model in the main selector and Profile details, persist across reopening, and show an applied checkmark in the manager. Clear selects None; each selection is individually undoable.
- Selecting a usable equipment profile applies it immediately. None clears the selected equipment route. Fresh installations start with no equipment correction and microphone EQ off; saved choices are retained on upgrades.
- The equipment manager scrolls within the display and keeps Close accessible. Accidental wheel movements over the closed profile selector do not change audio. Importing, saving, opening or filtering the library does not apply a correction.
- Includes source attribution, measurement uncertainty, source hashes, reproducible correction admission, and an experimental AR-2ax acoustic-power custom import in the source examples. Typical/relative profiles are not individual-unit certification.

Install over the existing application; Quit the active equalizer first. Closing its window may leave processing running. Presets and settings are retained. Only one equalizer should process audio at a time.

Linux installers cover compatible Ubuntu/Debian systems, Fedora 44+, and RHEL 10-compatible systems. Windows retains the existing interim VB-CABLE route, setup/reboot and recovery guidance; the application/installer remains unsigned. Passing automated UI and installer checks does not establish physical microphone accuracy, production acceptance, or arbitrary-channel hardware qualification.
