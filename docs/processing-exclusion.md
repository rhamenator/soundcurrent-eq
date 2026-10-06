# One active equalizer

SoundCurrent EQ and Studio share a per-user process lock on Linux and Windows. Only one can run, including in the background. Use **Quit** to release the lock; closing the window keeps the app running. Dead-process locks recover automatically.

Startup also checks recognized SoundCurrent, EasyEffects, PulseEffects, FxSound and Peace processes. Linux checks known PipeWire equalizer nodes. A GUI-thread check every two seconds stops SoundCurrent processing and restores its routing if a recognized conflicting equalizer appears later. Other programs are never terminated.

This detects known applications and routes, not every possible equalizer. Unknown programs, hidden Windows driver effects (including Equalizer APO without its frontend), hardware processing and effects under other users cannot be reliably identified. Disable these before using SoundCurrent. The two-second external check can permit a brief overlap; the shared SoundCurrent lock prevents simultaneous startup directly.

## Verification

Linux and native Windows tests cover cross-process refusal, recovery after a killed owner, normal release, recognized process identities and PipeWire node filtering. No physical Windows audio or interactive installer test was performed for these builds.
