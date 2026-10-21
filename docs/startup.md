# Start at sign-in

The Settings and calibration page contains **Start when I sign in**, disabled unless this installation already owns the login registration. Selecting it configures a per-user launch after desktop sign-in, without administrator privileges. It is not a boot-time system service.

EQ and Studio use one shared entry: enabling either replaces the other, so they do not both launch at sign-in. Disabling or uninstalling one must not remove the other application's registration. The existing runtime processing guard also remains active.

Linux uses `$XDG_CONFIG_HOME/autostart/soundcurrent.desktop` (normally `~/.config/autostart/`). It contains a quoted executable command and `TryExec`, so a removed executable is skipped. Package removal retains user preferences; reinstallation at the same path can reuse the entry. Uncheck the option before removing the package to remove the entry, or delete that specific file manually.

Windows uses the `SoundCurrent` value in the current user's Run registry key. Both cable and native installers remove it on uninstall only if it refers to their own installed executable. In-place updates preserve it.

`--background` starts without a visible main window when a system-tray icon is available. If the desktop has no tray support, the window opens so the app remains accessible. Closing a window and quitting still have their existing meanings. Startup registration does not override saved processing preferences.

Tests use temporary Linux config directories or a unique Windows test registry key. They test exclusive selection, enable/disable ownership and unsafe path rejection without changing the user's real startup registration. Actual sign-in qualification remains a separate platform test.
