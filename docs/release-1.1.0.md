# SoundCurrent EQ 1.1.0

## Localization and regional settings

- Choose interface language separately from regional number/date formatting.
- Embedded English plus 32 partial, unverified language packs; missing messages fall back to English. Each language pack currently translates 30 core strings. These are not complete or native-reviewed editions.
- Support right-to-left interfaces while preserving frequency/channel order and readable technical numeric labels.
- Keep preset IDs, device identifiers and saved processing values independent of translated display text.
- Add translator context for audio terminology and structural checks for catalog freshness, placeholders, literal ampersands and hidden direction overrides. Correct terminology issues found during the contextual AI review; fluent-speaker review remains required.

## Installing or updating

Install over the existing application; uninstalling first is unnecessary. Quit the running application before installing and reopen afterward. Select language/region on Settings & calibration and Quit/reopen to apply. Windows uses the existing VB-CABLE route; native driver signing remains separate future work. Installer/vendor text remains English.

Linux: use the downloadable Linux installer or the DEB/Fedora 44/RHEL 10-compatible RPM for your distribution. Windows: use the x64 setup installer. SHA256SUMS and corresponding GPL source are included with the release.

This release concerns the EQ and Studio equalizer applications, not the separately developed DAW. The 1.0.0 release remains available.
