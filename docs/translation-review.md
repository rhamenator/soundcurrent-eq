# Translation review — 2026-10-07

## Scope and evidence

A contextual AI review examined the 30 starter strings in each of 32 unverified languages (960 translated entries). This is an internal review, not an independent review or native-speaker certification. No external translator service was used. Remaining untranslated strings were not certified by this review.

Corrections address clear semantic risks and ambiguous audio terminology: Swahili gain previously used a word for profit and bands a word for belts; frequency-band labels were made explicit in several languages; flat-response labels/reset actions were clarified; Hungarian regional settings and Vietnamese audio/preset labels were improved. These replacement translations remain unverified themselves.

`data/localization/translation-context.json` supplies technical definitions in generated Qt Linguist translator notes. Gain means a signed signal-level adjustment in dB; flat means zero EQ gain, not mute; balance means left/right channel level; Quit exits the process while closing the window leaves it running. Notes are shared between the applications and can be reused by the DAW.

## Remaining uncertainty

French, German, Spanish, Italian and both Portuguese variants, Dutch and Polish are populated; the other 24 language packs remain incomplete. All 32 lack native review. Particular review priorities are Swahili audio vocabulary, Thai preset wording, natural microphone EQ terminology in every language, and regional Portuguese and Chinese usage. An AI reread or back-translation by the same model is not independent evidence and must not be labeled native review. Do not promote catalog review status based on this pass.

## Required review workflow

1. Review the English source and actual control behavior with the translator notes. Resolve ambiguous source wording before translating it.
2. A fluent reviewer should compare each target string directly with that intent and flag wrong meaning, unintended connotations, register, regional usage, spelling and unnatural phrasing. Back-translation can help identify questions, but does not replace this comparison.
3. Check strings in the running interface, including RTL, truncation, mnemonics, decimal entry, and complete error messages. Verify Quit, reset and lock semantics explicitly.
4. Record reviewer identity/locale, date, catalog commit, corrections and unresolved issues before setting nativeReviewed. Any later edited strings require renewed review.
5. Keep uncertain entries unfinished so they fall back to English. Keep partial-language coverage visible. Full language qualification also requires the release gates in localization.md.

## Automated checks

Catalog audit checks source inventory, duplicate keys, nonempty finished entries, exact placeholders, literal `&&` preservation, absence of hidden bidi override/isolate controls, coverage and compiled-catalog hashes. The interface supplies RTL direction explicitly. These checks verify structural integrity, not translation accuracy or cultural appropriateness.

## Localization completion checkpoint

The French catalog is now populated for all currently extracted app messages (437 EQ / 523 Studio). Translation and contextual review were performed by this coding agent; no external provider or fluent-speaker certification was used. Coverage includes help, equipment validation, calibration preview, standard Qt actions and Studio render/status text. Every French entry remains unverified. German, Spanish, Italian and both Portuguese variants are also populated; the other 26 languages retain their starter coverage. The independent localization-guard audit agrees with populated coverage; its linguistic/runtime qualification fields remain unknown because that tool cannot assess them. Compiled/runtime evidence is recorded separately in tests/results/localization/checkpoint.json.

## German completion checkpoint

All current EQ/Studio catalog entries are populated in German (437 / 523). This coding agent reviewed the supplied text against actual control meaning: Verstärkung is audio gain; Pegelreserve is headroom; Ohne Effekt is dry signal; Pegelkorrektur is channel trim, not file cutting; Loudness-Korrektur is the listening preset, not a loudness measurement; Rückkopplung is delay feedback. Profile type IDs, user/vendor names, rich-text links, extensions, placeholders and saved processing values remain invariant. Filter Q replaces the imprecise Width (Q) source caption. This is contextual AI review and remains unverified, not native-speaker certification.

## Spanish completion checkpoint

All current catalog entries are populated in Spanish (437 EQ / 523 Studio). Contextual AI review distinguishes Ganancia (audio gain), Factor Q del filtro (dimensionless filter quality), Plano (zero EQ gain), Compensación de sonoridad (low-volume listening correction), Sin efecto (dry signal), and Tiempo de relajación del compresor (compressor release). Manufacturer names, profiles, placeholders, URLs and file filters remain invariant. These translations remain unverified; this does not constitute native-speaker review.

## Italian completion checkpoint

All current extracted entries are populated (437 EQ / 523 Studio). Contextual AI review distinguishes Guadagno (signed audio gain), Fattore Q del filtro, Piatto (zero EQ gain), Compensazione loudness (low-volume listening correction), Senza effetto (dry signal), Rilascio del compressore and Annulla modifica (undo, distinct from Annulla for cancellation). Stable IDs, vendor/user metadata, links, file filters and placeholders remain unchanged. No native-speaker certification was obtained; all entries remain unverified.

## Portuguese regional completion checkpoint

Both pt-PT and pt-BR catalogs populate all currently extracted messages (437 EQ / 523 Studio each). Regional contextual review uses coluna/alto-falante, auscultadores/fones de ouvido, ficheiro/arquivo, guardar/salvar, controlos/controles, recetor/receptor, varrimento/varredura and percentagem/porcentagem. Technical intent preserves zero-gain Plano, low-volume Compensação loudness, Sem efeito for dry signal and signed gain adjustments. Compressor release is Tempo de recuperação do compressor in pt-PT and Tempo de liberação do compressor in pt-BR. This is contextual AI review, unverified; no native-speaker certification is claimed.

## Dutch completion checkpoint

All current extracted entries are populated (437 EQ / 523 Studio). Contextual AI review uses Versterking for signed audio gain, Filter-Q for quality factor, Vlak for zero EQ gain, Loudnesscompensatie for low-volume listening compensation, Zonder effect for dry signal, Hersteltijd van compressor for release and Ongedaan maken for undo. Vendor metadata, machine IDs, file filters, placeholders and source links remain invariant. All entries remain unverified; no native-speaker review is claimed.

## Polish completion checkpoint

All current extracted entries are populated (437 EQ / 523 Studio). Contextual AI review uses Wzmocnienie for signed audio gain, Dobroć filtra Q for dimensionless quality factor, Płaska charakterystyka for zero EQ gain, Kompensacja loudness for low-volume listening compensation, Bez efektu for dry signal, and Czas powrotu kompresora for release. Render counts use Wyrenderowane kanały: %1 to avoid an incorrect fixed grammatical ending. Placeholder-containing route and microphone captions avoid requiring declension of inserted names. Machine IDs, file filters, links and saved parameters remain invariant. All entries remain unverified; no native-speaker review is claimed.

## Czech and Slovak initial batch

Reviewed prompts against actual import, correction-preview, calibration and audio setup behavior. Signed gain uses zesílení/zosilnenie; headroom uses rezerva; meter text retains the estimated-level qualifier. Dynamic route types use a neutral label construction to avoid inflecting inserted names. Device identities, placeholders and numerical settings remain unchanged. Both catalogs remain incomplete and unverified; no native-speaker review is claimed.

Czech/Slovak second batch: wet mix is the effect contribution, not total output volume; delay time is distinct from decay time. Sweep error text explicitly describes a continuously changing test frequency. Failure prompts preserve the conditions under which processing is refused and the app stays open. This is contextual AI review, not native-speaker verification.

Czech/Slovak filter and measurement review: Q is a dimensionless quality factor, shelving differs from pass filtering, compressor attack/release differs from effect decay, and makeup is gain compensation. Loudness describes low-volume tonal compensation. Estimated level labels retain that qualifier. Published model correction remains additive to listening EQ and does not claim room/amplifier calibration. Native-speaker review remains unverified.

## Czech and Slovak completed extracted catalogs

All current extracted entries populated (441 EQ / 527 Studio). The review distinguishes compressor makeup from post gain, pass filters from shelving, measured response from inverted EQ correction and effects from microphone correction. Render summary uses a label form for arbitrary counts. Calibration warnings retain the microphone contribution and bounded suggested gain. File filters, markup URLs, placeholders, IDs and saved numeric parameters are unchanged. Catalogs remain unverified; no native-speaker review is claimed.

## Ukrainian and Russian initial batches

Translated independently with Ukrainian підсилення/чіткість and Russian усиление/чёткость. Dynamic equipment names use neutral constructions to avoid forcing noun declension. Inspection of enhancement.cpp confirms ambience damping reduces high-frequency content in the feedback path; decay describes duration. Estimated-meter qualifiers, clipping warnings and safe failure/paused-state messages remain explicit. This is contextual AI review; both catalogs are incomplete and native-speaker verification remains unverified.

Ukrainian/Russian filter and measurement review: добротність/добротность identifies dimensionless filter Q; компенсаційне/компенсационное підсилення/усиление distinguishes compressor makeup from post gain. Тонкомпенсація/тонкомпенсация means low-volume tone compensation. Negative route gain explicitly reverses polarity. Calibration continues to require a system measurement for room/amplifier effects, and labels distinguish estimated output from measured microphone response. Catalogs remain unverified.
