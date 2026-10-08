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
