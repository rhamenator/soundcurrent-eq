# Translation review — 2026-10-07

## Scope and evidence

A contextual AI review examined the 30 starter strings in each of 32 unverified languages (960 translated entries). This is an internal review, not an independent review or native-speaker certification. No external translator service was used. Remaining untranslated strings were not certified by this review.

Corrections address clear semantic risks and ambiguous audio terminology: Swahili gain previously used a word for profit and bands a word for belts; frequency-band labels were made explicit in several languages; flat-response labels/reset actions were clarified; Hungarian regional settings and Vietnamese audio/preset labels were improved. These replacement translations remain unverified themselves.

`data/localization/translation-context.json` supplies technical definitions in generated Qt Linguist translator notes. Gain means a signed signal-level adjustment in dB; flat means zero EQ gain, not mute; balance means left/right channel level; Quit exits the process while closing the window leaves it running. Notes are shared between the applications and can be reused by the DAW.

## Remaining uncertainty

French, German and Spanish are populated; the other 29 language packs remain incomplete. All 32 lack native review. Particular review priorities are Swahili audio vocabulary, Thai preset wording, natural microphone EQ terminology in every language, and regional Portuguese and Chinese usage. An AI reread or back-translation by the same model is not independent evidence and must not be labeled native review. Do not promote catalog review status based on this pass.

## Required review workflow

1. Review the English source and actual control behavior with the translator notes. Resolve ambiguous source wording before translating it.
2. A fluent reviewer should compare each target string directly with that intent and flag wrong meaning, unintended connotations, register, regional usage, spelling and unnatural phrasing. Back-translation can help identify questions, but does not replace this comparison.
3. Check strings in the running interface, including RTL, truncation, mnemonics, decimal entry, and complete error messages. Verify Quit, reset and lock semantics explicitly.
4. Record reviewer identity/locale, date, catalog commit, corrections and unresolved issues before setting nativeReviewed. Any later edited strings require renewed review.
5. Keep uncertain entries unfinished so they fall back to English. Keep partial-language coverage visible. Full language qualification also requires the release gates in localization.md.

## Automated checks

Catalog audit checks source inventory, duplicate keys, nonempty finished entries, exact placeholders, literal `&&` preservation, absence of hidden bidi override/isolate controls, coverage and compiled-catalog hashes. The interface supplies RTL direction explicitly. These checks verify structural integrity, not translation accuracy or cultural appropriateness.

## Localization completion checkpoint

The French catalog is now populated for all currently extracted app messages (437 EQ / 523 Studio). Translation and contextual review were performed by this coding agent; no external provider or fluent-speaker certification was used. Coverage includes help, equipment validation, calibration preview, standard Qt actions and Studio render/status text. Every French entry remains unverified. German and Spanish are also populated; the other 29 languages retain their starter coverage. The independent localization-guard audit agrees with populated coverage; its linguistic/runtime qualification fields remain unknown because that tool cannot assess them. Compiled/runtime evidence is recorded separately in tests/results/localization/checkpoint.json.

## German completion checkpoint

All current EQ/Studio catalog entries are populated in German (437 / 523). This coding agent reviewed the supplied text against actual control meaning: Verstärkung is audio gain; Pegelreserve is headroom; Ohne Effekt is dry signal; Pegelkorrektur is channel trim, not file cutting; Loudness-Korrektur is the listening preset, not a loudness measurement; Rückkopplung is delay feedback. Profile type IDs, user/vendor names, rich-text links, extensions, placeholders and saved processing values remain invariant. Filter Q replaces the imprecise Width (Q) source caption. This is contextual AI review and remains unverified, not native-speaker certification.

## Spanish completion checkpoint

All current catalog entries are populated in Spanish (437 EQ / 523 Studio). Contextual AI review distinguishes Ganancia (audio gain), Factor Q del filtro (dimensionless filter quality), Plano (zero EQ gain), Compensación de sonoridad (low-volume listening correction), Sin efecto (dry signal), and Tiempo de relajación del compresor (compressor release). Manufacturer names, profiles, placeholders, URLs and file filters remain invariant. These translations remain unverified; this does not constitute native-speaker review.
