#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' 'Windows builds now use the shared Qt interface.' 'On Windows, run scripts/install-windows-qt.ps1, then scripts/build-windows.ps1 -QtPrefix C:\Qt\6.12.0\msvc2022_64.' 'The Windows GitHub Actions workflow builds and packages these automatically.' >&2
exit 1
