# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
# ASCII source works under Windows PowerShell 5.1 without a source-file BOM.
$text = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('UsOpcGFyYXRpb24gLyDml6XmnKzoqp4gLyDYp9mE2LnYsdio2YrYqSAvICUxIC8g6Z+z5aOw'))
Write-Output $text
