# SoundCurrent EQ 1.3.4

- Analysis settings now live on the tab and apply as you change them; the modal Settings dialog (which paused the analysis) is gone. Channel, palette, frequency scale and Automatic range are always visible; the rest is in a collapsible panel.
- Zoom and pan the frequency axis with the mouse wheel and drag in the spectrogram, spectrum, averaged and microphone views; double click resets. In the waveform the wheel scales the amplitude.
- The Tuner detects chords (major, minor, diminished, augmented, sus2, sus4, 7, maj7, m7) and shows the 12 pitch classes.
- Automatic range no longer bounces in the averaged and microphone views: it widens at once but narrows only after the signal stays lower for 3 seconds.
- The spectrogram and waveform scroll on the sample clock and no longer stutter; the waveform paints about 250 times faster, and the interface no longer stalls on audio-server queries.
- New Pink noise and White noise calibration signals alongside the sweep and tones. Pink is gentler on tweeters; white plays 6 dB lower than the chosen level.
- Settings & calibration lists devices and calibration first.
- Translations for the new text are included in all 33 language catalogs (linguistic accuracy unverified).

## Upgrade

Use **Quit**, then install 1.3.4 over the existing application. Settings and custom profiles are retained. The public Windows package continues to use the interim VB-CABLE route; the unsigned native-driver implementation remains preserved for later production signing.
