# Equipment profiles

Development builds add **Settings & calibration → Equipment profile library / editor**.
The library filters by equipment type, brand and family, then displays individual models.
Search also matches measurement conditions. There are 1,092 adapted published speaker EQ
profiles from 255 brands, plus a qualified Pyle PDA29BU electrical reference.

## Create, import, adjust and save

- **Create profile** starts a speaker, microphone or amplifier correction. Enter your
  brand, family, model and conditions. A manually authored curve is not a measurement.
- **Import JSON** accepts the version 2 format below. The existing amplifier import
  button still accepts its earlier format.
- **Import response text** accepts TXT, CSV, FRD and CAL data containing frequency in Hz
  and **relative measured response** in dB. A third phase column is ignored. Comment
  lines starting `#`, `*` or `;` and a `Frequency…` heading are accepted. Decimal point
  is `.`; frequencies must strictly increase. Absolute SPL must be normalized first.
  This path inverts a measured response: do not import already-inverted correction
  gains here. Use JSON filters for already-designed EQ.
- **Edit / save copy** shows the correction and any measured response supplied. Drag
  correction control points, or edit type, frequency, gain and Q in the table. Keyboard
  editing is supported. Add/remove filters up to 16. A changed editor prompts **Save /
  Discard / Cancel** when closed, including Escape and the window close button.
- **Save** always creates a custom copy with a new identity and parent provenance;
  reference and imported originals remain intact. **Export JSON** shares the copy.
- **Apply profile** previews measurement conditions and requires explicit acceptance.
  Applying a speaker or amplifier profile replaces that equipment's previous model
  correction, while listening EQ remains adjustable. A microphone profile replaces
  the generic natural-voice shape, retaining microphone high-pass and user tone/gain
  controls. Speaker processing can continue alongside microphone processing.
- **Clear imported equipment corrections** restores existing model selection/tone
  behavior. Apply and Clear participate in the main EQ undo history and respect Lock.
  Profile edits are previews until a saved copy is explicitly applied; no sweep or
  signal plays from the editor.

## Profile your own equipment

Connect a microphone and use the existing quiet sweep or discrete-tone check. Stop
music and competing noise yourself before starting a measurement. Keep the microphone
at the intended listening position; use conservative test level and the Stop control.
The existing worker checks noise and clipping before admitting a result.

A successful check now offers **Save system response profile**. This opens an editable
profile with measured relative levels and a proposed bounded correction. Save it,
then select it in the equipment library and preview before applying. The profile
identifies the combined **speakers + amplifier + microphone + room**, including any
playback EQ in the route. It does not identify the response of each part separately.
Do not stack a system correction measured through an existing correction as if it were
an independent speaker or amplifier curve. Prefer measuring a bypassed output route
when creating a reusable whole-system profile.

To profile a microphone independently, use an independently characterized sound source
or a calibrated reference microphone; otherwise import its manufacturer's calibration
file. Record the serial number, orientation, distance, polar pattern, gain and interface
in Conditions. Dayton and miniDSP calibration files are individual-unit/orientation
specific. The microphone processing profile is bypassed by the measurement worker;
this release does not subtract it from the raw room measurement.

An amplifier-only profile needs electrical response measurements with specified load,
input, tone settings and level. Use published measurements or suitable external
measurement equipment and import the results. This app does not turn a room sweep
into an amplifier-only measurement. No electrical measurement wiring is automated.

## What the curves mean

**Teal** is the correction computed from the filter coefficients at 48 kHz. It is not
an uncorrected equipment response. **Orange**, when present, is supplied measured
response relative to its reference. Most collected Spinorama profiles contain generated
EQ rather than redistributable original measurement arrays; they display the correction
and link to the source. Original third-party article plots are not bundled.

Text imports and saved system measurements fit an approximate inverse magnitude
response with 16 peaking filters using bounded coordinate descent in log frequency.
There is no phase reconstruction, impulse correction or claim of exact inversion.
The fit does not extrapolate outside measured coverage. Each filter is limited to
±6 dB and Q 0.1–6. Automatic fitting omits positive filters centered below 80 Hz;
manual custom editing can add bass later. Inspect the preview and clipping indicators:
a curve cannot create amplifier headroom or speaker extension.

## Published collection and provenance

The collector pins Spinorama commit `acc757bb98d63327092ee537bde25d9c227811f3`:
[upstream repository](https://github.com/pierreaubert/spinorama).
It examines 1,100 model directories and admits 1,092 profiles under our 1–16-filter
rules, including bounded resolution of alternate generated IIR files. Eight models
remain explicit gaps; their responses are not invented. Metadata is parsed
as Python AST literals; downloaded code is never executed. Downloads are bounded,
four concurrent requests, cached locally, and SHA256-attributed. Collection is a
manual development operation, not a background network task in the app.

Run `python3 scripts/collect-equipment-profiles.py` to reproduce the pinned collection.
The report is `data/equipment/collection-report.json`. Generated EQ is adapted under
the upstream GPL-3.0 license; the original plots and articles remain at their sources.
Families are unclassified when unknown; verified initial groups are
[JBL 3 Series MkII](https://jblpro.com/en-US/product_families/3-series-mkii.html),
[Yamaha HS](https://usa.yamaha.com/products/proaudio/speakers/hs_series/), and
[Kali Lone Pine](https://www.kaliaudio.com/s/LP-68-Users-Manual.pdf).

Further primary sources are linked in the library and recorded in
`data/equipment/measurement-sources.json`: Dayton/miniDSP serial calibration,
Neumann and Audio-Technica microphone graphs, Shure documentation, SoundStage
amplifier tests and the user's Pyle document. Sources requiring graph digitization,
individual serials or additional permission are research entries, not invented profiles.

The Pyle reference transcribes approximate hand-written electrical dB readings from
page 4 of the user's PDF, attributed to Stash's January 29, 2025
[original post](https://techtalk.parts-express.com/forum/tech-talk-forum/1506906-pyle-audio-products-pda29bu-and-audio-specs).
Conditions include 12 V DC / 5 A supply and nominal 8-ohm load; transcription and
apparatus uncertainty are retained. The plotted room result using homemade Mach One
speakers is not imported as a Pyle-only response. The reference ships with a small
conservative high-frequency shelf, not aggressive inverse low-bass boost; edit a copy
if desired. It has not been verified on this user's amplifier.

## File format and limits

```json
{
  "schema": 2,
  "kind": "microphone",
  "brand": "My brand",
  "family": "My series",
  "model": "My model / serial / orientation",
  "custom": true,
  "measurementSource": "",
  "conditions": "User-created example; replace with actual measurement conditions",
  "provenance": "My notes",
  "filters": [{"type": "PK", "frequency": 1000, "gain": 0, "q": 1}],
  "response": [[20, -2], [1000, 0], [20000, 1]]
}
```

Published profiles require an HTTPS measurement source. IDs can be omitted; an import
identity is derived from the file hash. Response arrays are optional; when supplied,
points must be strictly increasing, finite, within 10–40,000 Hz and ±200 dB. Filters
are PK/LS/HS, 20–20,000 Hz, ±6 dB and Q 0.1–6. Imports are limited to 1 MiB / 4,096
points. The personal library holds up to 256 profiles / 16 MiB with atomic file commits.
Profiles are treated as data, not commands; no profile can run code or download files.

## Verification

`ctest --test-dir build --output-on-failure` includes parser bounds, inverse-fit sign/
quality, the complete bundled catalog, atomic save/reopen, and a real modal editor test
that changes gain, closes, chooses Save and checks preservation of the original.
Existing shared UI, DSP and premium engine/WAVE tests remain enabled. Tests use isolated
settings and no hardware output. Linux is tested locally; the new shared Windows code
has not yet been built or exercised in a Windows VM. Hardware sweep accuracy and a
custom profile on a live microphone require separate verification.
