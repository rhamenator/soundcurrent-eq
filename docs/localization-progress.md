# Localization completion work

## Goal and repository scope

The user replaced the Windows virtual-driver implementation goal with localization fixes on 2026-10-07. Driver code is preserved. This work owns EQ and Studio; the DAW is maintained in its separate chat. Published 1.1.0 assets remain unchanged; no new release is authorized by this continuation.

## Baseline and source review

The independent localization-guard handoff reported 30/318 translated strings in EQ and 30/397 in Studio for each of 32 target locales. It checked catalogs, not completeness of source extraction or compiled/live views.

Review found missing equipment dialog titles/actions, validation reasons, microphone connection text, operational error messages and Studio render messages. These now enter the catalogs. Complete messages replace concatenated prompt/conflict/version fragments. Qt standard action captions share the embedded translation catalog. Raw external backend diagnostics and OS-native dialog text may retain their original language.

The review also fixed two functional bugs: source/conditions length limits depended on translated labels, and equipment-kind filtering used display text as a schema value. Limits are now explicit and selectors retain stable speaker/microphone/amplifier data. Brand/family/model names and user-supplied profile content remain literal. SDK/DSP implementation and driver sources were not modified.

## Current checkpoint

- Populated catalogs: de, fr, es, it, pt-PT, pt-BR, nl, pl, cs, sk, uk, ru, el, tr, sv, da, nb, fi, ro, hu, nn, ar, he, fa; 441 extracted messages each. These are unverified translations; nativeReviewed remains false.
- Other 9 original target locales remain incomplete. Exact counts and compiled hashes are authoritative in data/localization/catalogs.json.
- Coverage is distinct from contextual AI review, native-speaker verification and runtime qualification. Checkpoint reports in tests/results/localization identify tested catalogs, platforms and scopes.
- Runtime supports separate UI language and number/date locale selection, regional/script fallback, pseudo localization and RTL. Maintenance preserves unfinished edits/comments and rejects unsupported numerus before rewriting.
- Shared interface layout now wraps meter guidance; the curve instruction also wraps for longer translated text.

## Completion criteria

Populate all current messages for every existing locale, pass structural and compiled/live UI checks on Linux/Windows, validate changed packages, and record evidence/known limitations. Full populated coverage must never be labeled native-speaker verification without an identified reviewer and catalog-specific evidence. The global --require-complete gate will remain failing until all missing entries are filled. No arbitrary English copies should be inserted merely to pass coverage.

Next translation batch: Arabic and Hebrew. French, German, Spanish and Italian are required to stay populated as sources evolve. The Filter Q caption now describes the dimensionless filter quality parameter; saved processing IDs and numeric values are unchanged.

Windows audio setup now translates application-owned failure, repair, calibration and restart instructions. Restart-required results always retain the translated reboot instruction alongside original helper diagnostics. The source guard also rejects unmarked literal messages passed to the setup completion helper.

Spanish: all 437 extracted messages populated. Contextual AI review covers calibration safety, flat gain, low-volume loudness compensation, dry/wet mixing, compressor timing, routing and filter Q. Native-speaker verification remains unverified. Spanish equipment import/edit/save workflows and inert Windows setup-message fixtures are included in the test gates.

Italian: all 437 extracted messages populated. Full Linux CTest passed 51/51, including Italian equipment workflows. Settings/effects tabs inspected with xcb/Xvfb at 1280×720. Contextual AI review remains unverified; Windows qualification for the new catalogs is pending.

Portuguese partial checkpoint: 216/437 messages populated for each regional variant. Linux compiled-catalog and both Portuguese main-window checks passed (3/3). Regional terminology is reviewed separately; the remainder stays unfinished and falls back to English. Native-speaker verification remains unverified.

Portuguese completed checkpoint: all 437 extracted messages populated in both regional catalogs. Full Linux CTest passed 53/53, including both equipment workflows. Regional settings/effects tabs inspected at 1280×720. Structural completeness is required for future updates; all translations remain unverified. Prior Italian commits passed Windows CI.

Dutch: all 437 extracted messages populated. Full Linux CTest passed 54/54 before the meter-help wrapping adjustment. The localized equipment import/edit/save workflow is included. Meter-help text now wraps instead of forcing a wider EQ page. Contextual review remains unverified; current Windows qualification pending.

Layout follow-up: four targeted Dutch/Spanish/pseudo-locale checks passed after wrapping the meter-help label. Inspected Dutch EQ/Studio and Spanish EQ screenshots at 1280×720. The EQ page no longer needs page-wide horizontal scrolling in these samples; the frequency-band strip keeps its own scroll control. Prior Portuguese commits passed Windows CI.

Polish: all 437 extracted messages populated. Full Linux CTest passed 55/55 before one final plot-caption wording correction; focused runtime/UI/equipment tests cover the final catalog separately. Contextual review distinguishes signed gain, filter Q, flat response, low-volume loudness correction and count-independent render summaries. Native-speaker verification remains unverified; current Windows qualification pending.

Polish final catalog: three focused runtime/main-window/equipment checks passed. After curve-instruction wrapping, three Polish/pseudo-locale checks passed. Small-screen inspection still shows Polish EQ horizontal scrolling and partially offscreen headroom; layout qualification remains incomplete. The user also requested optional startup at sign-in for both apps; implement this alongside localization, preserving mutual exclusion and leaving it disabled by default.

Startup option: implemented in Settings for both apps. One shared per-user registration chooses EQ or Studio. Background launch has a visible-window fallback when no tray is available. Linux full tests passed 56/56. Windows build/installer and actual sign-in qualification remain pending. See startup.md.

Czech and Slovak initial batch: cs: 102/441, sk: 102/441. Four focused Linux checks passed in each repository. Includes calibration/profile prompts, gain/headroom and audio setup errors. Both remain incomplete and unverified.

Czech and Slovak second batch: cs: 166/441, sk: 166/441. Four focused Linux checks passed for the compiled catalogs, both main windows and catalog regressions. Error recovery, updates, routing and delay controls are covered; catalogs remain incomplete and unverified.

Czech and Slovak through microphone/measurement batch: cs: 291/441, sk: 291/441. Full Linux CTest passed 56/56. Filters, compressor parameters, equipment editing and calibration limitations were reviewed contextually. Catalogs remain incomplete and unverified. Corrected startup code passed Windows CI; see startup-progress.json for exact tested commits.

Czech and Slovak completed checkpoint: all 441 current extracted messages populated. Full Linux CTest passed 58/58, including both equipment workflows. Czech EQ Settings and Slovak Studio effects were inspected at 1280×720. Render counts use neutral labels instead of fixed plural endings; native review remains unverified. Windows qualification pending for these catalogs.

Ukrainian/Russian initial batches: uk: 166/441, ru: 166/441. Four focused Linux checks passed per repository. Profiles, gain, error recovery, updates and delay controls were reviewed contextually. Both catalogs remain incomplete and unverified.

Ukrainian/Russian through measurement batch: uk: 291/441, ru: 291/441. Four focused Linux checks passed per app. Compressor/filter controls, equipment editing, microphone behavior and model-correction limits reviewed contextually; translations remain incomplete and unverified. Completed Czech/Slovak catalog commits passed Windows CI, with exact source commits recorded in their checkpoint report.

Ukrainian/Russian startup and render batch: uk: 349/441, ru: 349/441. Four focused Linux checks passed in each app. Includes background-startup ownership, hardware setup restart guidance, bounded calibration suggestions, profile copy/reference behavior and neutral render-count labels. Both catalogs remain incomplete and unverified.

Ukrainian/Russian complete extracted catalogs: 441 messages each. Full Linux CTest passed 60/60 including equipment workflows. Ukrainian EQ and Russian Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending on these catalogs.

Greek/Turkish initial batch: el: 102/441, tr: 102/441. Four focused Linux checks passed in each app. Gain, headroom, clipping, profile prompts and setup errors reviewed contextually; both catalogs remain incomplete and unverified.

Greek/Turkish recovery and effects batch: el: 166/441, tr: 166/441. Four focused Linux checks passed per app. Error recovery, update reminders and delay contribution reviewed contextually; catalogs remain incomplete and unverified. Studio completed Ukrainian/Russian catalogs passed Windows CI.

Greek/Turkish filter and editor batch: el: 230/441, tr: 230/441. Four focused Linux checks passed per app. Q, compressor timings/makeup, shelving/pass filters, meter estimates and equipment editing reviewed contextually. Both catalogs remain incomplete and unverified. EQ completed Ukrainian/Russian catalogs now also passed Windows CI.

Greek/Turkish microphone and measurement batch: el: 291/441, tr: 291/441. Four focused Linux checks passed per app. Microphone clipping, whole-system measurement, additive model correction, polarity and non-real-time rendering reviewed contextually. Catalogs remain incomplete and unverified.

Greek/Turkish startup and rendering batch: el: 349/441, tr: 349/441. Four focused Linux checks passed per app. Includes shared sign-in registration, restart guidance, profile reference copies, post gain and count-neutral render summaries. Both catalogs remain incomplete and unverified.

Greek/Turkish complete extracted catalogs: 441 messages each. Full Linux CTest passed 62/62 including equipment workflows. Greek EQ and Turkish Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending for these catalogs.

Swedish/Danish initial batch: sv: 102/441, da: 102/441. Four focused Linux checks passed per app. Gain/headroom, clipping, amplifier measurement requirements and setup errors reviewed contextually. Catalogs remain incomplete and unverified.

Swedish/Danish recovery and effects batch: sv: 166/441, da: 166/441. Four focused Linux checks passed per app. Error recovery, update checks, delay contribution and frequency-sweep prompts reviewed contextually; catalogs remain incomplete and unverified. Studio completed Greek/Turkish catalog commit passed Windows CI.

Swedish/Danish filter and editor batch: sv: 230/441, da: 230/441. Four focused Linux checks passed per app. Q, shelving/pass filters, compressor controls, equipment editing and estimated meters reviewed contextually; catalogs remain incomplete and unverified. EQ completed Greek/Turkish catalog commit now also passed Windows CI.

Swedish/Danish microphone and measurement batch: sv: 291/441, da: 291/441. Four focused Linux checks passed per app. Whole-system limits, microphone clipping, additive correction, polarity and balance reviewed contextually. Both catalogs remain incomplete and unverified.

Swedish/Danish startup and rendering batch: sv: 349/441, da: 349/441. Four focused Linux checks passed per app. Includes shared startup registration, preserved reference profiles, translated calibration links and neutral render counts. Both catalogs remain incomplete and unverified.

Swedish/Danish import and speaker-profile batch: 398/441 messages each. Four focused Linux checks passed per app. Import constraints, saving, disconnected-device recovery and Windows restart guidance reviewed contextually. Catalogs remain incomplete and unverified.

Swedish/Danish complete extracted catalogs: 441 messages each. Full Linux CTest passed 64/64 including equipment workflows. Swedish EQ and Danish Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending for these catalogs.

Norwegian Bokmål/Finnish initial batch: 102/441 messages each. Four focused Linux checks passed per app. Gain, headroom, clipping, amplifier measurements and audio-setup recovery reviewed contextually. Catalogs remain incomplete and unverified.

Norwegian Bokmål/Finnish recovery and effects batch: 166/441 messages each. Four focused Linux checks passed per app. Processing refusal, restart recovery, update checks and delay controls reviewed contextually. Catalogs remain incomplete and unverified.

Norwegian Bokmål/Finnish filter and editor batch: 230/441 messages each. Four focused Linux checks passed per app. Filter Q, compressor parameters, equipment editing and estimated meters reviewed contextually. Catalogs remain incomplete and unverified. Completed Swedish/Danish commits passed Windows CI, with exact runs recorded in their reports.

Norwegian Bokmål/Finnish microphone and measurement batch: 292/441 messages each. Four focused Linux checks passed per app. Additive correction, system measurement limits, clipping, polarity and balance reviewed contextually. Both balance endpoint captions match their instructions. Catalogs remain incomplete and unverified.

Norwegian Bokmål/Finnish startup and rendering batch: 349/441 messages each. Four focused Linux checks passed per app. Shared startup registration, saved-profile reference preservation, calibration links and neutral render counts reviewed contextually. Catalogs remain incomplete and unverified.

Norwegian Bokmål/Finnish import and speaker-profile batch: 398/441 messages each. Four focused Linux checks passed per app. Import constraints, saving, disconnected-device recovery and Windows restart guidance reviewed contextually. Catalogs remain incomplete and unverified.

Norwegian Bokmål/Finnish complete extracted catalogs: 441 messages each. Full Linux CTest passed 66/66 including equipment workflows. Norwegian Bokmål EQ and Finnish Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending for these catalogs.

Romanian/Hungarian initial batch: 102/441 messages each. Four focused Linux checks passed per app. Gain, headroom, clipping, amplifier measurements and audio-setup recovery reviewed contextually. Catalogs remain incomplete and unverified.

Romanian/Hungarian recovery and effects batch: 166/441 messages each. Four focused Linux checks passed per app. Processing refusal, restart recovery, updates and delay controls reviewed contextually. Catalogs remain incomplete and unverified.

Additional user requirement: add a separate Nynorsk (nn) catalog and runtime/workflow checks after completing Romanian and Hungarian. This expands the language scope beyond the original 32 target catalogs.

Romanian/Hungarian filter and editor batch: 230/441 messages each. Four focused Linux checks passed per app. Filter Q, compressor parameters, equipment editing and estimated meters reviewed contextually. Catalogs remain incomplete and unverified. Nynorsk remains next after completing these two catalogs.

Romanian/Hungarian microphone and measurement batch: 292/441 messages each. Four focused Linux checks passed per app. Additive correction, system measurement limits, clipping, polarity and balance reviewed contextually. Both balance endpoint captions match their instructions. Catalogs remain incomplete and unverified; Nynorsk follows these catalogs.

Romanian/Hungarian startup and rendering batch: 349/441 messages each. Four focused Linux checks passed per app. Shared startup registration, saved-profile reference preservation, calibration links and neutral render counts reviewed contextually. Catalogs remain incomplete and unverified; Nynorsk follows completion.

Romanian/Hungarian import and speaker-profile batch: 398/441 messages each. Four focused Linux checks passed per app. Import constraints, saving, disconnected-device recovery and Windows restart guidance reviewed contextually. Catalogs remain incomplete and unverified; Nynorsk follows completion.

Romanian/Hungarian complete extracted catalogs: 441 messages each. Full Linux CTest passed 68/68 including equipment workflows. Romanian EQ and Hungarian Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending for these catalogs.

Nynorsk added as a separate nn catalog, with 102/441 messages populated. Three focused Linux checks passed per app, including distinct Nynorsk/Bokmål regional selection. It remains incomplete and unverified. The target scope is now 33 non-English locales; 20 original catalogs are populated and 13 catalogs remain incomplete.

Nynorsk recovery and effects batch: 166/441 messages populated. Three focused Linux checks passed per app. Processing refusal, restart recovery, update checks and delay controls reviewed contextually. Catalog remains incomplete and unverified.

Nynorsk filter and editor batch: 230/441 messages populated. Three focused Linux checks passed per app. Filter Q, compressor parameters, equipment editing and estimated meters reviewed contextually. Catalog remains incomplete and unverified.

Nynorsk microphone and measurement batch: 292/441 messages populated. Three focused Linux checks passed per app. Additive correction, system measurement limits, clipping, polarity and balance reviewed contextually. Both balance endpoint captions match their instructions. Catalog remains incomplete and unverified.

Nynorsk startup and rendering batch: 349/441 messages populated. Three focused Linux checks passed per app. Startup registration, reference-profile preservation, calibration links and render counts reviewed contextually. Catalog remains incomplete and unverified.

Nynorsk import and speaker-profile batch: 398/441 messages populated. Three focused Linux checks passed per app. Import constraints, saving, disconnected-device recovery and Windows restart guidance reviewed contextually. Catalog remains incomplete and unverified.

Nynorsk complete extracted catalogs: 441 messages each. Full Linux CTest passed 70/70 including equipment workflows. Nynorsk EQ and Nynorsk Studio effects inspected at 1280×720. Contextual review remains unverified, with no native-speaker claim. Windows qualification pending for these catalogs.

Arabic initial batch: 102/441 messages populated. Three focused Linux checks passed per app. Arabic EQ sample inspected at 1280×720 with RTL text/tabs and LTR frequency/numerical controls. Catalog remains incomplete and unverified; full bidi qualification remains open.

Arabic recovery, calibration and delay batch: 166/441 messages populated. Three focused Linux checks passed. Processing refusal, 1 MiB limits, update behavior and processed delay mix reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Arabic equipment, filters and metering batch: 230/441 messages populated. Three focused Linux checks passed. Estimated levels, filter Q/high-pass/high-shelf, equipment kinds and dynamics makeup gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Arabic microphone and measurement batch: 291/441 messages populated. Three focused Linux checks passed. Additive correction, microphone clipping, balance endpoints and signed routing gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Coverage correction: explicit SC_TR markers are now discovered across all C++ source/header/adapter files, including future files outside the view list. Six catalog regression tests passed. Windows direct-literal error gaps are recorded in windows-message-gaps.json; this inventory is deliberately partial. Catalog totals still do not represent every user-facing message. Studio Windows channel-layout refusal now uses its existing translation. Full backend/helper/installer coverage remains required.

Arabic startup, output and rendering batch: 349/441 messages populated. Three focused Linux checks passed. Startup registration, reference-preserving edits, calibration links, post gain and render counts reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Arabic saving, speaker profiles and device recovery batch: 398/441 messages populated. Three focused Linux checks passed. Saving, speaker correction bounds, peak hold, device recovery and reverb versus echo reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Arabic current catalogs complete: 441/441 messages populated. Full Linux CTest passed 71/71, including Arabic equipment workflows and RTL assertions. Calibration limits and operational controls reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew initial audio controls and profile guidance: 102/441 current catalog entries populated. Three focused Linux checks passed. Gain/headroom, clipping and measured amplifier correction requirements reviewed contextually. Catalog remains incomplete and unverified; full bidi qualification remains open.

Hebrew calibration, recovery and delay batch: 166/441 messages populated. Three focused Linux checks passed. Calibration recovery, file limits, cancel versus close and processed delay mix reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew equipment, filters and metering batch: 230/441 messages populated. Three focused Linux checks passed. Estimated levels, filter Q/high-pass/high-shelf and equipment kinds reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew microphone and measurement batch: 291/441 messages populated. Three focused Linux checks passed. Additive correction, microphone clipping, balance endpoints and signed routing gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew startup, output and rendering batch: 349/441 messages populated. Three focused Linux checks passed. Startup registration, reference-preserving edits, calibration links, post gain and render counts reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew saving, speaker profiles and device recovery batch: 398/441 messages populated. Three focused Linux checks passed. Saving, speaker correction bounds, peak hold, device recovery and reverb versus echo reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Hebrew current catalogs complete: 441/441 messages populated. Full Linux CTest passed 72/72, including Hebrew equipment workflows and RTL assertions. Calibration limits and operational controls reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian initial audio controls and profile guidance: 102/441 messages populated. Three focused Linux checks passed. Gain/headroom, clipping and measured amplifier correction requirements reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian calibration, recovery and delay batch: 166/441 messages populated. Three focused Linux checks passed. Calibration recovery, file limits, cancel versus close and processed delay mix reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian equipment, filters and metering batch: 230/441 messages populated. Three focused Linux checks passed. Estimated levels, filter Q/high-pass/high-shelf and equipment kinds reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian microphone and measurement batch: 291/441 messages populated. Three focused Linux checks passed. Additive correction, microphone clipping, balance endpoints and signed routing gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian startup, output and rendering batch: 349/441 messages populated. Three focused Linux checks passed. Startup registration, reference-preserving edits, calibration links, post gain and render counts reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian saving, speaker profiles and device recovery batch: 398/441 messages populated. Three focused Linux checks passed. Saving, speaker correction bounds, peak hold, device recovery and reverb versus echo reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Persian current catalogs complete: 441/441 messages populated. Full Linux CTest passed 73/73, including Persian equipment workflows and RTL assertions. Calibration limits and operational controls reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese initial audio controls and profile guidance: 102/441 messages populated. Three focused Linux checks passed. Gain/headroom, clipping and measured amplifier correction requirements reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.
