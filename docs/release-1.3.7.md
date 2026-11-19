# SoundCurrent EQ 1.3.7

- The Tuner has a scrolling note display (about C1 to C6, time runs left to right). Wheel scrolls it, Ctrl+wheel zooms the octave range, double-click follows the music again, and a scroll bar shows the visible octaves. The names of the notes sounding now appear at its right edge.
- Notes that sound about equally in both channels (usually the lead) are teal; notes mostly in the left channel are blue and mostly in the right channel are amber (usually backing). Mono recordings count as all centre.
- The big note is the loudest note in the spectrum and must stand out from the background, so noise shows a dash. Low notes are measured from their harmonics for better precision. The readout and gauge stay in place and dim for about a second when a note disappears.
- Chords are named on their own line (**Show chords** turns it off), including chords played as an arpeggio.
- New **Voice** entry in the Instrument list. Choosing an instrument focuses the Tuner on its range, and the gauge now reports the actual note, not the nearest open string. The Instrument choice is also on the Tuner view.
- Optional **Show now playing** (Analysis settings, off by default, Linux only) shows the artist and title that the desktop media player reports. It reads the standard media-player interface read-only and sends nothing anywhere. The Linux package now depends on libqt6dbus6.
- Translations for the new text are included in all 34 catalogs (linguistic accuracy unverified).

## Upgrade

Use **Quit**, then install 1.3.7 over the existing application. Settings, setups, and custom profiles are retained. The public Windows package continues to use the interim VB-CABLE route; the unsigned native-driver implementation remains preserved for later production signing.
