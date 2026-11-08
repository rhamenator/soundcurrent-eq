# SoundCurrent EQ 1.3.1

- Restore Windows media-key volume and mute behavior while the VB-CABLE route is active. Installation, repair, and app updates now persist VB-CABLE's Windows Volume Control option and apply it to the running driver immediately when administrator approval is available.
- Reuse an existing VB-CABLE installation without silently reinstalling or taking over unrelated vendor settings. Silent updates never request elevation; they report a deferred configuration result instead.
- Keep SoundCurrent's native Windows driver path separate and unchanged. This compatibility fix is only for the interim VB-CABLE backend.
- Improve setup diagnostics for cancelled elevation, denied registry access, failed live-driver verification, and the reboot boundary after a new driver install.

## Validation

The exact Windows CI artifact was exercised in an independent disposable Windows 11 VM with the signed VB-CABLE driver. At a 50% endpoint setting, measured attenuation was -10.10 dB while Windows reported -10.51 dB. Mute produced silence; live +6 dB gain, -12 dB EQ, bypass, full-left balance, route restoration, and bridge restart all passed. Fresh install, reboot persistence, existing-driver reuse, coexistence, and app-only uninstall behavior were also checked.

Both repositories run pinned CodeQL analysis for Linux and the Windows driver-manager surface, repository policy/secret checks, dependency review, and Socket security reporting. The fixed-purpose setup helper accepts no arbitrary registry path, value, device, or command.

## Upgrade

Use **Quit**, then install 1.3.1 over the existing application. Settings and custom profiles are retained. A newly installed VB-CABLE driver still requires the Windows reboot requested by setup. After reboot, repairing or starting SoundCurrent can apply the volume-control option without a driver reinstall.

Linux behavior and packages are unchanged apart from the version update. Windows includes both the VB-CABLE installer and the preserved native-driver variant; native-driver installation remains unavailable unless Windows can verify an eligible signed package.
