# Analysis and microphone-reference profiles

The **Spectrogram** tab runs only while selected and visible. Leaving the tab or
closing the window to the tray stops its capture process and analysis work.
Returning resumes analysis; **Reset** starts a new average and clears peak hold.
Capture also pauses while the analyzer settings or profile editor dialog is open.
The reference is retained until replaced or invalidated by changing the analysis
rate, FFT size or channel selection. Nothing records audio to disk.

## Views

- **Spectrogram:** a scrolling, color-coded frequency history. Frequency increases
  vertically; time advances toward the right. Stereo mode shows separate lanes.
- **Spectrum:** a connected scope-style trace of level against frequency, with
  optional left/right traces, average and peak-hold overlays.
- **Waveform:** amplitude against time, including narrow peaks when many samples
  share a display pixel. This view does not run an FFT.
- **Tuner:** pitch, note, cents offset and standard instrument tuning targets.
  It does not require an FFT. This is intended for a dominant monophonic tone, not polyphonic transcription.
- **Vectorscope:** stereo relationship and correlation. This view does not run an
  FFT; an in-phase mono signal is vertical and opposite-polarity stereo is horizontal.
- **Averaged spectrum:** a smoothed frequency trace with optional captured-reference
  overlay. This is the upstream app's "room" display, renamed because a digital
  playback spectrum alone does not measure a room.
- **Microphone response:** a cumulative power average. With a captured reference,
  the graph shows the relative response, normalized around the median, in dB.
  Unexcited/noise-dominated bands are not treated as reliable response points.

The **Source** selector offers playback monitoring and available microphones.
Playback is a passive monitor of the physical output used by the equalizer, or
the system output when processing is off. Microphone capture uses the selected
input directly. Neither operation changes default devices or feeds a microphone
back to speakers. Disconnecting a selected microphone stops capture and is shown
in the selector; reconnecting permits capture again when this tab is active.

Analysis uses a two-channel 48 kHz capture stream. Mono devices may be expanded
and multichannel devices downmixed by the audio server/Windows audio engine.
Studio's arbitrary-channel processing is separate; this tab does not certify or
provide separate spectra for every physical theater channel. Mix spectra average
channel power so opposite stereo polarity does not hide sound; a mono waveform
uses the signed channel mix.

## Settings

Open **Settings** within the analyzer. Fundamental changes are staged until
**Apply**; **Close** discards unapplied changes. Apply stays visible. These are
visualization settings and do not change the equalizer's processing rate, output
gain or latency.

The controls carry over the C++ spectrogram app's analysis rate (16–96 kHz),
FFT size (512–8192), six palettes, linear/logarithmic/octave frequency mapping,
frequency limits, mono/stereo/automatic layout, left/right/mixed selection,
dB/linear amplitude, grid, peak hold, averaging, automatic range, display
sensitivity and ceiling, history length, scroll speed, note guides, transient
markers, music note display and standard instrument tunings.

- FFT windows longer than 250 ms are rejected, matching the upstream live-app
  guard. Maximum displayed frequency is limited to both analysis Nyquist and the
  actual 24 kHz capture Nyquist. Upsampling does not create ultrasonic data.
- Analysis-rate conversion uses a bounded, windowed-sinc resampler. Display work
  is limited to at most 25 updates/second and slower where FFT hop timing requires
  it. Audio processing continues independently if the GUI drops a display frame.
- **Freeze display** holds the picture and stops visualization FFT work while the
  selected tab's capture can continue. It does not pause music or change EQ.
- **Save preset** writes versioned JSON. **Load preset** accepts that JSON and
  legacy `.preset`/`.txt` files from the C++ app. Loading stages the settings for
  review; it does not silently apply them. Unknown/duplicate legacy keys and
  malformed, out-of-range or oversized input are rejected.
- A settings preset contains display/analysis choices, not audio, microphone IDs
  or a captured reference. Source selection is kept separately in local settings.

The standalone spectrogram application's recording/playback transport, media
metadata integration and file exports are not duplicated in the equalizers.
The in-app analyzer provides the requested views and settings; it does not claim
full standalone-app feature parity or polyphonic chord-recognition accuracy.

## Creating a relative microphone profile

1. Select **Microphone response** and the reference source. Use a known,
   repeatable, broad-band signal at a comfortable level. Disable automatic gain,
   noise suppression and adaptive effects when comparing linear responses; this tab does not emit
   test tones automatically. Hold placement, source and level constant.
2. Let the average accumulate. **Capture reference** becomes available after at
   least 25 analysis frames. Capturing the reference starts a fresh average.
3. Select the target microphone and collect another average under the same
   conditions. The reference stays available as you change input sources.
4. **Create microphone profile** proposes a relative response and bounded
   correction filters, and opens the existing equipment-profile editor. Inspect
   and edit the curve, conditions and provenance, then save a copy if appropriate.
   Saving does not apply it. Select it in microphone setup when ready to use it.

A comparison is rejected if there are too few usable broad-band points or too
little frequency coverage. There must be at least six retained points spanning
at least two octaves, with reference and target excitation above the measurement
floor. These thresholds are validity guards, not measurement certification.

The result includes changes in source material, room, placement, reference
microphone, body and clothing. A recording made with one microphone alone cannot
separate these factors. A calibrated reference and controlled, unchanged source
make the comparison more useful; ordinary music changing between the two averages
can create a misleading curve. Conditions are stored with the profile, and the
editor requires an explicit save.

Known component responses can in principle be removed from a controlled chain
measurement to estimate the remaining response. Several unknown components leave
an underdetermined combined residual, not a unique equipment profile. Published
speaker/amp curves, directivity and placement add uncertainty. The estimator below performs this subtraction but never certifies the residual
as an isolated equipment measurement. For acoustic room calibration use the separate
**Speaker & room calibration** dialog and review its proposed correction.

## Implementation and validation boundaries

The FFT planning and palette renderer are reused unchanged
from GPL-3.0 [Audio Spectrogram](https://github.com/rhamenator/audio-spectrogram),
commit `f6084d95f6f0b7db8b0bed73797439a3ee335841`. Its repository is not modified.
The Qt UI, bounded capture snapshots, resampling and reference workflow are local
integration. Analysis allocates and paints outside the real-time audio callback.
History, PCM snapshots and FFT sizes are bounded; inactive tabs create no analysis
FFT, paint timer or capture stream.

Windows monitoring uses shared-mode WASAPI loopback on the selected render
endpoint; the existing microphone recorder's default behavior is retained.
Microsoft documents the [loopback and automatic conversion stream flags](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants).
Linux monitoring reads the corresponding PulseAudio/PipeWire monitor source.
Synthetic DSP/UI and silent virtual-output tests cannot certify every physical
Windows device or multichannel downmix. The development host has stereo output.

Translation catalogs cover the new controls in all 34 existing languages;
linguistic accuracy remains unverified.

## Estimate a missing equipment response

1. Import a controlled **complete-chain measured response** with **Import response
   text** in the equipment profile manager. This is the response to a known test
   signal, not an arbitrary music spectrum and not already-inverted EQ gains.
   A saved speaker/room measurement containing response points can also serve as
   the chain. Keep its measurement conditions, signal, processing and placement.
2. Select that profile and choose **Estimate profile**. Profiles without measured
   response points cannot be used (at least six points are required per input). Choose the target equipment type, describe
   the conditions, and check the known components that were actually in the chain.
   Use the type, manufacturer and search filters to find components; checked items
   stay selected when hidden, and the summary lists them. All other effects must
   be known or cancel to attribute the residual to one device.
3. Acknowledge that unknown effects remain, then choose **Review estimated profile**.
   The app subtracts measured responses in dB, interpolated on logarithmic frequency.
   It uses the common measured range only (at least two octaves), never extrapolates,
   and rejects repeated components or subtraction of a measurement from itself.
4. The existing editor shows the **Estimated profile** notice, residual response
   (orange), and independently editable corrective filters (teal). Rename it and
   adjust the correction if desired. **Save copy** saves a custom profile; Cancel
   preserves the sources. Neither estimation nor saving applies the correction.
   Use the manager's explicit selection/Apply action when ready. Preview, when
   available, is temporary and restores the previous correction on closing.

Estimated status survives renaming, editing, export/import and restart. It appears
in the equipment selectors, custom library, status and editor. Derivation metadata
retains the method, source identities, conditions, SHA-256 source-profile hashes,
frequency coverage and timestamp. A source may itself be estimated; this is recorded
and does not remove its uncertainty. Input curves are not modified. The result's
response and provenance are distinct from its editable filters.

Cumulative microphone reference comparisons are also labeled estimated and retain
reference/target names, analysis configuration and averaging count. An unknown
reference microphone, clothing, positioning or room change remains in that result.

Published model curves may be normalized differently or measured with a different
load, direction, distance or device sample. The result is a **relative combined
residual**, not an absolute sensitivity calibration or phase/impulse-response model.
Constant gain offsets therefore depend on the input normalization. Deep acoustic
nulls cannot safely be repaired by large boosts. Existing conservative correction
limits remain in force; a fitted EQ is an approximation, not a claim of exact inversion.

Estimated profiles use equipment JSON schema **4**, while existing schema 2/3 files
remain supported. Older app versions reject schema 4 rather than silently present
an estimate as a directly measured profile; keep both equalizers updated when
sharing these files. Linguistic accuracy of the new translations remains unverified.

### Spectrogram display detail and units

The rolling spectrogram uses the plot's physical pixel dimensions, including
high-DPI display scaling, rather than stretching a fixed 600 × 256 bitmap.
Resizing preserves the visible history. Storage is bounded at 2048 × 2048 pixels
(16 MiB for the history image), and smooth scaling handles larger displays.
Frequency rows interpolate between FFT bins when enlarged and preserve narrow
peaks when several bins share a row. Time columns blend adjacent analyzed frames;
this improves drawing quality without claiming additional measured time detail.

Frequency axes use readable tick positions and explicit units: **500 Hz**,
**1 kHz**, **12.5 kHz**, with localized decimal separators and no scientific
notation. The spectrum, tuner, cursor readouts and analysis-rate selector use the
same convention. Numeric frequency-entry fields retain explicit Hz units so their
editable values and step sizes stay consistent.

The image resolution does not change the underlying FFT resolution. For closer
frequency separation, increase **FFT size** in Settings (up to 8192, subject to the
250 ms window limit), or narrow the displayed frequency range. Audio processing,
the analysis cadence and the inactive-tab shutdown behavior are unchanged.

Axis labels follow the selected frequency scale in every frequency-based view:
**Linear** uses equal Hz intervals; **Logarithmic** uses multiplicative frequency
positions with readable 1/2/5 ticks; **Octave** uses equal octave steps anchored
at 1 kHz (for example 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz). Crowded octave axes
skip whole octaves. Linear and octave axes omit non-aligned endpoint ticks so
those endpoints cannot suggest an irregular interval. Hz/kHz formatting is the
same in every mode and does not change the selected scale.
