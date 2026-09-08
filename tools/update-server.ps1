<#
.SYNOPSIS
    Updates a GTA:Orange Windows server installation in place from the GitHub releases.
.DESCRIPTION
    Replaces orange_server.exe, modules\lua-module.dll, modules\lua-module\API.lua,
    the examples and this script; config.yml and your resources\ are left untouched.
    Restart the server afterwards.
.EXAMPLE
    .\update-server.ps1                 # latest release
    .\update-server.ps1 -Channel nightly
    .\update-server.ps1 -Force
#>
param(
    [string]$Dir = $PSScriptRoot,
    [ValidateSet("stable", "nightly")]
    [string]$Channel = "stable",
    [string]$Repository = "SashaGoncharov19/GTA-Orange-Reloaded",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if ($Channel -eq "nightly") {
    $base = "https://github.com/$Repository/releases/download/nightly"
} else {
    $base = "https://github.com/$Repository/releases/latest/download"
}

Write-Host "[update] channel: $Channel ($base)"
$remote = (Invoke-WebRequest -UseBasicParsing "$base/server-version.txt").Content.Trim()
$local = if (Test-Path "$Dir\version.txt") { (Get-Content "$Dir\version.txt" -Raw).Trim() } else { "unknown" }
Write-Host "[update] installed: $local, available: $remote"

if ($remote -eq $local -and -not $Force) {
    Write-Host "[update] already up to date"
    exit 0
}

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("gta-orange-update-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    Write-Host "[update] downloading gta-orange-server-win64.zip"
    Invoke-WebRequest -UseBasicParsing "$base/gta-orange-server-win64.zip" -OutFile "$tmp\server.zip"
    Expand-Archive -Path "$tmp\server.zip" -DestinationPath $tmp -Force
    $src = Join-Path $tmp "server"

    New-Item -ItemType Directory -Force -Path "$Dir\modules\lua-module", "$Dir\resources" | Out-Null
    Copy-Item "$src\orange_server.exe" "$Dir\orange_server.exe" -Force
    Copy-Item "$src\modules\lua-module.dll" "$Dir\modules\lua-module.dll" -Force
    Copy-Item "$src\modules\lua-module\API.lua" "$Dir\modules\lua-module\API.lua" -Force
    if (Test-Path "$Dir\examples") { Remove-Item "$Dir\examples" -Recurse -Force }
    Copy-Item "$src\examples" "$Dir\examples" -Recurse -Force
    if (-not (Test-Path "$Dir\config.yml")) { Copy-Item "$src\config.yml" "$Dir\config.yml" }
    if (-not (Test-Path "$Dir\resources\example")) { Copy-Item "$src\resources\example" "$Dir\resources\example" -Recurse }
    Copy-Item "$src\version.txt" "$Dir\version.txt" -Force
    if (Test-Path "$src\update-server.ps1") { Copy-Item "$src\update-server.ps1" "$Dir\update-server.ps1" -Force }

    Write-Host "[update] updated $local -> $remote. Restart the server to apply."
} finally {
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
