# FxSound Windows routing comparison

Checked 2026-10-06 against the publisher's repositories and routing guide.

FxSound publishes a Windows virtual audio driver based on Microsoft's virtual
sample. It routes system playback through the FxSound endpoint and sends processed
audio to the app-selected physical endpoint using that endpoint's hardware driver.
The publisher's guide says Windows output selection can revert to FxSound while
processing is active. It is therefore not evidence of cable-free processing directly
attached to a native endpoint. A SoundCurrent virtual driver could integrate setup,
device selection and removal more neatly than a separate VB-CABLE installation,
but would retain the extra endpoint and still require a signing/deployment route.

Decision: retain the native APO direction requested by the owner. Incorporate the
useful integrated-management behavior: chosen-device status, direct sound settings,
explicit enable/disable and removal/rollback. Do not change default endpoints or
silently enroll every device. FxSound's virtual backend is a possible separately
chosen future route, not a replacement for the native-driver requirement.

The driver repository identifies AGPL-3.0 and a Microsoft sample license, and the
application identifies AGPL-3.0. No source was imported. Reuse would require a
separate license review; open publication is not unrestricted permission to copy
or relicense code into these GPL-3.0-only projects.

Sources:
- https://github.com/fxsound2/fxsound-driver
- https://github.com/fxsound2/fxsound-app
- https://forum.fxsound.com/t/how-playback-output-device-works-when-using-fxsound/6331
