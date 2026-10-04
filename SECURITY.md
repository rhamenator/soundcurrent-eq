# Security

SoundCurrent EQ runs as the signed-in user. It does not need root privileges to
run, does not listen on a network port, and does not send telemetry. It calls
`pipewire`, `pactl`, `pw-dump`, and `pw-cli` with argument arrays, without a shell.
Its temporary PipeWire configuration lives in a private temporary directory.
Custom presets are stored in the user's configuration directory with owner-only
file permissions. A local Unix socket in the user's private runtime directory
lets a second launch reopen the existing window; the socket accepts connections
from the same user only.

Please report suspected vulnerabilities privately using GitHub's **Report a
vulnerability** button under the repository's Security tab. Include the Ubuntu
version, PipeWire version, and steps to reproduce. Do not post an exploit in a
public issue before a fix is available.

Supported release line: the latest `0.3.x` release. Security fixes will be
published as a new release with an updated `.deb` and SHA-256 checksum.
