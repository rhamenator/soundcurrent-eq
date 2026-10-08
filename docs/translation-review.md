# Translation review — 2026-10-07

## Current catalog status — 2026-10-08

All 524 currently extracted source messages have populated translations in all 33 non-English catalogs, including Nynorsk. There are 34 catalogs including English. This count describes catalog coverage; it does not establish that every user-facing string has been extracted. Missing interface strings remain a separate second pass, as requested.

The dated checkpoints below document earlier states and contextual AI review. Their incomplete-language counts are historical, not current status. Automated structural and compiled-catalog checks do not prove linguistic accuracy. Native-speaker verification remains unverified for every non-English locale. Runtime qualification is tracked separately in tests/results/localization. Current Linux lifecycle checks pass on Ubuntu, Fedora and AlmaLinux fixtures. Both current application builds passed Windows installed-package fixtures. An older EQ binary triggered a Defender detection whose false-positive status remains unresolved. Visual review is partial and interactive installer/startup qualification remains pending. No release is authorized by this checkpoint.

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

Ukrainian/Russian startup and render review: render-channel and clipped-sample counts use labels to avoid a fixed plural suffix. Profile editing preserves the measured reference and saves a custom copy. Quit/reopen remains distinct from closing the window. Device-setup changes retain the restart warning and possible playback interruption. Structural checks preserve source hyperlinks and placeholders; native-speaker verification remains unverified.

## Ukrainian and Russian completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q uses добротність/добротность; signed gain uses підсилення/усиление; tone compensation uses тонкомпенсація/тонкомпенсация. Wet mix remains effect contribution, compressor makeup differs from post gain, and measurement results retain microphone influence. Rendering counts use neutral labels. Warnings preserve restart, quiet-start and clipping behavior. Catalogs remain unverified; native-speaker review is not claimed.

## Greek and Turkish initial batch

Greek ενίσχυση and Turkish kazanç describe audio gain; ψαλιδισμός/kırpılma describes clipping. Headroom is περιθώριο στάθμης/seviye payı, distinct from user balance and post gain. Ambience damping refers to high-frequency absorption in feedback and decay to duration. Dynamic model names use neutral route-type constructions. Error messages retain paused processing, kept settings and restart guidance. Contextual AI review remains unverified, with no native-speaker claim.

Greek/Turkish recovery review: processing refusal stays explicit when equalizer inspection fails. File limits and protected settings remain unchanged. Turkish cancel-render text refers to creating an audio file, distinct from live processing. Decay describes effect duration, while wet mix describes contribution. Catalogs remain incomplete and native review unverified.

Greek/Turkish filter/editor review: συντελεστής ποιότητας φίλτρου Q/filtre kalite faktörü Q describes quality, not bandwidth. Υψιπερατό/yüksek geçiren differs from shelving. Compressor compensation and release are distinguished from post gain and effect decay. Estimated-meter labels retain uncertainty, and quitting explicitly restores normal audio. Native review remains unverified.

Greek/Turkish measurement review: loudness is low-volume tonal compensation rather than a global volume increase. Greek offline rendering describes non-real-time processing to avoid implying an internet requirement. Negative route gain means polarity inversion. Correction remains additive and microphone clipping requires reducing input gain/boost. Preset and profile limits preserve numerical meaning; native review remains unverified.

Greek/Turkish startup/render review: sign-in differs from a boot-time service; enabling one app replaces the shared registration. Quitting differs from closing the window. Profile-save instructions preserve the reference copy. Rendering summaries use labels for arbitrary channel and clipped-sample counts. Link text is localized while href attributes remain unchanged. Native review remains unverified.

## Greek and Turkish completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q remains dimensionless quality, headroom is level margin, compressor makeup differs from post gain, and wet mix is effect contribution. Low-volume loudness compensation differs from output volume. Relative measurements include the microphone; measured response differs from inverted correction. Rendering summaries use labels for arbitrary counts. URLs, file filters, placeholders, machine IDs and numerical settings are preserved. Catalogs remain unverified; native-speaker review is not claimed.

## Swedish and Danish initial batch

Regional terms reviewed separately: Swedish förstärkning/klippning and Danish forstærkning/klipning. Headroom is nivåmarginal/niveaumargin, not balance or overall volume. Meter text retains estimated levels and peak warnings. Damping refers to high-frequency absorption while decay refers to duration. Amplifier profiles retain the requirement for actual electrical measurements. Native-speaker review remains unverified; both catalogs are incomplete.

Swedish/Danish recovery review: refusing processing when active-equalizer inspection fails remains explicit. Preset names are distinguished from built-in identifiers, file-size limits remain unchanged, and app-open recovery behavior is retained. Avklingningstid/henfaldstid describes effect decay; effektandel describes processed effect contribution. Native review remains unverified.

Swedish/Danish filter review: kvalitetsfaktor identifies dimensionless Q rather than bandwidth. Hyllfilter/shelving-filter differs from högpassfilter/højpasfilter. Kompensationsförstärkning/kompensationsforstærkning is compressor makeup; decay time is not compressor release. Estimated-meter labels retain uncertainty. Exit actions retain normal-audio restoration. Native review remains unverified.

Swedish/Danish measurement review: loudnesskompensation describes low-volume tonal compensation. Negative linear route gain is explicit polarity inversion. Balance attenuates the opposite channel rather than boosting the selected one. Measurement conditions, conservative model corrections and microphone clipping warnings preserve their original limits. Native review remains unverified.

Swedish/Danish startup/render review: sign-in remains a per-user registration; selecting one SoundCurrent app replaces the other registration. Saving an edited equipment response creates a custom copy and preserves the reference. Offline rendering remains distinct from live processing. Render summaries use neutral count labels. Calibration links retain their exact destinations. Native review remains unverified.

Swedish/Danish import and speaker-profile review: frekvensgång/frekvensgang describes frequency response. Imported points require increasing frequencies and finite bounded values; numeric limits and file globs are unchanged. Efterklang/rumklang describes reverb, with effect contribution distinct from output gain. Restart guidance and automatic-output fallback remain explicit. Native review remains unverified.

## Swedish and Danish completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q remains dimensionless quality, headroom is level margin, compressor makeup differs from post gain, and wet mix is effect contribution. Low-volume loudness compensation differs from output volume. Relative measurements include the microphone; measured response differs from inverted correction. Rendering summaries use labels for arbitrary counts. URLs, file filters, placeholders, machine IDs and numerical settings are preserved. Catalogs remain unverified; native-speaker review is not claimed.

## Norwegian Bokmål and Finnish initial batch

Forsterkning/vahvistus describes gain; nivåmargin/tasovara describes headroom, distinct from balance. Klipping/leikkautuminen describes clipping. Room-effect damping and decay retain separate controls, and dynamic boost retains its peak ceiling. Amplifier profiles still require electrical measurements with a known load; marketing specifications are insufficient. Setup recovery retains paused processing and the open app. Placeholders, units and character limits are preserved. Native-speaker review remains unverified.

Norwegian Bokmål/Finnish recovery review: failures to inspect active equalizers explicitly prevent processing. Setup errors preserve an open app and Windows restart guidance. File-size limits and half-dB steps retain their original values. Avklingningstid/vaimenemisaika describes decay duration; effektandel/efektin osuus describes processed effect contribution. Render cancellation remains distinct from disabling live processing. Native review remains unverified.

Norwegian Bokmål/Finnish filter review: kvalitetsfaktor/laatutekijä describes dimensionless Q rather than bandwidth. Høypassfilter/ylipäästösuodatin differs from shelving. Kompensasjonsforsterkning/kompensointivahvistus describes compressor makeup, separate from post gain. Meter captions retain estimated levels and potential clipping; exit actions restore ordinary audio. Native review remains unverified.

Norwegian Bokmål/Finnish measurement review: loudness compensation remains tonal compensation rather than overall volume. Negative route gain explicitly inverts polarity. Balance attenuates the opposite channel without boosting the selected one; captions use V/H in Norwegian and V/O in Finnish. Model correction remains additive, system effects need measurement, and microphone clipping requires reduced input gain or boost. Limits and schema tokens are preserved. Native review remains unverified.

Norwegian Bokmål/Finnish startup and rendering review: sign-in remains per-user, and selecting one app replaces the other shared registration. Closing the window differs from quitting. Profile edits save a custom copy while preserving the reference. Measured response differs from correction at 48 kHz. Render summaries use count labels without fixed plural endings. Calibration link destinations remain unchanged. Native review remains unverified.

Norwegian Bokmål/Finnish import and speaker-profile review: frekvensrespons/taajuusvaste describes measured frequency response. Imported points require increasing frequencies and finite bounded values; numerical limits and file globs remain unchanged. Romklang/kaikunta describes reverb, distinct from delayed echo and output gain. Device disconnection explicitly switches to automatic output; Windows restart guidance remains intact. Native review remains unverified.

## Norwegian Bokmål and Finnish completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q remains dimensionless quality, headroom is level margin, compressor makeup differs from post gain, and wet mix is effect contribution. Low-volume loudness compensation differs from output volume. Relative measurements include the microphone; measured response differs from inverted correction. Rendering summaries use labels for arbitrary counts. URLs, file filters, placeholders, machine IDs and numerical settings are preserved. Catalogs remain unverified; native-speaker review is not claimed.

## Romanian and Hungarian initial batch

Câștig/erősítés describes gain; rezervă de nivel/szinttartalék describes headroom, distinct from balance. Clipping warnings retain potential distortion, while dynamic boost retains its peak ceiling. Ambience damping and decay remain separate controls. Amplifier profiles require electrical measurements with known load and settings; marketing specifications are insufficient. Setup recovery keeps processing paused and the app open. Placeholders, units and character limits remain unchanged. Native review remains unverified.

Romanian/Hungarian recovery review: failures to inspect active equalizers explicitly prevent processing. Setup recovery preserves an open app. File-size limits and half-dB steps retain their values. Timp de stingere/lecsengési idő describes duration; proporția efectului/effekt aránya describes processed contribution. Romanian clipping warnings describe cut-off peaks, distinct from an intentional limiter. Native review remains unverified.

Romanian/Hungarian filter review: factor de calitate/jósági tényező identifies dimensionless Q rather than bandwidth. Pass filters differ from shelving. Câștig de compensare/kompenzációs erősítés describes compressor makeup, distinct from post gain. Attack and release describe compressor timing, separate from effect decay. Meter labels retain estimated levels and potential clipping; exit actions restore normal audio. Native review remains unverified.

Romanian/Hungarian measurement review: loudness compensation remains tonal compensation rather than overall volume. Negative route gain explicitly inverts polarity. Balance attenuates the opposite channel without boosting the selected one; captions use S/D in Romanian and B/J in Hungarian. Model correction remains additive, system effects need measurement, and microphone clipping requires reduced input gain or boost. Limits and schema tokens are preserved. Native review remains unverified.

Romanian/Hungarian startup and rendering review: sign-in remains per-user, and selecting one app replaces the shared registration. Closing the window differs from quitting. Profile edits save a custom copy while preserving the reference. Measured response differs from correction at 48 kHz. Render summaries use count labels without fixed plural endings. Calibration link destinations remain unchanged. Native review remains unverified.

Romanian/Hungarian import and speaker-profile review: răspuns/frekvenciamenet describes measured frequency response. Imported points require increasing frequencies and finite bounded values; numerical limits and file globs remain unchanged. Reverberație/zengetés describes reverb, distinct from delayed echo and output gain. Device disconnection explicitly switches to automatic output; Windows restart guidance remains intact. Native review remains unverified.

## Romanian and Hungarian completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q remains dimensionless quality, headroom is level margin, compressor makeup differs from post gain, and wet mix is effect contribution. Low-volume loudness compensation differs from output volume. Relative measurements include the microphone; measured response differs from inverted correction. Rendering summaries use labels for arbitrary counts. URLs, file filters, placeholders, machine IDs and numerical settings are preserved. Catalogs remain unverified; native-speaker review is not claimed.

## Nynorsk initial batch

Nynorsk is its own written standard and catalog, separate from Bokmål. Wording uses innstillingar, einingar, forsterking and førehandsinnstilling consistently. Nivåmargin remains distinct from balance, and clipping warnings retain estimated peak risk. Amplifier profiles require electrical measurement, not marketing specifications. Setup recovery preserves an open app with paused processing. Regional language selection retains nn and nb separately. Native review remains unverified.

Nynorsk recovery review: inability to inspect active equalizers explicitly prevents handsaming. Setup errors keep the app open. Limits and half-dB steps retain their values. Avklingingstid describes duration, while effektdel describes processed contribution. Render cancellation remains distinct from disabling live playback processing. Native review remains unverified.

Nynorsk filter review: kvalitetsfaktor identifies dimensionless Q, distinct from bandwidth. Høgpassfilter differs from hyllefilter. Kompensasjonsforsterking identifies compressor makeup, separate from post gain. Attack/release timing remains separate from effect decay. Meter captions retain estimated levels and possible clipping; exit restores normal audio. Native review remains unverified.

Nynorsk measurement review: loudness compensation remains low-volume tonal compensation rather than overall volume. Negative route gain explicitly reverses polarity. Balance attenuates the opposite channel without boosting the selected one; endpoint captions use V/H. Model correction remains additive, system effects require measurement, and microphone clipping requires lower input gain or boost. Numerical limits and schema tokens are preserved. Native review remains unverified.

Nynorsk startup/render review: innlogging describes user sign-in rather than system boot. Enabling one app replaces the shared registration. Profile saving preserves the reference and creates an eigendefinert kopi. Render summaries use neutral labels for arbitrary counts. Measured response differs from correction at 48 kHz; measurement suggestions retain the 3 dB limit. Link destinations and placeholders are unchanged. Native review remains unverified.

Nynorsk import and speaker-profile review: frekvensrespons describes measured response. Imported points require increasing frequencies and finite bounded values; numerical limits and file globs remain unchanged. Romklang describes reverb, distinct from delayed echo and output gain. Device disconnection explicitly switches to automatic output; Windows restart guidance remains intact. Native review remains unverified.

## Nynorsk completed extracted catalogs

All currently extracted messages populated (441 EQ / 527 Studio). Q remains dimensionless quality, headroom is level margin, compressor makeup differs from post gain, and wet mix is effect contribution. Low-volume loudness compensation differs from output volume. Relative measurements include the microphone; measured response differs from inverted correction. Rendering summaries use labels for arbitrary counts. URLs, file filters, placeholders, machine IDs and numerical settings are preserved. Catalogs remain unverified; native-speaker review is not claimed.

## Arabic initial batch

الكسب describes gain; هامش المستوى describes headroom, distinct from channel balance. قص قمم الإشارة describes clipped signal peaks. Room-effect damping differs from decay duration. Amplifier curves require electrical measurements with known load and settings; marketing specifications are insufficient. Processing pauses during setup and recovery keeps the app open. Placeholders, units and file/character limits remain unchanged. Arabic EQ was sampled at 1280×720; full bidi and native-speaker review remain unverified.

## Second-pass dialog titles and percentages (2026-10-08)

The amplifier confirmation title refers to applying measured response correction
through EQ, not adjusting the hardware gain or changing firmware. The conflict
title refers to competing software processing/routing ownership, not distortion
or an acoustic measurement failure. Both titles received contextual AI review
and translations in all 33 non-English catalogs. Arabic wording explicitly
identifies audio-equalization software. Native-speaker verification remains
unverified for these additions.

Tray Open and equalizer state captions reuse existing translations. Enhancement
amount labels now use the selected regional digits and percent symbol at
initialization, live edits and settings restoration; numerical processing is
unchanged. Per-locale Qt fixtures exercise all five percentage controls.

The literal Qt inventory does not cover every dynamic expression, saved/user
name, backend diagnostic or installer message. Its remaining language-picker
caption `English (en)` is an intentional native autonym plus stable locale tag.
Whole-interface coverage remains incomplete.

## Amplifier preview and details prose (second pass, 2026-10-08)

All 33 non-English catalogs now translate the measurement-condition label,
source label, conditional apply warning and correction-filter heading. The
warning refers to using a measured amplifier response only with matching
electrical load, input and tone settings. It is not a recommendation to copy
a curve measured with a different speaker load. Contextual review is AI review;
native-speaker verification remains unverified.

Profile names, original measurement conditions, attribution URLs and filter
JSON remain supplied data. Display-only direction isolates keep that content
separate from the translated surrounding prose, without changing stored values.
Every locale fixture checks names and conditions containing literal `%1`/`%2`,
URL escapes, and numerical/filter JSON so placeholder substitution cannot
rewrite imported content. This checks strings and data boundaries; full native
dialog visual qualification remains a later package gate.

## Speaker details (second pass, 2026-10-08)

The measurement-attribution label, correction policy and peaking/low-shelf/high-
shelf names are now translated in all 33 non-English catalogs. Contextual AI
review distinguishes bell-shaped peaking EQ from peak/clipping indicators and
shelving EQ from low/high-pass cutoff filters. Established technical shelving
loanwords are retained where appropriate. Native-speaker verification remains
unverified.

The policy describes correction-only gain ±6 dB, Q at most 6, and omitted
positive-gain filters below 80 Hz. These match the unchanged speaker-profile
loader validation. Listening preset EQ is added separately and can exceed those
correction-only limits. Numbers are now regional display values, while processing
and stored profile fields are unchanged.

Published names, attribution and URLs remain supplied content. Display isolates
keep numerical/unit tokens and URLs together. The details dialog explicitly uses
plain text. All locale fixtures test the attribution boundary, negative gain,
frequency/Q formatting, URLs and policy tokens; native dialog layout/glyph
qualification still belongs to the final package tests.

## Second pass: stable equipment taxonomy keys

Speaker and equipment-library subtype dropdowns now store the original equipmentType key in item data. Filtering and refresh no longer depend on display text. Regression fixtures replace a caption with French/Japanese text and verify the speaker results and preserved subtype selection. All 34 equipment UI tests and 38 shared/localized/catalog tests passed on Linux with Qt offscreen. Built-in taxonomy labels still need translation; this prerequisite does not establish full interface coverage or Windows qualification. Native-speaker verification remains unverified.

## Second pass: common speaker categories and model qualifier

Translated bookshelf, center-channel, floorstanding, in-wall and unclassified equipment captions in all 33 non-English locales, plus the original Sony SS-CS5 qualifier. Shared display mapping preserves original keys and returns unknown/custom subtype text verbatim. Contextual AI review distinguishes center-channel and enclosure/installation types, and preserves the SS-CS5M2 model identifier. Native-speaker verification remains unverified.

All 34 equipment UI tests and 38 shared/localized/catalog tests passed on Linux Qt offscreen; the 10 extraction unit tests also passed. Runtime fixtures check the actual speaker dropdown labels and keys, original Sony qualifier, taxonomy refresh and custom Unicode/placeholder preservation. Ten less common built-in categories still need translation. Windows and package qualification for this source change remain pending. Zero unfinished catalog entries does not prove whole-interface extraction coverage.

## Second pass: remaining speaker categories

Translated Cinema, Column, Constant beamwidth, Omnidirectional, Outdoor, Panel, Portable PA, Soundbar, Surround and Touring PA in all 33 non-English locales. The shared mapper now covers all 15 types present in the bundled Spinorama database. A regression check compares database taxonomy keys with marked display mappings, requiring explicit coverage for future categories. Keys and unknown/custom subtype text remain unchanged.

Contextual AI review distinguishes column arrays from home floorstanding speakers, panel speakers from UI panels, speaker radiation from microphone pickup, and touring/portable PA from other abbreviations. Some languages conventionally borrow Soundbar or PA. Constant beamwidth means consistent angular acoustic coverage, as described in [JBL’s official CBT documentation](https://jblpro.com/en-US/products/cbt-70j-1), rather than a fixed frequency bandwidth. Native-speaker verification remains unverified.

Builds, complete catalog checks and all 11 extraction/taxonomy unit tests passed. The Linux runtime matrix is not yet qualified: several equipment fixtures timed out while host resource load was high, and an isolated EQ Hindi retry also timed out. Resource pressure is a hypothesis requiring further investigation, not an established explanation. Original failures must remain in the report even after any later successful retries. Windows and changed-package qualification remain pending.

## Equipment fixture timing investigation

The original equipment UI fixture used a single 20-second deadline for its complete sequence of editor, Cancel/Discard, save, malformed/valid import and Apply dialogs. Instrumented Studio Swedish and Swahili retries still advanced through phases near 18 seconds before that deadline. EQ Turkish and Finnish fixture processes were observed in uninterruptible I/O sleep at `jbd2_log_wait_commit`, establishing filesystem journal waits as a contributor. This does not prove an explanation for every earlier failure.

The fixture now clicks the actual warning OK button, logs phase timings and watches for 20 seconds without phase progress. An external CTest cap of 120 seconds remains: the GUI watchdog cannot execute while its event loop is blocked. Application code, profile persistence and QSaveFile commit behavior are unchanged. Earlier failures and intermediate retries are retained; updated full locale runs are reported separately. This is functional UI qualification, not a performance certification or native linguistic review.

Updated full equipment locale qualification passed: 34/34 tests in each application, run serially with the progress watchdog and external cap. Passing logs and current application binary hashes are saved with equipment-ui-watchdog.json. This supersedes the earlier incomplete equipment runtime qualification while retaining all original failures and intermediate retry results. Current main-window/catalog checks had separately passed all 38 targeted tests. Windows, changed-package qualification, remaining extraction gaps and native linguistic verification are not established by these Linux offscreen results.

## Second pass: saved setup schema, numeric and Boolean diagnostics

Added display mappings and translations in all 33 non-English locales for unsupported Studio profile schema, invalid numeric field, and invalid Boolean field. Contextual AI review treats these as saved setup validation errors: schema denotes format/version and required structure; numeric fields require finite JSON numbers within the permitted range; Boolean fields require actual true/false values. Backend reason strings, saved schema, processing numbers and unknown/external diagnostic text remain unchanged. Native-speaker verification remains unverified.

All 39 targeted Linux Qt offscreen tests passed in each app. Studio locale fixtures directly reject malformed schema, a numeric string containing a comma, and textual true; they verify invariant reasons, translated display mapping, unchanged inputs and valid profile roundtrip. These fixtures do not click a malformed-file import dialog. Runtime logs and binary hashes are retained in second-pass-profile-diagnostics.json. Source/catalog unit checks passed: Studio 12; EQ 11 with one Studio-specific check skipped. Fourteen owned Studio diagnostics still need translation mappings, and a source audit now checks that the reported gap set matches the actual model. Windows and changed-package checks remain pending.

## Second pass: remaining owned session diagnostics

Added the remaining 14 Studio session-model validation mappings and translations in all 33 non-English locales. All 17 reasons in this inventory now have explicit display mappings; source/catalog regression verifies the reported remaining set is empty. Backend keys, saved schema and processing remain unchanged; unknown external diagnostic text is retained verbatim. This inventory does not prove whole-interface coverage.

Contextual AI review distinguishes parameter bounds from acoustic frequency ranges and saved routing/filter structure from UI controls. The Nynorsk plural parametrar was checked against the [official dictionary](https://ordbokene.no/nn/parameter); this supports that term only. Native-speaker verification remains unverified.

Both single-job builds, fresh complete catalog checks and all 39 targeted Linux Qt offscreen tests passed per app. Studio fixtures reject malformed channel/filter/routing/enhancement data and verify unchanged input, valid signed routing and saved profile roundtrip through the parser/display boundary. These are not malformed-file picker tests. Current counts are 568 messages for EQ and 676 for Studio. Evidence and binary hashes are in second-pass-owned-diagnostics.json. Windows and changed-package qualification remain pending.

## Second pass: Qt-wrapped source caption guard

The source guard and candidate inventory now recognize exact QStringLiteral, QString, QLatin1String and QLatin1StringView wrappers. Twelve synthetic tray/combo/dialog cases prove those wrappers cannot bypass the caption guard. The invariant literal extractor remains separate; translated expressions and dynamic/user text are not interpreted as literals. Commented-out captions remain excluded.

Source/catalog unit checks passed (EQ 14 plus one Studio-only skip; Studio 15), and fresh complete catalog checks passed. The narrow inventory found only English (en), an intentional language self-name. This result excludes formatted/dynamic expressions, stored names, backend/helper diagnostics and installer text, and does not prove total interface coverage. Application and catalogs were unchanged by this batch, so no build or runtime repetition was needed. Current accumulated Windows/package changes remain unqualified. Evidence is in second-pass-wrapped-literal-audit.json.

## Second pass: Windows setup catalog lookup

Audio helper dialog titles now reuse the translated Audio driver setup caption. A shared data-only PowerShell lookup reads a generated export of finished TS entries; build staging packages the data/script, and pre-install cable readiness includes them. Exact payload deletion manifests include the new owned stage files. Helper actions, driver verification, ownership and exit codes are unchanged. Native driver source is preserved.

Both host PowerShell 7.6.6 runs passed all 34 explicit title catalog lookups, regional/Chinese selection cases, unknown external text, missing/corrupt/unsupported data and non-string fallback, plus packaged PowerShell parsing. The lookup reads app language preference on Windows and otherwise system UI culture; real registry preference reads, Forms dialogs and Windows PowerShell 5.1 remain unqualified. Windows package builds now run these inert tests with powershell.exe before installer creation. No driver/endpoint actions or VMs were used.

This batch covers catalog infrastructure and the common title only. Most helper bodies, other captions, NSIS pages/messages and app command-line language propagation still require work. Existing catalog completeness does not establish helper coverage. Native linguistic verification remains unverified, and changed-package install/update/uninstall testing is pending. Evidence is in second-pass-setup-catalog-lookup.json.

## Second pass: setup helper loaded-language propagation

The runtime records its actually loaded interface catalog, and Windows audio setup passes that tag in a separate Language argument. The shared argv builder preserves script path, action, quiet mode, requester ID and execution policy. Cable/native helpers accept the tag; standalone use can retain saved-preference/system selection. No language preference is rewritten. Lookup source keys now use ordinal, case-sensitive comparison so differently cased unknown text remains raw.

Single-job builds and targeted Linux Qt offscreen checks passed (EQ 39; Studio 42). Fixtures check loaded catalog propagation and the compiled argv builder, including Unicode/spaces in a script path. Both inert host PowerShell 7.6.6 runs passed all 34 title lookups, fallback cases, exact-case behavior and helper parameter AST checks. Windows process launch, Forms and PowerShell 5.1 are not qualified by these tests. Evidence and binary hashes are in second-pass-helper-language.json. Native linguistic review remains unverified.

An additional boundary gap was found: the app currently decodes helper output using fromLocal8Bit, without an explicit PowerShell UTF-8 output contract. Fix this before translated bodies are added. Most helper bodies and NSIS text remain untranslated, and current changed-package qualification is pending. No VMs or driver/endpoint operations were used.

## Second pass: audio helper UTF-8 output protocol

Cable/native helper output now explicitly uses UTF-8 without a BOM, and the app decodes accumulated process output with the shared UTF-8 decoder. Existing whitespace trimming is retained. An ASCII-source inert PowerShell fixture emits French, Japanese, Arabic and placeholder text; a compiled Qt test launches it and verifies captured bytes decode exactly. The MSVC test target explicitly uses UTF-8 source compilation. Windows package builds run the fixture with powershell.exe and a process-only RemoteSigned policy.

Single-job builds and all targeted host checks passed (EQ 40; Studio 43), including the real inert PowerShell-to-Qt process boundary. Both 34-catalog helper lookup suites also passed after converting their external Unicode test payload to ASCII-source/base64 form for PowerShell 5.1 compatibility. Production driver helpers, Windows PowerShell 5.1 and actual installers were not executed. Source assignments in the real helpers are reviewed; the process fixture is deliberately inert. Evidence is in second-pass-helper-utf8.json.

This closes the previously recorded host output-encoding prerequisite, not the remaining helper body/NSIS translation or Windows/package qualification. Native linguistic verification remains unverified. No VMs, driver/endpoint operations or machine execution-policy changes were used.

## Second pass: owned audio setup validation errors

Translated three cable setup errors in all 33 non-English locales: choose one action, invalid requesting process, and missing route-preserving helper. Existing action predicates, English source keys and exit codes remain unchanged. Contextual AI review distinguishes an action switch from device selection, the requesting process from the human, and audio endpoint routing from navigation/network routing. Native-speaker verification remains unverified.

The setup source manifest now declares the common title and these three errors. Export requires finished translations for every declared key. Regression checks compare literal helper calls with the manifest, including a PowerShell AST check, and reject unfinished required text before writing an export. This only covers declared lookup calls, not remaining English helper bodies.

Single-job builds and targeted Qt checks passed (EQ 40; Studio 43). Source/catalog unit checks passed (EQ 16 plus one Studio-only skip; Studio 18). Both host PowerShell suites passed 136 required lookups. Actual production helper invocations in French, Arabic and Nynorsk used no action switches and Quiet; they returned exact translated UTF-8 action-validation text with exit 30 before any driver/PnP check. The requester and missing-helper production error paths were not executed. Evidence is in second-pass-setup-errors.json. Windows and changed-package qualification remain pending. No VMs or driver/endpoint actions were used.

## Second pass: setup package integrity errors

Translated missing VB-CABLE package, checksum mismatch and executable signature-verification failure captions in all 33 non-English locales. The pinned SHA-256, Authenticode Valid requirement, driver actions, exit codes and stable vendor/app identifiers are unchanged. Contextual AI review treats the missing package as the bundled ZIP, checksum as file integrity rather than signal quality, and signature as executable signing without guessing the cause of failure. Native-speaker verification remains unverified.

Single-job builds and targeted Qt checks passed (EQ 40; Studio 43); source/catalog checks passed (EQ 16 plus one skip; Studio 18). Both PowerShell suites passed 238 required lookups and the existing safe quiet action-error invocations in fr/ar/nn. These results do not execute the newly translated production integrity-failure paths. Windows and changed-package qualification remain pending. Evidence is in second-pass-setup-package-errors.json. No VMs or driver/endpoint actions were used.

## Second pass: safe setup template formatting

Added a formatter for owned setup templates with numbered %1..%99 placeholders. Translation may reorder tokens, but must retain their exact multiset; damaged translations fall back to the English template. Replacement is single-pass, so inserted Unicode, dollar signs, paths and literal %1 text remain data. Localized-number and numerus placeholders are explicitly unsupported by this API. Production dynamic messages have not yet been converted.

Both host PowerShell 7.6.6 suites passed 238 required lookups and formatting fixtures, with safe quiet action-error invocations in French, Arabic and Nynorsk. Catalog unit checks passed (EQ 16 plus one skip; Studio 18). No C++ or catalog entries changed, so binaries were not rebuilt. Windows PowerShell 5.1, actual dialogs and changed packages remain unqualified. Native-speaker verification remains unverified. No VMs or driver/endpoint actions were used. Evidence: second-pass-setup-formatting.json.

## Second pass: setup failure messages with error codes

Translated the native driver-manager failure and VB-CABLE cancelled/incomplete setup messages in all 33 non-English locales, and wired them to the safe formatter. The numbered token carries the original process exit code as literal text. Contextual AI review preserves the distinction between cancellation and incomplete setup, between retaining the application installation and preserving audio settings, and between an unchanged Windows security configuration and a claim that installation succeeded. Stable app/vendor identifiers remain unchanged. Native-speaker verification remains unverified.

Host PowerShell checks exercise both real declared templates in all 34 catalogs with signed decimal, hexadecimal and literal-placeholder values. Trusted fallback function definitions from both production scripts are tested in isolation when the localization payload is absent; driver script bodies are not executed. The existing quiet no-action validation fault is also exercised in French, Arabic and Nynorsk. These tests do not execute production driver-manager/cable failure paths, Windows dialogs or installer lifecycle. Current evidence is in second-pass-setup-code-errors.json. Other helper bodies, the native catch prefix and NSIS text still require translation.

## Second pass: native setup failure heading

The native helper now reuses the translated action-validation message and formats a translated incomplete-setup heading around literal exception details in all 33 non-English locales. Contextual AI review treats setup as audio driver configuration and the inserted exception as technical data. Native-speaker verification remains unverified. Other native helper bodies remain incomplete.

Both actual helper no-action error paths passed on host PowerShell 7.6.6 in French, Arabic and Nynorsk, with exit 30 and exact UTF-8 output before driver/endpoint checks. Catalog/unit checks passed, plus targeted Linux Qt tests (EQ 40; Studio 43). A brief Windows test was prepared, but the VM could not start because an existing paused qemu-img copy held its disk lock; that process was left untouched and the VM remained off. Windows runtime and current package lifecycle qualification remain pending. Evidence: second-pass-setup-failure-heading.json.

## Second pass: setup prose extraction and regression guard

A parser-backed audit found 28 raw prose candidates in each app’s two shipped setup helper sources: two invariant product names and 26 untranslated captions or fragments. It includes returned/concatenated quit instructions previously outside the declared-lookup inventory. The explicit backlog preserves these gaps and their source locations. It is not a claim of complete extraction or translated coverage.

Both host PowerShell 7.6.6 checks passed AST fixtures for raw throws, interpolation, returned fragments and dialog bodies/titles, plus comment and marked-source exclusion. A mutation test injected a new untranslated caption into a temporary copy and confirmed that the actual gate rejects it. Existing 340 required lookups and both safe helper no-action paths in fr/ar/nn passed. No app/catalog changes, builds, VMs or driver operations occurred in this batch. Windows and changed-package qualification remain pending. Native-speaker verification remains unverified. Evidence: second-pass-setup-prose-guard.json.

## Second pass: native package and signing errors

Translated missing shared driver-manager executable, incomplete/unverifiable driver package, and unsigned manager errors in all 33 non-English locales. Contextual AI review distinguishes the bundled helper executable from Windows Device Manager; Hebrew wording was adjusted explicitly. Digital signatures refer to executable/driver-package trust, not audio characteristics. The alternative package-error wording does not guess the reason for verification failure. The app repair and signed-release instructions retain their original meaning. Native-speaker verification remains unverified.

The three resolved raw candidates were removed from the explicit backlog, leaving 23 untranslated candidates and two invariant product names. Required setup lookups now total 442 across 34 catalogs. Host PowerShell checks also run the actual native helper with a temporary payload that lacks the manager executable: French, Arabic and Nynorsk return exact translated UTF-8 detail inside the failure heading, exit 30, before identity lookup, process waits, elevation or driver operations. Production signature/package-validation failure paths were not executed. The source predicates, signing checks, ownership, exit codes and driver actions are unchanged.

Current compiled catalog/interface and host script evidence is in second-pass-native-package-errors.json. No VM was used in this batch. Actual Windows runtime and changed-package lifecycle qualification remain pending; the remaining helper/NSIS scope is not complete.

## Second pass: native setup lifecycle notices

Translated the restart-required and driver-ready notices in all 33 non-English locales. Replaced the misleading instruction to quit both EQ and Studio with an instruction to quit running SoundCurrent apps and wait for audio recovery to finish. This matches the existing process wait, which checks the two applications and route guardian. No process predicate, wait duration, driver operation, return status or ownership rule changed. Contextual AI review distinguishes a Windows system restart from reopening the app and keeps SoundCurrent Audio as the driver’s invariant name. Native-speaker verification remains unverified.

The real native success-notice conditional is extracted with the PowerShell parser and executed in an isolated scope with a Notice output stub. Across all 34 catalogs, 136 cases verify code 3010 emits the restart notice for installation or removal, successful installation emits the ready notice, and successful removal emits no ready notice. This runs only that trusted source branch, not driver installation. Actual process-wait timeout and Windows Forms dialogs remain unqualified. Existing quiet no-action and missing-manager faults, template formatting and the mutation guard also passed on host PowerShell 7.6.6.

The raw prose backlog now has 20 untranslated candidates and two invariant product names, all in the cable helper; the native helper has no remaining candidates under this specific AST heuristic. This is not full interface-coverage proof and excludes NSIS and external error data. Current evidence: second-pass-native-notices.json. Windows and current package lifecycle qualification remain pending. No VMs were used in this batch.
