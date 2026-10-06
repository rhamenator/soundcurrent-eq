# Listening enhancements

Reference: FxSound documents Clarity, Ambience, Surround Sound, Dynamic Boost and Bass Boost: https://forum.fxsound.com/t/how-to-use-fxsound-effects/6332 (checked 2026-10-05). SoundCurrent implements its own algorithms, without claiming an identical sound or copying FxSound DSP.

| Control | SoundCurrent behavior |
|---|---|
| Clarity | High-frequency detail, up to approximately +6 dB |
| Bass Boost | Low-frequency weight, up to approximately +9 dB |
| Ambience | Damped room reflections from four independent feedback delays per side |
| Surround Sound | Mid/side stereo widening, preserving mono sum; no discrete surround upmix |
| Dynamic Boost | Stereo-linked compression plus makeup gain and an instantaneous peak ceiling |

Amounts start at zero, which is exact bypass. Amount changes ramp over 20 ms to soften transitions. No lookahead latency is added. This is a listening enhancer, not a transparent mastering limiter or a recovery of lost source information.

EQ exposes amount sliders on the Equalizer page, with reset, lock, grouped Undo, persistence and saved listening presets. Built-in listening presets reset enhancements to zero; saved custom presets include their effect amounts. Older saved presets remain dry.

Studio exposes these effects on the Studio page. **Show advanced controls** expands clarity/bass corner frequencies, ambience decay/damping, maximum width, dynamics threshold/ratio/attack/release/makeup/ceiling. These settings participate in Studio Undo, save/open, state validation and offline rendering. Dry resets enhancements along with the existing delay/reverb effects. Older schema-1 Studio files without the optional `enhancements` array open dry.

Enhancements process front L/R (the first two routed channels), or mono. Studio's other channels retain their per-channel EQ, gain, mute/solo, routing, delay and reverb; LFE is not bass-boosted or stereo-widened by this front-pair stage. They apply to speaker playback, not microphone correction or calibration tone generation.

Pipeline: EQ / equipment correction → time effects where enabled in Studio → enhancements → post gain / balance / channel trim → existing output clipping protection. The dynamics ceiling is before post gain: positive post gain can exceed it. Existing Equalizer-page level meters estimate EQ/gain response and do not model enhancement dynamics or ambience; Studio channel meters report processed peaks.

Linux EQ loads the bundled LADSPA module in its existing PipeWire filter-chain; live `Props` changes update controls without restarting routing. Windows EQ and both Studio backends use the same framework-independent C++ effect implementation. Processing does not allocate, lock or perform I/O. Effect buffers are prepared at construction; parameter values are finite and bounded.

## Verification

DSP tests check exact zero bypass, selective bass/clarity response, width and mono-sum preservation, ambience tails/reset, dynamics quiet gain/ceiling/live updates, invalid settings and no processing allocations. Existing DSP integration tests exercise enhancement-enabled and bypassed EQ. Studio tests check enhancement-enabled planar/interleaved equivalence, other-channel preservation, bypass, advanced UI edits, Undo and state round trips.

`python3 tests/pipewire_enhancements.py [path/to/soundcurrent-eq]` (EQ repo) starts a private PipeWire server, links synthetic source/destination streams, loads the full graph and reads back changed effect controls. It never connects to the host server or physical devices.
