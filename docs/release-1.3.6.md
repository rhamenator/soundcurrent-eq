# SoundCurrent EQ 1.3.6

- `soundcurrent-eq --help` and `--version` now print and exit instead of starting the application.
- New `--preview` mode starts the window with isolated settings and audio processing off, for trying the interface or recording documentation.
- A read-only recording mode lets the Analysis page read a chosen output's monitor in a `--preview` session. It is off in normal use.
- `--preview` sessions cannot start audio processing and do not take over a running instance.

## Upgrade

Use **Quit**, then install 1.3.6 over the existing application. Settings, setups, and custom profiles are retained. The public Windows package continues to use the interim VB-CABLE route; the unsigned native-driver implementation remains preserved for later production signing.
