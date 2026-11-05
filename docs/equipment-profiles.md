# Equipment profiles

Open **Settings & calibration → Equipment profile library / editor**.
The library filters by equipment type, brand and family, then displays individual models.
Search also matches measurement conditions. There are 1,092 adapted published speaker EQ
profiles from 255 brands, plus a qualified Pyle PDA29BU electrical reference.

## Create, import, adjust and save

- **Create profile** starts a speaker, microphone or amplifier correction. Enter your
  brand, family, model and conditions. A manually authored curve is not a measurement.
- **Import JSON** accepts the version 2 format below and version 3 for larger corrections. The existing amplifier import
  button still accepts its earlier format.
- **Import response text** accepts TXT, CSV, FRD and CAL data containing frequency in Hz
  and **relative measured response** in dB. A third phase column is ignored. Comment
  lines starting `#`, `*` or `;` and a `Frequency…` heading are accepted. Decimal point
  is `.`; frequencies must strictly increase. Absolute SPL must be normalized first.
  This path inverts a measured response: do not import already-inverted correction
  gains here. Use JSON filters for already-designed EQ.
- **Edit / save copy** shows the correction and any measured response supplied. Drag
  correction control points, or expand Advanced to edit type, frequency, gain and Q in the table. Keyboard
  editing is supported. Add/remove filters up to 32. A changed editor prompts **Save /
  Discard / Cancel** when closed, including Escape and the window close button.
- **Save** always creates a custom copy with a new identity and parent provenance;
  reference and imported originals remain intact. **Export JSON** shares the copy.
- **Selecting a profile applies it immediately**, without an Apply confirmation.
  Opening the manager, changing search/taxonomy filters, importing or saving does
  not change audio. Scrolling over the closed model selector does not select a
  profile; open its popup to scroll the list. The manager scrolls within the
  available screen space and keeps Close accessible. The **Apply profile** button can reapply the displayed saved
  profile. A checkmark and model name confirm the applied choice. The main speaker
  or amplifier selector and its details show the imported correction, including after
  reopening the app; manufacturer/type filters preserve it. Choose **None — use my own EQ** to clear the selected equipment type;
  with **All equipment** selected, None clears all imported equipment corrections
  and legacy speaker/amplifier model selections. Listening EQ remains in place.
  Applying a speaker or amplifier profile replaces that equipment's previous model
  correction, while listening EQ remains adjustable. A microphone profile replaces
  the generic natural-voice shape, retaining microphone high-pass and user tone/gain
  controls. Speaker processing can continue alongside microphone processing.
- **Clear imported equipment corrections** clears equipment corrections and selects
  None for speakers and amplifiers, preserving your listening EQ. Apply and Clear participate in the main EQ undo history and respect Lock.
  Profile edits are previews until a saved copy is explicitly selected or reapplied; no sweep or
  signal plays from the editor.

## Profile your own equipment

Connect a microphone and use the existing quiet sweep or discrete-tone check. Stop
music and competing noise yourself before starting a measurement. Keep the microphone
at the intended listening position; use conservative test level and the Stop control.
The existing worker checks noise and clipping before admitting a result.

A successful check now offers **Save system response profile**. This opens an editable
profile with measured relative levels and a proposed bounded correction. Save it,
then select it in the equipment library to apply it immediately. The profile
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
settings and no hardware output. Linux is tested locally; shared Windows UI, installer and installed-locale checks
run in GitHub Actions. Physical Windows audio devices, hardware sweep accuracy and
a custom profile on a live microphone require separate verification.

## Managing profiles

In Settings & calibration, choose **Manage profiles** in the speaker, microphone,
or amplifier section. The shared management dialog starts on that equipment type;
you can also browse other types, manufacturers, families, and models.

Use **Create profile** to enter your own equipment and correction filters, or import
a measured JSON/response-text file. The editor prompts you to save modified work
before leaving. Editing a bundled profile saves a custom copy. **Delete** asks for
confirmation and removes only the selected saved user profile, never a bundled
measurement. Deleting a library entry does not change corrections already applied;
use the correction controls to clear or replace those.

A fresh installation starts with Flat listening EQ, microphone EQ off, and no
speaker, amplifier, or imported equipment correction. Saved choices are retained on subsequent starts.
The speaker selector uses one canonical Sony SS-CS5 entry from the published
equipment library; the previous Sony selection is mapped to that entry. Its
measurement source and conditions remain available; it is not a measured SS-CS5M2
response.

## Expanded measured microphone and amplifier collection (2026-11-04)

Both applications share `data/equipment/published.json`: **42 microphone corrections
and 17 amplifier/load-state corrections**, in addition to the existing speaker
collection and Pyle reference. These are correction curves, not product-directory
placeholders. Pattern, capsule/grid, load, supply and measured-state variants count
as separate corrections, not separate physical models. No price or quality ranking
is implied. Models without admissible curves stay out of the selectable library.

The microphone collection spans dynamic, condenser, ribbon, shotgun, USB and lavalier
microphones. It includes Shure, AKG, Neumann, Sony, Sennheiser, Schoeps, Coles,
Electro-Voice, Royer Labs, DPA and RØDE. Lavalier entries include Shure WL93,
legacy DPA 4060-BM with separately identified grids, and RØDE Lavalier II.

The amplifier collection includes measured states for consumer integrated amplifiers
and receivers from NAD, Yamaha, Kenwood, Sansui, Onkyo, Marantz, Grundig, TEAC and
Denon, plus Fosi Audio ZA3 and modified AIYIMA A07 resistive-load variants. These do
not promise correction for an arbitrary reactive speaker load. In particular, the
AIYIMA measurements used LM4562 op-amps and a 27 V supply; they are not stock A07,
A07 MAX or A07 Pro measurements. Some admitted amplifier changes are very small;
an audible improvement is not promised.

### Sources and adaptation

- [University of Surrey dataset, DOI 10.5281/zenodo.4633508](https://zenodo.org/records/4633508),
  version 1.0.0, **CC BY 4.0**, by Juan Carlos Franco Hernández, Tim Brookes and
  Enzo De Sena. The associated paper is Franco Hernández, Bǎcilǎ, Brookes and
  De Sena, [JAES 70(10), 2022](https://doi.org/10.17743/jaes.2022.0027).
  SoundCurrent derives on-axis 1.25 m magnitude ratios against the dataset's
  NTi M2211 reference, smooths by one-third octave and normalizes at 1 kHz.
  It uses the authors' already-windowed IRs; applying another window would change
  the measurement. Coverage is deliberately restricted to 100 Hz–16 kHz.
  Reference calibration uncertainty remains: these are relative specimen responses,
  not individually certified free-field calibration files for a customer's unit.
- [Keiang dScope III amplifier archive](https://www.keiang.de/modules.php?name=Content&pid=69):
  independent numerical readout of gain traces for 1 W/channel into 8 ohms.
  Channels A/B are normalized and averaged in dB; this does not correct channel
  balance. Plot-reading uncertainty is approximately 0.03 dB. Input/tone settings
  are not fully documented, and the source explicitly notes that its analyzer
  lacks current traceable calibration. A vintage measured specimen can differ
  from another specimen of the same model. Original plots and article prose
  are not redistributed.
- [Erin's Audio Corner ZA3 measurements](https://www.erinsaudiocorner.com/electronics/fosi_za3/)
  and [Pavel Macura's A07 measurements](https://pmacura.cz/FCT/aiyima_a07_test.htm):
  independently transcribed resistive-load traces with explicitly named operating
  conditions. Approximate readout uncertainty is 0.1 dB; no simulated-speaker trace
  is passed off as the response for every speaker.
- Published [Shure WL93 manual](https://pubs.shure.com/view/guide/WL93-SM93/en-US.pdf),
  [DPA 4060-BM sheet](https://im.static-thomann.de/pics/atg/atgdata/document/specs/4060_bm.pdf),
  [RØDE Lavalier II sheet](https://edge.rode.com/pdf/page/232/modules/4935/LavalierII_Datasheet_V6%20%281%29.pdf)
  and [RØDE NT-USB Mini sheet](https://edge.rode.com/pdf/products/96/NT-USB_Mini_ds_V06.pdf):
  independent numerical graph readings; the Lavalier II uses its original Bezier
  response path. These are typical responses, not per-unit calibration. Lavalier
  clothing/body effects and optional USB DSP are not inferred from a model curve.
  Only the 100 Hz–16 kHz part of the USB Mini curve is inverted; its intentional
  steep bass rolloff is not inverted. Original PDFs/artwork are not bundled.

Source hashes, graph calibration coordinates, adaptation notes and exclusions are
in `data/equipment/published-collection-report.json`. Numerical inputs are in
`published-response-inputs.json`; these arrays are development inputs, not importable
correction profiles. The Mic Archive index helped locate source documents; its
unlicensed aggregate response library was not copied. Individual serial files for
UMIK, Dayton and similar mics remain user imports; another unit's file is not a
model-wide calibration.

### Admission and reproduction

The compiler uses the same bounded inverse fitter as the profile importer, checks
at least a 15% reduction in sampled squared residual, and limits the **combined**
correction to ±6 dB at 48 kHz. A per-filter cap alone would not bound overlapping
filters. This verifies a fit to supplied data, not listening quality or accuracy
on a different physical unit. No positive filter is centered below 80 Hz. Missing
curves, ambiguous capsules, failed fits and near-flat traces remain explicit gaps.
Graphs show measured response only within the supplied frequency coverage.

Build and reproduce without network access or audio hardware:

```sh
cmake --build build --target soundcurrent-profile-compiler
python3 scripts/prepare-published-equipment.py --check
# Optional source re-extraction (NumPy, SciPy and Pillow; development only):
python3 scripts/verify-equipment-sources.py --surrey --amplifiers --fetch
```

Data attribution stays attached to every profile and its custom copies. Surrey
measurement derivatives retain CC BY 4.0 attribution; SoundCurrent's software and
its own correction-generation implementation use GPL-3.0-only. The measurement
license does not relicense proprietary source artwork. Catalog updates are manual
maintenance operations; the applications do not scrape websites at runtime.

### Acoustic Research AR-2ax custom import

`data/examples/acoustic-research-ar-2ax-experimental.json` is deliberately **not**
bundled as a selectable recommendation. It is a custom import experiment derived
from the **acoustic-power** graph on printed page 18 (PDF page 11) of the
[historical AR brochure](https://retronik.silicium.org/DOCUMENTS/Audiovideo/Acoustic-Research/Acoustic-Research-AR-2AX-AR-3A-AR-4XA-AR-5-AR-6-AR-7-AR-LST-AR-LST-2-Brochure.pdf).
It covers only 500 Hz–20 kHz, with approximately 1 dB readout uncertainty.
It is not an on-axis, room, or current-unit measurement, and does not reconstruct
phase or sum the separate driver curves. Original attenuator settings and exact
revision are not established. Age, restoration and replacement drivers matter.
The factory drawing archive is another reference, but its scan was too poor for
confident response digitization. AR-2a and AR-2x data are not substituted.

Import that JSON through **Manage profiles → Import JSON**, then explicitly select
the saved entry to audition it. Importing/saving alone does not change audio. The
real JSON import is exercised by the `equipment-real-import` UI test in isolated
settings. Nothing is applied to the owner's listening setup by that test.

## Switching and managing saved profiles

Import JSON once, then click Apply profile. Saved custom speaker and amplifier
profiles remain in the main model dropdown after selecting another model.
Applying a speaker profile resets the main Manufacturer and Type filters to All.

In Manage profiles, select Custom to show a list of your saved profiles. Click a
list entry to use it; Edit / save copy, Export JSON and Delete act on that entry.
Delete removes the library entry, with confirmation; an already-applied correction
remains active until you select another profile or None.

Apply profile and Close stay outside the scrolling content. Close keeps the
applied correction. The editor similarly keeps Save/Cancel outside its scroll area.
Hover over a button for help; the descriptions also support keyboard focus,
screen readers and Qt's context help. Existing detailed tips are retained; other
buttons receive context from their section. New profile tips have translations
in every supported catalog, with linguistic accuracy still unverified.

| Button | Purpose |
| --- | --- |
| Import JSON | Add an existing correction profile to your saved library. |
| Import response text | Read measured response data (TXT/CSV/FRD/CAL), then prepare a correction curve. This is not an already-designed EQ file. |
| Create profile | Build a correction profile manually. |
| Edit / save copy | Adjust a copy without overwriting the original. |
| Export JSON | Write the selected profile to a file for sharing or backup. |
| Apply profile | Use the displayed profile for its equipment type. |
| Delete | Remove a saved custom entry; bundled profiles are protected. |

The AR-2ax example uses a historical acoustic-power measurement from about
500 Hz to 20 kHz. Its relatively modest correction does not establish flat bass,
flat on-axis response, or the response of an aged individual pair in a room.

## Graphical correction editor

Drag a point to change its frequency and gain, double-click to add a point, and
use Width for broad or narrow corrections. Advanced numerical controls and
profile metadata can be expanded when needed. Undo reverses an entire drag.
New profiles can be built from a few points; the maximum is 32 actual filters.
The orange measured response is preserved: drawing a correction does not create
or change a measurement.

Enable the relevant equalizer before using Preview. Preview auditions existing
audio temporarily; turning it off or closing the editor restores the previous
correction. Save copy creates a named custom profile; select/apply that saved
profile to keep using it. Cancel discards edits after confirmation.

Profiles with up to 16 filters retain schema 2 compatibility. Profiles with
17–32 filters use schema 3 and require an updated app; older apps reject them.
The processing budget is 96 filters, accommodating 31 listening EQ bands plus
32 speaker and 32 amplifier filters. Microphone correction supports 32 filters.
Linux PipeWire graphs reserve those slots, so the larger limit increases their
fixed processing cost. The C++ processor visits only the active filters.

## Edit an imported profile

Select the imported equipment in the main window, then open **Manage profiles**.
The active imported profile is selected on opening, and **Edit / save copy** opens
its correction graph. Drag points or use the frequency, gain and width controls;
Preview can audition the changes while the corresponding EQ is enabled.
Choose **Save copy**, then **Apply profile** to use the edited correction. Both the
original and the new copy remain available in the saved library. Measurement data
and source attribution are retained. Closing without saving offers Save, Discard
and Cancel. Opening the manager alone never changes audio or saves a profile.
An active imported correction from an older app can also be edited even if it is
not yet present in the saved library.

**Delete** removes imported profiles and saved copies after confirmation. Deleting
the active imported correction also clears that equipment's selection; deleting
an inactive entry leaves the current correction alone. Bundled references cannot
be deleted. Reimporting the same JSON selects the existing saved profile rather
than adding another row. Duplicate matching ignores export IDs and provenance
wrappers, but retains distinctions in equipment identity, curves, measurement
source and conditions. Changed curves with a reused ID receive a separate identity;
existing entries are never overwritten. Deliberately saving an edited copy remains
available independently of import deduplication.
