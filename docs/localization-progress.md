# Localization completion work

## Goal and repository scope

The user replaced the Windows virtual-driver implementation goal with localization fixes on 2026-10-07. Driver code is preserved. This work owns EQ and Studio; the DAW is maintained in its separate chat. Published 1.1.0 assets remain unchanged; no new release is authorized by this continuation.

## Baseline and source review

The independent localization-guard handoff reported 30/318 translated strings in EQ and 30/397 in Studio for each of 32 target locales. It checked catalogs, not completeness of source extraction or compiled/live views.

Review found missing equipment dialog titles/actions, validation reasons, microphone connection text, operational error messages and Studio render messages. These now enter the catalogs. Complete messages replace concatenated prompt/conflict/version fragments. Qt standard action captions share the embedded translation catalog. Raw external backend diagnostics and OS-native dialog text may retain their original language.

The review also fixed two functional bugs: source/conditions length limits depended on translated labels, and equipment-kind filtering used display text as a schema value. Limits are now explicit and selectors retain stable speaker/microphone/amplifier data. Brand/family/model names and user-supplied profile content remain literal. SDK/DSP implementation and driver sources were not modified.

## Current checkpoint

- Populated catalogs: de, fr, es, it, pt-PT, pt-BR, nl, pl; 441 extracted messages each. These are unverified translations; nativeReviewed remains false.
- Other 24 target locales remain incomplete. Exact counts and compiled hashes are authoritative in data/localization/catalogs.json.
- Coverage is distinct from contextual AI review, native-speaker verification and runtime qualification. Checkpoint reports in tests/results/localization identify tested catalogs, platforms and scopes.
- Runtime supports separate UI language and number/date locale selection, regional/script fallback, pseudo localization and RTL. Maintenance preserves unfinished edits/comments and rejects unsupported numerus before rewriting.
- Shared interface layout now wraps meter guidance; the curve instruction also wraps for longer translated text.

## Completion criteria

Populate all current messages for every existing locale, pass structural and compiled/live UI checks on Linux/Windows, validate changed packages, and record evidence/known limitations. Full populated coverage must never be labeled native-speaker verification without an identified reviewer and catalog-specific evidence. The global --require-complete gate will remain failing until all missing entries are filled. No arbitrary English copies should be inserted merely to pass coverage.

Next translation batch: Czech and Slovak. French, German, Spanish and Italian are required to stay populated as sources evolve. The Filter Q caption now describes the dimensionless filter quality parameter; saved processing IDs and numeric values are unchanged.

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
