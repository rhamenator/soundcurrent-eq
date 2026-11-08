# Windows route release parity

VB-CABLE and native-driver installers remain separate artifacts. Cable keeps the established `SoundCurrent-EQ-<version>-windows-x64-setup.exe` name; native uses `SoundCurrent-EQ-<version>-windows-x64-native-setup.exe`, so neither can overwrite the other.

Every Windows build emits a route-specific manifest with its exact Git commit, version, installer hash, and hashes of the common application payload. Before publishing either route, run `scripts/verify-windows-variant-parity.py` with both manifests. The gate rejects differing versions, commits, application payloads, or colliding installer names.

This makes native cutover an artifact/signing decision rather than a separate application branch. A native package still requires Microsoft production signing and its independent release qualification; manifest parity does not substitute for driver runtime testing.
