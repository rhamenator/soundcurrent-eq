# Localization completion work

## Goal and repository scope

The user replaced the Windows virtual-driver implementation goal with localization fixes on 2026-10-07. Driver code is preserved. This work owns EQ and Studio; the DAW is maintained in its separate chat. Published 1.1.0 assets remain unchanged; no new release is authorized by this continuation.

## Baseline and source review

The independent localization-guard handoff reported 30/318 translated strings in EQ and 30/397 in Studio for each of 32 target locales. It checked catalogs, not completeness of source extraction or compiled/live views.

Review found missing equipment dialog titles/actions, validation reasons, microphone connection text, operational error messages and Studio render messages. These now enter the catalogs. Complete messages replace concatenated prompt/conflict/version fragments. Qt standard action captions share the embedded translation catalog. Raw external backend diagnostics and OS-native dialog text may retain their original language.

The review also fixed two functional bugs: source/conditions length limits depended on translated labels, and equipment-kind filtering used display text as a schema value. Limits are now explicit and selectors retain stable speaker/microphone/amplifier data. Brand/family/model names and user-supplied profile content remain literal. SDK/DSP implementation and driver sources were not modified.

## Current checkpoint

- French: all 437 extracted EQ messages and all 523 extracted Studio messages populated, including standard actions, help and errors. Contextual AI translation/review only; nativeReviewed remains false.
- German: all 437 extracted EQ messages and all 523 extracted Studio messages populated. Contextual AI review covers audio terminology, safe calibration/update instructions, and standard actions; nativeReviewed remains false.
- Portuguese (Portugal/Brazil): both catalogs populated; coverage and regional runtime checks are recorded in portuguese-progress.json.
- Other 26 locales: 30 core entries populated each; the remaining messages must be translated and reviewed in subsequent batches.
- Locale selection: region-only Chinese and explicit Latin-script Portuguese aliases resolve to available catalogs; formatting extensions do not block fallback; unsupported explicit scripts remain rejected.
- Maintenance: preserve unfinished translator work/comments; reject unsupported numerus before rewriting; validate placeholders, markup/hyperlinks, glob filters, resource inventory and compiled hashes; prevent regressions in previously populated locales.
- Qualification: run real main-window fixtures for all 32 languages plus pseudo locales; test French and German equipment import/edit/save and field limits; inspect translated tabs at small-screen size. Windows checks and package validation are tracked on the review PRs.

## Completion criteria

Populate all current messages for every existing locale, pass structural and compiled/live UI checks on Linux/Windows, validate changed packages, and record evidence/known limitations. Full populated coverage must never be labeled native-speaker verification without an identified reviewer and catalog-specific evidence. The global --require-complete gate will remain failing until all missing entries are filled. No arbitrary English copies should be inserted merely to pass coverage.

Next translation batch: Dutch. French, German, Spanish and Italian are required to stay populated as sources evolve. The Filter Q caption now describes the dimensionless filter quality parameter; saved processing IDs and numeric values are unchanged.

Windows audio setup now translates application-owned failure, repair, calibration and restart instructions. Restart-required results always retain the translated reboot instruction alongside original helper diagnostics. The source guard also rejects unmarked literal messages passed to the setup completion helper.

Spanish: all 437 extracted messages populated. Contextual AI review covers calibration safety, flat gain, low-volume loudness compensation, dry/wet mixing, compressor timing, routing and filter Q. Native-speaker verification remains unverified. Spanish equipment import/edit/save workflows and inert Windows setup-message fixtures are included in the test gates.

Italian: all 437 extracted messages populated. Full Linux CTest passed 51/51, including Italian equipment workflows. Settings/effects tabs inspected with xcb/Xvfb at 1280×720. Contextual AI review remains unverified; Windows qualification for the new catalogs is pending.

Portuguese partial checkpoint: 216/437 messages populated for each regional variant. Linux compiled-catalog and both Portuguese main-window checks passed (3/3). Regional terminology is reviewed separately; the remainder stays unfinished and falls back to English. Native-speaker verification remains unverified.

Portuguese completed checkpoint: all 437 extracted messages populated in both regional catalogs. Full Linux CTest passed 53/53, including both equipment workflows. Regional settings/effects tabs inspected at 1280×720. Structural completeness is required for future updates; all translations remain unverified. Prior Italian commits passed Windows CI.
