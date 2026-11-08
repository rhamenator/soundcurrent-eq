# Post-reboot volume persistence correction

The first real Windows reboot preserved the Software DWORD but reset live VB-CABLE volume control. The live probe returned 12, and 50% volume measured 0.00 dB attenuation instead of -10.51 dB. Studio's live-editing state and Realtek enhancements-Off preference survived.

The vendor control panel saves the volume DWORD to BOTH HKLM\SOFTWARE\VB-Audio\Cable and HKLM\SYSTEM\CurrentControlSet\Services\VB-Cable. The installed signed 3.3.1.7 driver queries the Software location and then the SYSTEM fallback. The helper omitted the fallback. It now writes, flushes and verifies both fixed values, migrates Software-only state, then applies live control. The fixture covers migration, missing/disabled SYSTEM state, write failures and live/pending behavior. No arbitrary registry path or service-start modification was added.

The administrator-approved helper completed with exit 0; both saved values now read 1. Repaired EQ transport passed Windows volume and real media-key mute/unmute/volume-down. At 50%, attenuation measured -10.10 dB (endpoint reported -10.51); mute RMS was 0/0. Post gain, EQ, bypass, balance, stream restart, Bass/Clarity and reset also passed. Audio roles/levels/mutes were restored. Both installed readiness helpers returned 0. See Studio PR #23 for compact mode controls and the shared post-reboot report.

A second full Windows reboot has not occurred since this correction. Revised boot persistence is pending that test; current live operation passes. Secure Boot remains enabled, native test driver untouched, original installers/files preserved, quarantined rollback script not restored.
