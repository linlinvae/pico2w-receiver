# SPDX-License-Identifier: Apache-2.0
# Repository-only checks: no SDK downloads, builds, flashing or device access.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$gitRoot = $root.Replace('\', '/')
$files = @(& git -c "safe.directory=$gitRoot" -C $root -c core.quotepath=false ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate repository files' }
$published = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in $files) { [void]$published.Add($file.Replace('\', '/')) }

$manifestFiles = @(Get-ChildItem -LiteralPath "$root/pico2w/preserved" -Filter '*.json' -File)
$manifestFiles += Get-Item -LiteralPath "$root/switchpro/preserved/receiver-manifest.json"
$hashCount = 0
foreach ($manifest in $manifestFiles) {
    $base = "$root/pico2w"
    if ($manifest.Name -in @('previous-firmware.json', 'receiver-manifest.json')) { $base = $root }
    foreach ($record in (Get-Content -LiteralPath $manifest.FullName -Raw -Encoding UTF8 | ConvertFrom-Json)) {
        $path = Join-Path $base $record.path
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing preserved file: $($record.path)" }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $record.sha256) {
            throw "Preserved hash mismatch: $($record.path)"
        }
        $hashCount++
    }
}
Write-Output "Preservation: $hashCount entries across $($manifestFiles.Count) manifests verified."

$docs = @('README.md', 'CONTRIBUTING.md', 'NOTICE.md', 'docs/releases.md', 'docs/validation/2026-10-22-publication.md',
    'pico2w/README.md', 'pico2w/host/modern/README.md')
$linkCount = 0
foreach ($doc in $docs) {
    $path = Join-Path $root $doc
    $content = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($content, '\[[^\]\r\n]+\]\(([^)\s]+)\)')) {
        $target = $match.Groups[1].Value
        if ($target -match '^[a-zA-Z][a-zA-Z0-9+.-]*:' -or $target.StartsWith('#')) { continue }
        $target = [Uri]::UnescapeDataString(($target -split '[?#]', 2)[0])
        $resolved = [IO.Path]::GetFullPath((Join-Path (Split-Path $path -Parent) $target))
        $prefix = $root + [IO.Path]::DirectorySeparatorChar
        if (!$resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Link outside repository: $doc -> $target" }
        if (!(Test-Path -LiteralPath $resolved)) { throw "Broken link: $doc -> $target" }
        $relative = $resolved.Substring($prefix.Length).Replace('\', '/')
        if (Test-Path -LiteralPath $resolved -PathType Container) {
            $directoryPrefix = $relative.TrimEnd('/') + '/'
            if (!@($files | Where-Object { $_.StartsWith($directoryPrefix, [StringComparison]::OrdinalIgnoreCase) }).Count) {
                throw "Linked directory is not published: $doc -> $target"
            }
        } elseif (!$published.Contains($relative)) { throw "Linked file is not published: $doc -> $target" }
        $linkCount++
    }
}
Write-Output "Documentation: $linkCount relative links resolve to publishable files/directories."

$downloads = Get-Content -LiteralPath "$root/docs/releases.md" -Raw -Encoding UTF8
$downloadCount = 0
foreach ($match in [regex]::Matches($downloads, '\| \[[^\]]+\]\(\.\./([^\)]+)\) \| `([A-Fa-f0-9]{64})` \|')) {
    $path = Join-Path $root $match.Groups[1].Value
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $match.Groups[2].Value) {
        throw "Download hash mismatch: $($match.Groups[1].Value)"
    }
    $downloadCount++
}
if ($downloadCount -ne 10) { throw "Expected 10 documented download hashes; found $downloadCount" }
Write-Output "Downloads: $downloadCount documented SHA256 values verified."

foreach ($file in $files) {
    if ($file -match '(^|/)(resources|sdk|node_modules|__pycache__|build|package-stage)/' -or
        $file -match '(^|/)\.env($|\.(?!example$))' -or $file -match '\.(pem|key)$') {
        throw "Local dependency, cache or credential file would be published: $file"
    }
}
Write-Output 'Repository checks passed. Hardware behavior is not checked by this script.'
