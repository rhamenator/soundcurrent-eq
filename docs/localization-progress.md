# Localization completion work

## Goal and repository scope

The user replaced the Windows virtual-driver implementation goal with localization fixes on 2026-10-07. Driver code is preserved. This work owns EQ and Studio; the DAW is maintained in its separate chat. Published 1.1.0 assets remain unchanged; no new release is authorized by this continuation.

## Baseline and source review

The independent localization-guard handoff reported 30/318 translated strings in EQ and 30/397 in Studio for each of 32 target locales. It checked catalogs, not completeness of source extraction or compiled/live views.

Review found missing equipment dialog titles/actions, validation reasons, microphone connection text, operational error messages and Studio render messages. These now enter the catalogs. Complete messages replace concatenated prompt/conflict/version fragments. Qt standard action captions share the embedded translation catalog. Raw external backend diagnostics and OS-native dialog text may retain their original language.

The review also fixed two functional bugs: source/conditions length limits depended on translated labels, and equipment-kind filtering used display text as a schema value. Limits are now explicit and selectors retain stable speaker/microphone/amplifier data. Brand/family/model names and user-supplied profile content remain literal. SDK/DSP implementation and driver sources were not modified.

## Current checkpoint

- Populated catalogs: de, fr, es, it, pt-PT, pt-BR, nl, pl, cs, sk, uk, ru, el, tr, sv, da, nb, fi, ro, hu, nn, ar, he, fa, zh-Hans, zh-Hant, ja, ko, hi, id, vi, th, sw; 459 extracted messages each. These are unverified translations; nativeReviewed remains false.
- All original target locales and added Nynorsk have populated current catalogs; omitted application messages still require the second pass. Exact counts and compiled hashes are authoritative in data/localization/catalogs.json.
- Coverage is distinct from contextual AI review, native-speaker verification and runtime qualification. Checkpoint reports in tests/results/localization identify tested catalogs, platforms and scopes.
- Runtime supports separate UI language and number/date locale selection, regional/script fallback, pseudo localization and RTL. Maintenance preserves unfinished edits/comments and rejects unsupported numerus before rewriting.
- Shared interface layout now wraps meter guidance; the curve instruction also wraps for longer translated text.

## Completion criteria

Populate all current messages for every existing locale, pass structural and compiled/live UI checks on Linux/Windows, validate changed packages, and record evidence/known limitations. Full populated coverage must never be labeled native-speaker verification without an identified reviewer and catalog-specific evidence. The global --require-complete gate passes for the current catalogs; this does not prove that every application-owned message has been extracted. No arbitrary English copies should be inserted merely to pass coverage.

Next work: second-pass inventory and localization of omitted application-owned messages. French, German, Spanish and Italian are required to stay populated as sources evolve. The Filter Q caption now describes the dimensionless filter quality parameter; saved processing IDs and numeric values are unchanged.

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

Simplified Chinese calibration, recovery and delay batch: 166/441 messages populated. Three focused Linux checks passed. Calibration recovery, file limits, cancel versus close and delay wet mix reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese equipment, filters and metering batch: 230/441 messages populated. Three focused Linux checks passed. Estimated levels, filter Q/high-pass/high-shelf and equipment kinds reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese microphone and measurement batch: 291/441 messages populated. Three focused Linux checks passed. Additive correction, microphone clipping, balance endpoints and signed routing gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese startup, output and rendering batch: 349/441 messages populated. Three focused Linux checks passed. Startup registration, reference-preserving edits, calibration links, post gain and render counts reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese saving, speaker profiles and device recovery batch: 398/441 messages populated. Three focused Linux checks passed. Saving, speaker correction bounds, peak hold, device recovery and reverb versus echo reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Simplified Chinese current catalogs complete: 441/441 messages populated. Full Linux CTest passed 74/74, including equipment workflows and script selection assertions. Calibration limits and operational controls reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese initial audio controls and profile guidance: 102/441 messages populated. Three focused Linux checks passed. Gain/headroom, clipping and measured amplifier correction requirements reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese calibration, recovery and delay batch: 166/441 messages populated. Three focused Linux checks passed. Calibration recovery, file limits, cancel versus close and delay wet mix reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese equipment, filters and metering batch: 230/441 messages populated. Three focused Linux checks passed. Estimated levels, filter Q/high-pass/high-shelf and equipment kinds reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese microphone and measurement batch: 291/441 messages populated. Three focused Linux checks passed. Additive correction, microphone clipping, balance endpoints and signed routing gain reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese startup, output and rendering batch: 349/441 messages populated. Three focused Linux checks passed. Startup registration, reference-preserving edits, calibration links, post gain and render counts reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese saving, speaker profiles and device recovery batch: 398/441 messages populated. Three focused Linux checks passed. Saving, speaker correction bounds, peak hold, device recovery and reverb versus echo reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

Traditional Chinese current catalogs complete: 441/441 messages populated. Full Linux CTest passed 75/75, including equipment workflows and script selection assertions. Calibration limits and operational controls reviewed contextually. Catalog remains incomplete and unverified; mixed-direction formatting remains open.

### Japanese: first existing-catalog batch

Japanese now has 102/441 populated current catalog entries. The first 80 shared sources cover device routing, calibration limitations, enhancement controls and update/quit guidance. Both repositories passed the three focused Linux checks (localization, Japanese MainWindow offscreen, catalog structure). These translations remain unverified; Windows and visual qualification remain pending. Seven languages still have incomplete existing catalogs. Missing source strings remain scheduled for the second pass.

### Japanese: second existing-catalog batch

Japanese now has 166/441 populated current catalog entries. Added equipment-profile errors, calibration failure messages, update guidance, preset actions and delay controls. Both apps passed the three focused Linux checks. Native-speaker, Windows and visual qualification remain pending; translations are unverified. Missing source strings remain scheduled for the second pass.

### Japanese: third existing-catalog batch

Japanese now has 230/441 populated current catalog entries. Added dynamics controls, estimated metering, profile editor guidance and filter validation. Both apps passed the three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Japanese: fourth existing-catalog batch

Japanese now has 291/441 populated current catalog entries. Added import/export guidance, live-channel limits, microphone calibration and balance behavior. Both apps passed the three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Japanese: fifth existing-catalog batch

Japanese now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile source links, sweep cautions and rendering messages. Both apps passed the three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Japanese: sixth existing-catalog batch

Japanese now has 398/441 populated current catalog entries. Added save/reset actions, room measurement, microphone routing and peak-marker guidance. Both apps passed the three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Japanese: current catalogs populated

All current Japanese catalog entries are populated (441 EQ, 527 Studio). Contextual AI review remains unverified; native-speaker and Windows qualification are pending. Six target languages still have incomplete existing catalogs. Omitted source strings remain scheduled for the second pass. The Japanese checkpoint report records Linux test evidence.

### Japanese: completeness and profile-editor gates

Japanese is now required to stay fully populated by the source/catalog validator. Runtime assertions verify its regional fallback, LTR layout and standard Save action. The actual profile editor now runs its import/edit/save/cancel/discard/apply workflow in Japanese; all four focused Linux checks passed in both apps. The preceding full-suite results remain 75/75 EQ and 77/77 Studio before this additional workflow test. Translations remain unverified.

### Korean: first existing-catalog batch

Korean now has 102/441 populated current catalog entries. The first 80 shared sources cover routing, measurement limitations and primary audio controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: second existing-catalog batch

Korean now has 166/441 populated current catalog entries. Added profile-saving errors, calibration failures, update guidance and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: third existing-catalog batch

Korean now has 230/441 populated current catalog entries. Added effects, estimated metering and equipment-profile editing. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: fourth existing-catalog batch

Korean now has 291/441 populated current catalog entries. Added import/export guidance, microphone calibration and balance behavior. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: fifth existing-catalog batch

Korean now has 349/441 populated current catalog entries. Added output controls, source links, startup and rendering guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: sixth existing-catalog batch

Korean now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Korean: current catalogs populated

All current Korean catalog entries are populated (441 EQ, 527 Studio). Both apps passed three focused Linux checks, including the actual MainWindow offscreen. Translations remain unverified; native-speaker, visual and Windows qualification are pending. Five target languages remain incomplete. Omitted source strings remain scheduled for the second pass.

### Korean: completeness and profile-editor gates

Korean is required to stay fully populated by the source/catalog validator. Runtime assertions verify regional fallback, LTR layout and the standard Save action. The profile editor import/edit/save/cancel/discard/apply workflow runs in Korean. All four focused Linux checks passed in both apps. Translations remain unverified; Windows and visual qualification are pending.

### Hindi: first existing-catalog batch

Hindi now has 102/441 populated current catalog entries. The first 80 shared sources cover device routing, calibration limitations and main audio controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: second existing-catalog batch

Hindi now has 166/441 populated current catalog entries. Added saving errors, calibration failures and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: third existing-catalog batch

Hindi now has 230/441 populated current catalog entries. Added effects, estimated metering and equipment-profile editing. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: fourth existing-catalog batch

Hindi now has 291/441 populated current catalog entries. Added import validation, microphone calibration, channel limits and balance guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: fifth existing-catalog batch

Hindi now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile-source links and rendering messages. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: sixth existing-catalog batch

Hindi now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Hindi: current catalogs populated

All current Hindi catalog entries are populated (441 EQ, 527 Studio). Both apps passed three focused Linux checks, including the actual MainWindow offscreen. Translations remain unverified; native-speaker, visual and Windows qualification are pending. Four target languages remain incomplete. Omitted source strings remain scheduled for the second pass.

### Hindi: completeness and profile-editor gates

Hindi is required to stay fully populated by the source/catalog validator. Runtime assertions verify regional fallback, LTR layout and the standard Save action. The profile editor import/edit/save/cancel/discard/apply workflow runs in Hindi. All four focused Linux checks passed in both apps. Translations remain unverified; Windows and visual qualification are pending.

### Indonesian: first existing-catalog batch

Indonesian now has 102/441 populated current catalog entries. The first 80 shared sources cover routing, calibration limitations and main audio controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: second existing-catalog batch

Indonesian now has 166/441 populated current catalog entries. Added profile-saving errors, calibration failures and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: third existing-catalog batch

Indonesian now has 230/441 populated current catalog entries. Added effects, metering and profile-editor controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: fourth existing-catalog batch

Indonesian now has 291/441 populated current catalog entries. Added import validation, microphone calibration, channel limits and balance guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: fifth existing-catalog batch

Indonesian now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile-source links and rendering messages. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: sixth existing-catalog batch

Indonesian now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Indonesian: current catalogs populated

All current Indonesian catalog entries are populated (441 EQ, 527 Studio). Both apps passed three focused Linux checks, including the actual MainWindow offscreen. Translations remain unverified; native-speaker, visual and Windows qualification are pending. Three target languages remain incomplete. Omitted source strings remain scheduled for the second pass.

### Indonesian: completeness and profile-editor gates

Indonesian is required to stay fully populated by the source/catalog validator. Runtime assertions verify regional fallback, LTR layout and the standard Save action. The profile editor import/edit/save/cancel/discard/apply workflow runs in Indonesian. All four focused Linux checks passed in both apps. Translations remain unverified; Windows and visual qualification are pending.

### Vietnamese: first existing-catalog batch

Vietnamese now has 102/441 populated current catalog entries. The first 80 shared sources cover routing, calibration limitations and main audio controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese: second existing-catalog batch

Vietnamese now has 166/441 populated current catalog entries. Added profile-saving errors, calibration failures and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese: third existing-catalog batch

Vietnamese now has 230/441 populated current catalog entries. Added effects, metering and profile-editor controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese: fourth existing-catalog batch

Vietnamese now has 291/441 populated current catalog entries. Added import validation, microphone calibration, channel limits and balance guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese: fifth existing-catalog batch

Vietnamese now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile-source links and rendering messages. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese: sixth existing-catalog batch

Vietnamese now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Vietnamese existing catalog complete

All 441/441 current Vietnamese catalog entries are populated. Required-complete and regional fallback gates now include Vietnamese. Both apps passed four focused Linux checks, including the profile import/edit/save/cancel/discard/apply workflow. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: first existing-catalog batch

Thai now has 102/441 populated current catalog entries. Added main controls, routing, amplifier-measurement limitations and enhancement guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: second existing-catalog batch

Thai now has 166/441 populated current catalog entries. Added profile errors, calibration messages and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: third existing-catalog batch

Thai now has 230/441 populated current catalog entries. Added effects, level indicators and equipment-profile controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: fourth existing-catalog batch

Thai now has 291/441 populated current catalog entries. Added import validation, microphone calibration, channel limits and balance guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: fifth existing-catalog batch

Thai now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile-source links and rendering messages. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai: sixth existing-catalog batch

Thai now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Thai existing catalog complete

All 441/441 current Thai catalog entries are populated. Required-complete and regional fallback gates now include Thai. Both apps passed four focused Linux checks, including the profile import/edit/save/cancel/discard/apply workflow. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: first existing-catalog batch

Swahili now has 102/441 populated current catalog entries. Added main controls, routing, amplifier-measurement limitations and enhancement guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: second existing-catalog batch

Swahili now has 166/441 populated current catalog entries. Added profile errors, calibration messages and delay controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: third existing-catalog batch

Swahili now has 230/441 populated current catalog entries. Added effects, level indicators and equipment-profile controls. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: fourth existing-catalog batch

Swahili now has 291/441 populated current catalog entries. Added import validation, microphone calibration, channel limits and balance guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: fifth existing-catalog batch

Swahili now has 349/441 populated current catalog entries. Added output controls, startup guidance, profile-source links and rendering messages. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili: sixth existing-catalog batch

Swahili now has 398/441 populated current catalog entries. Added save/reset actions, room measurement and microphone-routing guidance. Both apps passed three focused Linux checks. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Swahili existing catalog complete

All 441/441 current Swahili catalog entries are populated. Required-complete and regional fallback gates now include Swahili. Full Linux CTest passed 82/82, including profile workflows and all locale MainWindow offscreen checks. The global catalog completeness gate passes. Translations remain unverified; Windows and visual qualification are pending. Omitted source strings remain scheduled for the second pass.

### Second-pass source inventory

Added `scripts/localization_inventory.py` and its generated `tests/results/localization/source-inventory.json`. This read-only discovery aid scans literal C++ exceptions and selected view calls across all source files, NSIS candidates and Python diagnostics. Counts are candidates, not confirmed untranslated user messages. Dynamic/composed strings, shell/PowerShell helpers, native dialogs and data-driven names still require review. Current catalog completeness checks continue to pass.

Confirmed first adapter targets: missing Windows audio-route instructions, disconnected speakers, invalid equalizer/microphone settings; Studio also has live-layout and invalid Studio-setting messages. Translate at the Qt adapter/view boundary while preserving backend identifiers and numeric processing. Installer strings require their own language selection and catalog workflow. The existing full Linux 82/82 (EQ) and 84/84 (Studio) results predate this informational tooling change; no new runtime qualification is claimed.

### Second-pass Windows adapter errors in progress

Qt adapter errors now use translation markers; a regression guard rejects direct untranslated exception literals. Seven new EQ / nine new Studio source messages entered the catalogs, including bridge-start fallback errors. German and French translations have been supplied; other locales remain unfinished and fall back to English. The required-complete gate is intentionally failing until the new translations are filled. Seven catalog-maintenance regressions passed in each repository. No Windows runtime qualification or new full-suite pass is claimed.

Second-pass adapter translation checkpoint: German, French, Spanish, Italian, European/Brazilian Portuguese, Dutch and Polish now populate all newly exposed adapter messages. Structural checks passed for these eight locales; seven catalog-maintenance regression tests passed in each repository. Other locales still need the seven EQ / nine Studio new messages, so the global completeness gate remains failing. Work remains local until that batch is complete; translations remain unverified.

Second-pass adapter translation checkpoint: added Czech, Slovak, Ukrainian, Russian, Greek and Turkish. Fourteen locales now populate all newly exposed adapter messages and pass structural checks. Seven catalog-maintenance regressions passed in each repository. The other nineteen non-English locales still need these messages, so the global completeness gate remains failing. Changes remain local; runtime qualification for this batch is pending and translations are unverified.

Second-pass adapter translation checkpoint: added Swedish, Danish, Norwegian Bokmål/Nynorsk, Finnish, Romanian and Hungarian. Twenty-one locales now populate all newly exposed adapter messages and pass structural checks. Seven catalog-maintenance regressions passed in each repository. Twelve non-English locales still need these messages; the global completeness gate remains failing. Work remains local, translations unverified and runtime qualification pending.

Second-pass adapter translation checkpoint: added Arabic, Hebrew, Persian, Simplified/Traditional Chinese, Japanese and Korean. Twenty-eight locales now populate all newly exposed adapter messages and pass structural checks. Seven catalog-maintenance regressions passed in each repository. Hindi, Indonesian, Vietnamese, Thai and Swahili still need these messages; the global completeness gate remains failing. Work remains local, translations unverified and runtime qualification pending.

### Second-pass Qt Windows adapter messages complete

All 33 non-English locales now cover the new seven EQ / nine Studio messages. Global required-complete and structural checks pass. Both apps rebuilt and passed two focused Linux checks; compiled message loading and Unicode exception round-trip run across non-English locales. The adapter rejects unmarked literal exception messages in source regression checks. Backend diagnostics and installers remain separate second-pass work. Translations are unverified; Windows/device/installer qualification is pending. Earlier in-progress checkpoints describe historical states.

### Backend diagnostic translation boundary

Added a shared Qt desktop-only audio-error mapper. The invariant Windows backend diagnostic “The selected EQ settings are invalid” now displays the existing translated equalizer-settings message. Unknown driver diagnostics retain their exact text and Unicode details. The backend remains Qt-independent and its diagnostics/processing identifiers are unchanged. Both builds passed catalog completeness and two focused Linux checks, including mapping across non-English locales. Other backend diagnostics still require cataloging; Windows runtime and package qualification remain pending.

### Backend literal diagnostics in progress

Extended the Qt desktop boundary to direct literal Windows audio/capture errors, including unsupported formats, buffer overrun and stalled microphone consumption. Each app adds eleven applicable catalog messages; backend diagnostic IDs and DSP code remain unchanged. Microphone and calibration display paths now use the same mapping. German/French translations pass structural checks; seven catalog-maintenance regressions pass. Remaining locales, HRESULT/composed diagnostics and runtime/package qualification remain incomplete. The global completeness gate is failing for the new untranslated entries. Changes remain local and translations unverified.

Backend literal translation checkpoint: added Spanish, Italian, European/Brazilian Portuguese, Dutch and Polish. Eight locales now populate all applicable new backend literal messages; structural checks and seven catalog-maintenance regressions passed. Twenty-five non-English locales remain. Changes remain local; no runtime qualification is claimed and translations remain unverified.

Backend literal translation checkpoint: added Czech, Slovak, Ukrainian, Russian, Greek and Turkish. Fourteen locales now populate the applicable new backend messages. Structural checks and seven catalog-maintenance regressions passed in each repository. Nineteen locales remain. Changes remain local, translations unverified and runtime qualification pending.

Backend literal translation checkpoint: added Swedish, Danish, Norwegian Bokmål/Nynorsk, Finnish, Romanian and Hungarian. Twenty-one locales populate all applicable new backend messages. Structural checks and seven catalog-maintenance regressions passed in each repository. Twelve locales remain; changes are local and translations unverified. Runtime/package qualification remains pending.

Backend literal translation checkpoint: added Arabic, Hebrew, Persian, both Chinese variants, Japanese and Korean. Twenty-eight locales populate the applicable new messages. Structural checks and seven catalog-maintenance regressions passed in each repository. Hindi, Indonesian, Vietnamese, Thai and Swahili remain. Changes are local, translations unverified and runtime/package qualification pending.

### Backend literal diagnostic batch complete

All 33 non-English locales now cover the eleven applicable direct-literal backend messages per app. Global required-complete and structural checks pass. Both apps rebuilt and passed two focused Linux checks, including compiled translations, known diagnostic mapping and preservation of unknown Unicode details. Backend/DSP sources remain unchanged. HRESULT/composed diagnostics, helper and installer messages, visual and Windows/package qualification remain outstanding. Translations remain unverified. Earlier in-progress checkpoints describe historical states.

### Composed Windows operation diagnostics in progress

The desktop mapper recognizes known backend check/checked operation captions and the exact HRESULT failure shape, translating the operation and message template while retaining the code. A compiled Qt fixture passed for both headers with a translated action/template, mixed-case code preservation and unknown/malformed fallback. Added the fixture as a CTest target. New operation captions (61 EQ / 63 Studio) and one failure template require translation; completeness is currently failing. Backend processing sources remain unchanged. Work remains local; no Windows runtime qualification is claimed.

HRESULT operation translation checkpoint: German and French now cover all applicable operation captions and the failure template. Current catalogs passed structural validation. The saved backend-error-text CTest fixture passed in both Linux builds with a synthetic translator; it checks exact error-code preservation and unknown/malformed fallback. Thirty-one locales remain; changes are local and translations unverified.

HRESULT operation translation checkpoint: added Spanish and Italian. Four current locale catalogs are structurally valid and complete; failure templates preserve placeholders and error codes. Spanish uses a neutral failure label that also fits noun-style diagnostics. Seven maintenance regressions and the synthetic-translator backend-error-text fixture passed in each repository. Twenty-nine locales remain; changes are local and translations unverified.

HRESULT operation checkpoint: European/Brazilian Portuguese and Dutch added. Seven current catalogs are structurally valid and complete. Portuguese terminology distinguishes colunas/alto-falantes, controlo/controle and mistura/mixagem. Seven maintenance regressions and the synthetic error-code fixture passed before the Dutch addition; Dutch additionally passed structural validation. Twenty-six locales remain. Changes are local and translations unverified.

HRESULT operation checkpoint: Polish and Czech added. Nine current catalogs are structurally valid and complete, with the neutral failure template preserving placeholder/error-code details. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Twenty-four locales remain. Changes are local and translations unverified.

HRESULT operation checkpoint: Slovak, Ukrainian and Russian added. Twelve current catalogs are structurally valid and complete. Russian distinguishes buffer fill from signal level and captured samples from packets. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Twenty-one locales remain. Changes are local and translations unverified; compiled catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Greek added. Thirteen current catalogs are structurally valid and complete. The playback-padding caption describes buffered frames, while the output-buffer caption describes buffer fill rather than signal amplitude. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Twenty locales remain; translations are unverified and actual compiled-catalog UI/Windows qualification remains pending.

HRESULT operation checkpoint: Turkish added. Fourteen current catalogs are structurally valid and complete. Initialization captions describe preparation, distinct from starting streams; buffer-fill captions remain distinct from audio levels. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Nineteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Swedish added. Fifteen current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Eighteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Danish added. Sixteen current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Seventeen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Norwegian Bokmål added. Seventeen current catalogs are structurally valid and complete. Mute captions explicitly describe disabled sound rather than attenuation; buffer fill remains distinct from audio level. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Sixteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Finnish added. Eighteen current catalogs are structurally valid and complete. Initialization, stream startup, buffer fill, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Fifteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Romanian added. Nineteen current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, samples and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Fourteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Hungarian added. Twenty current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, samples and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Thirteen locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Nynorsk added after Romanian and Hungarian. Twenty-one current catalogs are structurally valid and complete. Nynorsk captions use høgtalar, lydstraum, storleik and avspeling; mute status explicitly means disabled sound. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Twelve locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Arabic added. Twenty-two current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, samples and signal level use distinct captions; placeholders and hexadecimal prefix remain unchanged. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Eleven locales remain. Changes are local and translations unverified; mixed-direction rendering, actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Hebrew added. Twenty-three current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, samples and signal level use distinct captions; placeholders and hexadecimal prefix remain unchanged. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Ten locales remain. Changes are local and translations unverified; mixed-direction rendering, actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Persian added. Twenty-four current catalogs are structurally valid and complete. Preparation and startup, buffer fill and signal level use distinct captions; placeholders and hexadecimal prefix remain unchanged. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Nine locales remain. Changes are local and translations unverified; mixed-direction rendering, actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Simplified Chinese added. Twenty-five current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Eight locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Traditional Chinese added. Twenty-six current catalogs are structurally valid and complete. Regional terminology includes 音訊, 串流, 封包, 喇叭, 位準 and 等化器; buffer fill remains distinct from signal level. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Seven locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Japanese added. Twenty-seven current catalogs are structurally valid and complete. Buffer usage, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Six locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Korean added. Twenty-eight current catalogs are structurally valid and complete. Buffer usage, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Five locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Hindi added. Twenty-nine current catalogs are structurally valid and complete. Initialization captions describe preparation, distinct from stream startup; buffer fill remains distinct from signal level. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Four locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Indonesian added. Thirty current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Three locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Vietnamese added. Thirty-one current catalogs are structurally valid and complete. Buffer fill, buffered audio frames, sample values and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Two locales remain. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation checkpoint: Thai added. Thirty-two current catalogs are structurally valid and complete. Preparation and startup, buffer usage and signal level use distinct captions. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Swahili remains. Changes are local and translations unverified; actual catalog UI and Windows qualification remain pending.

HRESULT operation catalog batch complete: Swahili added. All 33 non-English locales now cover the known operation captions and failure template. Global required-complete/structural/compiled-hash checks pass: 521 extracted source messages across 34 catalogs. Seven maintenance regressions and the synthetic error-code fixture passed in each repository. Translations remain unverified; rebuilt application tests, actual catalog UI and Windows/package qualification remain pending. Omitted interface messages still require the second-pass audit.

HRESULT batch Linux qualification: rebuilt application and localization test; full CTest suite 83/83 passed. Added actual embedded-catalog tests for four operation captions and composed failures in every non-English locale, preserving mixed-case hexadecimal codes. This does not prove native-speaker accuracy, visual rendering or Windows installation behavior.

Second-pass reachability audit: two additional Windows messages are outside current extracted catalogs: the conditional microphone-start timeout and composed cable-format rejection. Their fixed producer strings and call sites were inspected; desktop mappings/catalog entries are still needed. Backend processing sources remain unchanged. See backend-composed-gaps.json; this is not a complete interface audit.

Second-pass composed diagnostic fix: mapped microphone startup timeout and cable format rejection at the desktop boundary and translated both across all 33 non-English catalogs. Global required-complete and structural checks plus seven maintenance regressions pass. Added these to actual compiled-catalog mapping tests; rebuild/runtime execution remains pending. Backend sources unchanged; translations unverified.

Second-pass composed diagnostic qualification: rebuilt localization-test and passed four focused Linux CTests in each repository. Actual embedded catalogs mapped the two added diagnostics in every non-English locale without English fallback. Visual/UI and Windows/package qualification remain pending; nativeReviewed remains false.
