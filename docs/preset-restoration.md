# Listening state and desktop integration

Flat is the default on first startup. Subsequent launches restore the listening preset and its actual frequency, gain and Q settings. Manual edits reopen as Custom. The state uses stable preset identifiers independently of the interface language. Invalid saved state falls back to Flat; removed or changed named presets retain the saved curve as Custom.

The application, window and tray use embedded logo pixels rather than relying on a desktop icon theme. Tray host caching and appearance can still vary by desktop environment.

EQ and Studio remain mutually exclusive system-wide processors. A DAW process or ordinary track/monitor EQ node is not a system-wide equalizer and does not trigger the equalizer shutdown guard. Avoid unintentionally applying monitoring correction twice when using DAW monitor EQ alongside a system equalizer. Automatic detection of an active DAW monitor-EQ stage is not implemented by this change.
