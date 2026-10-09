# Localization

EQ and Studio share the same Qt translation/runtime interface. It is reusable by the DAW without coupling the DSP to Qt. Copperfin was inspected read-only: its deterministic fallback, pseudo localization, placeholder checks and separation of machine contracts inform this design. No Copperfin or DAW files were modified.

## Current coverage

English is the source language. **All 33 non-English catalogs, including Nynorsk, are populated for the 524 currently extracted source messages. No language pack is native-reviewed.** This is catalog coverage, not proof that every user-facing message has been extracted. Missing interface strings are deferred to the requested second pass. Populated translations remain unverified; missing messages fall back to English. `data/localization/catalogs.json` records exact counts and review status. Contextual AI review, native-speaker verification, compiled runtime checks and visual/package qualification are separate evidence categories.

Coverage is global: European languages, Arabic, Hebrew, Persian, simplified/traditional Chinese, Japanese, Korean, Hindi, Indonesian, Vietnamese, Thai and Swahili. Portuguese for Portugal and Brazil and the two Chinese scripts are separate catalogs. Regional fallback reuses a base-language catalog only where one is explicitly available; explicit script variants are not collapsed into another script. Qt locale data maps region-only Chinese tags (zh-TW/zh-CN) to the corresponding available script catalog and accepts equivalent explicit Latin-script Portuguese tags. Without a Portuguese region, Qt’s default territory selects the regional pack; unsupported explicit regions fall back to English. Unicode/private-use formatting extensions do not block language selection. The prior 143-item Europe inventory is retained as **planned** work in `language-inventory.json`, including minority languages. The inventory is extensible and is not a claim of full global coverage.

The number/date locale selector independently exposes the locales supplied by the installed Qt version (hundreds of language/region/script combinations). Having a formatting locale does **not** mean that the UI is translated into its language. Qt 6.4 and Qt 6.12 may provide different inventories.

## Use

Open **Settings & calibration → Language and regional settings**. Choose the interface language and number/date format independently. Use **Quit** and reopen to apply changes; closing the window leaves the process running. This avoids rebuilding views while live audio is active. Unsupported languages or missing messages use English. Native OS file dialogs may continue using the operating system's language.

Selection precedence: `--language TAG`, saved app preference, `SOUNDCURRENT_LANGUAGE` when the preference is system, then system UI-language preferences, then English. Example:

```sh
soundcurrent-eq --language fr-CA
soundcurrent-studio --language pt-BR
```

Language selection uses embedded resources; it never downloads catalogs or executes translation files. Settings persist stable language tags. Preset IDs, model/brand names, custom names, schema keys, routing IDs, frequency limits, units and JSON numbers stay invariant. Built-in preset display names can be translated while their item data retains the original ID. Qt numeric controls parse the selected number locale; JSON/DSP use numeric values. RTL layouts do not reverse the frequency band order or change channel mappings.

## Translators and maintenance

Edit `data/localization/soundcurrent_TAG.ts` with Qt Linguist. The shared context is `SoundCurrent`; user-facing literals use `SC_TR(...)`, or `soundcurrent::i18n::text(...)` at a data-driven view boundary. Keep `%1`, `%2`, `%n` and `%L1` placeholders intact. Use full sentences with placeholders instead of concatenated translated fragments. Current UI messages are not numerus messages. The maintenance script rejects numerus entries before rewriting any catalog, until explicit Qt language-specific plural rules and regression tests are implemented. Unfinished translated text and translator comments are preserved during updates.

```sh
python3 scripts/localization.py --update
python3 scripts/localization.py --check
```

Updating requires Qt Linguist `lrelease` (Qt 6 preferred). Compiled QM files are committed with SHA-256 provenance in `catalogs.json` and embedded in every Linux/Windows build; ordinary builds need only Qt Core/Widgets. The audit requires only Python and checks extraction freshness, blanks, duplicate keys, placeholders, literal ampersands, rich-text link/tag integrity, file-dialog extension patterns, embedded resource inventory, coverage and TS/QM hashes. `--require-complete` rejects any unfinished translation across all languages. `requiredCompleteLocales` in the seed inventory prevents already-populated languages from silently acquiring blanks when UI text changes. Add a locale and its native name to `seed-translations.json` before the first update; subsequent updates preserve existing translator edits. Standard Qt action captions use the embedded application catalog; OS-native dialogs may retain the operating system language. Use proper language/script/region tags; do not copy a regional label to imply a reviewed regional translation.

Developer-only test languages `qps-ploc` and `qps-rtl` expand text and exercise RTL layout. `--localization-ui-test --language TAG` constructs the real views without starting audio/network processing. `--ui-self-test` remains deterministic English regardless of the user's preferences.

Before qualifying a language: translate **all** messages, obtain native-speaker review of audio terminology and regional usage, check dialogs/accessibility/mnemonics, decimal input, Unicode paths, RTL channel/frequency ordering, 1280×720 and HiDPI layouts on both Linux and Windows. Installer text and native driver/vendor interfaces are separate translation work; 1.1.0 installers remain English. The native driver and system routing contracts stay unchanged.

References: [Qt internationalization](https://doc.qt.io/qt-6/internationalization.html), [QTranslator](https://doc.qt.io/qt-6/qtranslator.html).

## Translation accuracy

See [translation review](translation-review.md) for contextual corrections, known uncertainties and fluent-review requirements. Audio terminology notes in `data/localization/translation-context.json` are embedded in Qt Linguist catalogs. Structural audits do not certify linguistic accuracy.

Locale inference uses [Qt QLocale language, script and territory data](https://doc.qt.io/qt-6/qlocale.html), rather than treating every four-character suffix as a script.

### Windows audio helper language

The app passes the catalog it actually loaded as a separate `-Language`
argument to cable/native audio setup. This follows a command-line language
override without changing persisted language preferences. The shared argument
builder retains the script path, requester ID, quiet mode and existing action.
Standalone shortcuts may omit the argument and use saved preference/system UI
culture through the helper lookup. Unsupported helper catalog tags fall back
to English; the development pseudo-locales are not exported as helper packs.

Helper translation source keys are compared ordinally and case-sensitively.
Unknown external text, including different capitalization, is kept verbatim.
The common dialog title is localized; most helper message bodies and NSIS
pages still need translation and actual Windows package qualification.

### Audio helper output encoding

Cable and native setup scripts set console output and native-pipeline encoding
to UTF-8 without a BOM. The app accumulates QProcess output chunks and decodes
the finished buffer as UTF-8, retaining the existing whitespace trimming.
This protocol is independent of the Windows ANSI code page and number locale.

An inert ASCII-source PowerShell fixture emits a UTF-8 payload containing
French, Japanese, Arabic and a literal placeholder. A compiled Qt test captures
that child process output and checks the exact decoded value. Windows package
builds run it explicitly with powershell.exe and a process-only RemoteSigned
policy; no machine execution policy, audio endpoints or driver state is changed.
Host PowerShell qualification does not prove Windows PowerShell 5.1 behavior.

### Owned setup template formatting

Use Format-SCSetupText with a literal owned Source and a separate Values array. It supports numbered %1 through %99, including repeated tokens and reordered translations. Values are inserted once as literal data. A translated token mismatch falls back to the source template; missing values or unsupported %L/numerus placeholders reject the template. Do not pass arbitrary caught external diagnostics as owned templates. Production lookup sources must be declared in setup-sources.json and pass the AST inventory check.

### Setup prose regression guard

The PowerShell AST candidate audit covers raw English-like literals anywhere in the two shipped cable/native helpers, including returned fragments, interpolation and dialog captions. Declared Get/Format-SCSetupText source arguments are excluded. setup-prose-backlog.json distinguishes invariant product names from untranslated candidates. New candidates fail the inert setup test; removing translated candidates is allowed. Ordinary checks never regenerate the backlog. The backlog is unfinished work, not a coverage exemption proving completion.

The heuristic does not establish full interface extraction, does not parse NSIS, and does not interpret external variable-only diagnostics. Use the separate GUI/backend audits as well.

### External installer labels

EXTERNAL_UI_LABELS in the catalog validator is scoped to reviewed instruction source keys. It preserves the exact bundled third-party button label and its occurrence count. Do not freeze SoundCurrent-owned captions globally. The cable repair instruction preserves Install Driver and inserts the separately translated Audio driver setup caption through a numbered value. Native linguistic verification and actual Windows installer qualification remain separate requirements.

### Installer source audit

nsis_string_audit.py inventories supported custom controls, MessageBox/DetailPrint text, section captions, MUI headers and text definitions. nsis-text-backlog.json is an explicit unfinished-work inventory; new raw text or unaudited language references fail catalog unit tests. It is not a complete NSIS parser and does not expand macros or line continuations. A $(key) reference does not prove its definition, translation, layout or installed runtime behavior. Compiler and install/update/uninstall checks remain separate gates.

The source-specific external-label rule also preserves CABLE Input and CABLE Output in the reviewed Windows endpoint diagnostic. These are default external device labels; user-edited names and SoundCurrent-owned action captions remain separate.

### Windows PowerShell 5.1 qualification

The 2026-10-08 inert helper run passed on Windows PowerShell 5.1.26100.9549: 1,122 required source/catalog lookups per application, isolated message fixtures and real quiet no-action errors in French, Arabic and Nynorsk. See second-pass-windows-ps51.json and its runtime log. The initial policy-blocked invocation is retained; the retry used process-only RemoteSigned. This does not qualify current installed Qt UI, driver operations or installer lifecycle. Native-speaker verification remains unverified.

The repair-failure instruction now uses a translated Audio driver setup caption through %1, with source-specific protection for Remove Driver, CABLE Input and CABLE Output. Host checks cover all 34 catalogs and reject altered labels and placeholders. One longer cable repair notice remains untranslated. Earlier Windows PowerShell qualification applies to its recorded source commit; it does not qualify this new instruction. See second-pass-cable-repair-failure.json.

### Helper source inventory and extraction

Catalog extraction includes setup-sources.json directly; a seed entry is no longer required merely to retain a declared helper source. Adding a declaration invalidates existing catalogs until they are updated and translated. Declarations must be a list of unique, nonempty strings. This is separate from the PowerShell AST gate that checks literal calls against declarations, and does not prove complete extraction of unmarked interface text. Tests and scope are recorded in second-pass-setup-source-inventory.json.

### Direct installer language definitions

Reviewed $(key) references now require one nonempty direct LangString definition for each declared MUI language in that installer file. Missing languages and duplicate definitions fail the gate. This limited check does not expand includes or macros and normalizes MUI names to LANG tokens; language aliases need explicit support before use. It does not establish translation accuracy, compiler success, layout or whole-interface coverage. Existing English installer text remains pending translation. See second-pass-nsis-language-definitions.json.

### Installer audio page heading

Connect your audio is translated in all 33 non-English catalogs. Both installer variants reference SCConnectAudio with a direct English definition. Tests keep this source synchronized with finished catalog entries. Minimal fixtures using the actual definition and header lines compile in NSIS; full current installer builds and installed UI qualification remain pending. Only English is currently enabled in the installers. Translated heading entries alone do not provide localized installer pages or language selection. See second-pass-installer-audio-heading.json.

The audio page subtitle uses the shared catalog template Set up %1 for %2., translated across all 33 non-English locales. Driver and application product names remain exact; ordering follows each translation. Both installer variants reference SCSetupAudio with direct English definitions. Rendering/placeholder tests and minimal NSIS compiler fixtures passed. These templates are prepared for future installer locale activation; current installers still enable only English. Full current package and UI qualification remain pending. See second-pass-installer-setup-subtitle.json.

The cable and native driver checkbox captions are translated across all 33 non-English catalogs, with exact driver-name guards. Cable installation retains its absence condition and administrator approval wording; the native option retains shared install/update semantics. Both installer variants reference SCInstallDriver with their respective direct English definitions. Actual definition/control lines compile in minimal NSIS fixtures, and all catalog/name-guard tests pass. Installers still enable only English; full locale activation, translated page layout and current package lifecycle are pending. See second-pass-installer-driver-checkbox.json.

### Shared installer caption export

windows_installer_catalogs.py exports the reviewed heading, setup subtitle and driver checkbox for every catalog locale and both routes. Use --product with SoundCurrent EQ or SoundCurrent Studio and --output with a destination JSON file. Captions and NSIS-escaped values are separate fields. Name substitution is single-pass; unsupported placeholder forms and control characters fail. Export requires finished, validated catalogs before writing. All 204 captions compile in a syntax-only fixture using the English language ID; this is not locale activation, layout or package qualification. Production packaging integration is described below; remaining text and language selection are pending. See second-pass-installer-caption-export.json.

### Installer build integration

The Windows build script now invokes windows_installer_catalogs.py to produce installer-translations.json and a separate localized-installer.nsi under build-windows-native, then compiles that generated file. The generator validates product/schema, preserves source files and rejects missing/duplicate caption definitions or unplanned language activation. Current English build copies match source bytes for all four installer variants. Host PowerShell parsing and CLI generation passed; full Windows packaging execution, language selection and installed UI/lifecycle qualification remain pending. See second-pass-installer-build-integration.json.

Installer driver-check failure guidance is translated across all 33 non-English locales. It inserts the translated Audio driver setup caption and retains the app/Start-menu retry paths. Both installer variants reference SCDriverCheckFailed; production caption generation includes it. Catalog tests, generated-English source preservation and a 272-caption syntax fixture passed. The fixture uses English language IDs and does not test locale selection or layout. Current full installer builds, locale activation and UI/lifecycle qualification remain pending. See second-pass-installer-driver-check.json.

The cable installer restart notice is translated across all 33 non-English catalogs and exported only for the cable route. It preserves the requirement to restart Windows before using the equalizer or opening cable settings, with exact VB-CABLE name guards. Generated-English preservation tests and a 306-caption syntax-only fixture passed. Language activation, layout, full current packages and installed lifecycle tests remain pending. See second-pass-installer-reboot.json.

The native installer shared-driver notice now asks users to quit any running SoundCurrent app rather than assuming both products are running. All 33 non-English catalogs retain the conditional driver-retention advice when removing one app. Export is native-only; driver ownership/uninstall behavior is unchanged. Catalog tests, source-preserving build generation and a 340-caption syntax fixture passed. Current installer language activation, real UI layout and package lifecycle remain pending. See second-pass-installer-shared-driver.json.

The native installer administrator-approval notice is translated across all 33 non-English catalogs. Context review identifies the driver manager as a signed program and retains the conditional restart advice; several software-role translations were refined. Native-only export and generated-source preservation tests passed, along with a 374-caption syntax-only fixture. No elevation behavior, real locale selection, installed layout or current package lifecycle is qualified by these checks. See second-pass-installer-native-approval.json.

The native existing-driver notice now makes app registration conditional on keeping driver setup enabled, matching the installer’s checkbox-gated manager invocation. All 33 non-English catalogs retain that condition, driver availability for the other app and exact product names. Defaults and ownership behavior are unchanged. Catalog tests, generated-source preservation and a 408-caption syntax fixture passed. Real registration, language activation, layout and current package lifecycle remain unqualified. See second-pass-installer-native-present.json.

The native installer routing explanation is translated across all 33 non-English catalogs. It distinguishes playback through the app, physical speaker/headphone selection inside the app and preserved hardware drivers. Name guards, native-only export, source-preserving generation and a 442-caption syntax fixture passed. This does not qualify real routing, language selection, layout or current package lifecycle. See second-pass-installer-native-routing.json.

### Windows metadata encoding failure

Studio Windows CI run 37861227271 failed while reading UTF-8 catalog metadata as CP1252. Both shared exporter copies now read setup source and catalog metadata explicitly as UTF-8. Multilingual test fixture and runner JSON I/O is explicit too. A simulated-CP1252 regression reproduces the legacy failure before output and verifies the fixed exporter across all 34 locales with exact Arabic caption preservation. Fresh Windows CI and current package qualification remain pending. The EQ Windows job also failed, but its detailed log was unavailable; the matching cause is not independently confirmed. See second-pass-windows-codepage.json.

Installer build-copy generation now uses UTF-8 byte I/O and preserves each matched definition’s carriage return. A retained failure demonstrates the prior CRLF-to-LF conversion. Current-English fixtures preserve LF, CRLF and mixed endings byte-for-byte across all four installer variants, independent of host text-mode defaults. This host regression does not qualify a Windows build or installed package. See second-pass-installer-source-endings.json.

Studio Windows CI run 37861749452 passed catalog export, then failed the Qt/PowerShell output fixture without useful detail. The inert fixture now reports bounded stdout/stderr hexadecimal bytes and process status, and returns explicit errors. Host success, failed-start and nonzero-child cases pass. No production audio code changed, and the Windows failure’s root cause remains unresolved pending fresh CI evidence. See second-pass-process-output-diagnostics.json.

The Windows output fixture previously ran from the build directory before Qt/CRT deployment, while the verified SDK installer did not set Qt bin on PATH. It now runs from the package stage after app-local dependencies are deployed, reports numeric exit codes and is removed on success before packaging. Both build scripts parse on the host. Dependency ordering is a plausible cause of the silent Windows failure, but the exact loader exit code and successful Windows rerun remain unverified. See second-pass-staged-output-fixture.json.

The existing-cable notice now uses the actual translated Quit app button label; a missing Quit key was rejected during initial validation. All 33 non-English catalogs retain reuse and output-restoration guidance, with several off-state descriptions clarified. Source/name/placeholder checks and a 476-caption syntax fixture passed. Separately, Studio Windows run 37862439944 passed the staged output fixture and reached UI tests, then failed the German owned-validation check. This Windows evidence applies to its recorded commit, not the new caption batch. Remaining activation, layout and package lifecycle gates remain incomplete.

### Incomplete cable driver repair notice

The installer repair notice now has catalog translations in all 33 non-English locales. Review preserves the remove/restart/reinstall/restart sequence and protects VB-CABLE. Catalog tests and all 510 exported NSIS caption literals pass; the compiler fixture uses English language IDs and does not prove language activation or installed layout. Native-speaker review remains unverified. See `tests/results/localization/second-pass-installer-cable-repair.json`.

### Signed cable installer instructions

All 33 non-English catalogs now contain the signed-installer notice. The exact external button caption `Install Driver` and the required Windows restart remain intact. Catalog tests and 544 exported caption literals pass syntax checks. This does not qualify installer locale activation, translated layout, or a current installed package. Native-speaker verification is unverified. See `tests/results/localization/second-pass-installer-signed-notice.json`.

### Windows Studio route validation qualification

Windows CI run 37863575451 passed for Studio commit `6a26206`, including 35 localized UI assertions and installer compilation. It predates the newer repair and signed-installer notices and does not test installed-package lifecycle. Test diagnostic logs now preserve exact Unicode as UTF-8 hex; actual dialog assertions remain unchanged. EQ now uses the same relevant PR Windows-check trigger as Studio. See `tests/results/localization/second-pass-windows-route-success.json`.

### Shared cable ownership notice

All 33 non-English catalogs now contain the shared-cable notice. The English caption explicitly identifies the last SoundCurrent app, and contextual review checked optional removal, other software dependencies, extra A/B virtual cables, and quitting background processing. Catalog tests and 578 exported caption literals compile. Native-speaker review, installer language activation, installed layout and current package lifecycle remain unverified. See `tests/results/localization/second-pass-installer-shared-cable.json`.

### Cable routing caption and Windows Unicode dialog evidence

All custom audio-page control captions now use catalog-backed LangStrings, with all 33 non-English translations prepared. A new guard rejects raw control captions. Other welcome, progress and error text remains incomplete; installer locale activation is pending. All 612 exported caption literals compile in an English-ID syntax fixture. Studio Windows run 37864139375 passed at commit `dde32e7`; six actual dialog captures decode as strict UTF-8, including French “Redémarrez”. That run predates the shared/routing caption work and does not verify installed-package lifecycle. See `second-pass-installer-cable-routing.json` and `second-pass-windows-dialog-utf8.json` under `tests/results/localization/`.

### Quit before update and uninstall dialogs

Both installer routes now use catalog translations for the quit-before-update and quit-before-uninstall messages in all 33 non-English locales. Review preserves full process exit, closing-window/background behavior and updates without prior uninstall. A shared single-value formatter rejects malformed placeholders and preserves inserted names literally; existing retry and Quit app captions now use it too. Catalog tests, final exporter tests and 748 caption syntax checks pass. Installer locale activation and installed-package lifecycle qualification remain pending. See `tests/results/localization/second-pass-installer-quit-dialogs.json`.

### Installer progress and retry captions

Three shared catalog sources now translate native driver setup progress, cable installer launch, and retry progress across all 33 non-English locales. Review preserves route names, shared ownership and failure/restart distinctions. All static DetailPrint caption sites use LangStrings; dynamic helper output is preserved. Catalog tests and 884 exported caption literals pass syntax checks. Installer locale activation, current installed UI and package lifecycle remain pending. See `tests/results/localization/second-pass-installer-progress.json`.

### Native driver removal error

The removal failure notice now asks users to quit any running SoundCurrent app, instead of both EQ and Studio unconditionally. All 33 non-English translations are prepared and contextually reviewed; native-speaker review remains unverified. The uninstall failure still aborts before payload deletion and keeps the app for retry. Catalog tests and 918 caption syntax checks pass; native installed-package runtime and locale activation remain unqualified. See `tests/results/localization/second-pass-installer-native-removal.json`.

### Cable removal failure and helper details

The owned cable-removal explanation now has translations in all 33 non-English locales. The caller retains the newline and `$1` helper-output suffix, and the failure still aborts uninstall so users can retry. The NSIS audit accepts only the reviewed suffix after a real LangString; extra prose/variables and missing definitions reject. Catalog tests, 952 caption literals and 34 composite message syntax checks pass. This does not qualify installed-uninstaller behavior or locale activation. See `tests/results/localization/second-pass-installer-cable-removal.json`.

### Restart-dialog reuse and rebuilt Linux UI checks

The installer restart dialog now reuses the existing all-locale restart notice. Exit-code handling, reboot flag and silent default are preserved. Both Linux apps were rebuilt with current catalogs: EQ passed 40 selected checks and Studio passed 43, including all 33 non-English UI locales, two layout-test locales, regional-format and UTF-8 diagnostics. These are offscreen widget assertions, not installed desktop or package lifecycle qualification. The restart reference audit and 34 translated message syntax checks also pass. See `tests/results/localization/second-pass-restart-reuse-linux-ui.json`.

### Setup failure after app installation

The shared dialog now reports setup failure without falsely claiming an existing native driver is absent. All 33 non-English translations preserve the installed app, Start-menu retry and details for the reason. A single-pass formatter validates three distinct parameters. The dialog quotes the actual currently English shortcut name `Audio driver setup`; shortcut localization and migration cleanup remain a known interface gap. All supported NSIS MessageBox sites now reference declared LangStrings. Catalog tests, 1020 caption literals and 68 setup-failure message syntax checks pass. These do not qualify locale activation or installed-package lifecycle. See `tests/results/localization/second-pass-installer-setup-failure.json`.

### Installer welcome paragraphs

Both welcome paragraphs now have translations in all 33 non-English locales. Review preserves update without prior uninstall, retained settings/presets/equipment profiles, saving work and full exit/background behavior. Generic quit wording avoids an incorrect button caption. Arabic grammar, Romanian wording and Swahili preset terminology were refined. Two catalog sources are joined with an escaped CRLF paragraph break after app-name substitution. Catalog tests and 1088 caption literals, including 68 welcome texts, compile with the actual MUI welcome macro. All supported non-Section NSIS text sites now reference LangStrings; whole-interface coverage, language activation, shortcut names and installed layout/lifecycle remain unqualified. See `tests/results/localization/second-pass-installer-welcome.json`.

### Repair-helper notice second pass

The long pre-repair modal is translated in all 33 non-English locales. Its app action uses the localized Audio driver setup caption; external endpoint and installer labels remain exact. The actual modal expression passed inert argument capture in all 34 catalogs, and 1,190 helper lookups plus safe fault and mutation checks passed. No driver actions or VM session were used. This is formatting evidence, not installed dialog layout or native-language qualification. See `tests/results/localization/second-pass-helper-repair.json`.

### Explicit installer locale identities

`installer-language-map.json` covers all 34 catalog tags with unique NSIS names and Windows language IDs. Validation checks catalog coverage, duplicate identities, RTL direction, and actual installed NLF/MUI asset pairs. On the Linux NSIS installation, 33 pairs match; Swahili has no built-in pair and requires owned installer translations. The mapping is validated during caption export and has a CTest regression gate. Installer locale activation remains incomplete; this mapping does not enable language selection or qualify rendered installer pages.

### Swahili installer assets

Owned `packaging/windows/languages/Swahili.nlf` and `Swahili.nsh` provide 89 base strings and 62 Modern UI declarations, including both conditional component-description variants. Altered NSIS source provenance and its zlib notice are retained alongside review inventories. Token, quote and whitespace checks passed; Unicode base and full MUI install/uninstall fixtures compiled for both component-page variants. Fixtures were never executed. The locale-map validator resolves the missing built-in Swahili pair to these owned files and checks their Windows ID and direction. Native-speaker verification, rendered UI qualification, and installer language-selection activation remain incomplete. See `tests/results/localization/swahili-installer-assets.json`.
