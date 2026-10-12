param([ValidateSet('CH582','CH592','all')][string]$Chip = 'CH582')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$dependencies = Get-Content "$projectRoot/dependencies.lock.json" -Raw | ConvertFrom-Json
function Invoke-GitChecked {
    param([string[]]$Arguments)
    & git @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Git dependency setup failed.' }
}
$dependencyNames = @()
if ($Chip -eq 'CH592' -or $Chip -eq 'all') { $dependencyNames += 'sdk' }
if ($Chip -eq 'CH582' -or $Chip -eq 'all') { $dependencyNames += 'sdk_ch582' }
$dependencyNames += 'toolchain'
foreach ($name in $dependencyNames) {
    $dependency = $dependencies.$name
    $target = Join-Path $projectRoot $dependency.path
    if (!(Test-Path -LiteralPath $target)) {
        Invoke-GitChecked -Arguments @('clone','--depth','1','--filter=blob:none','--sparse','--no-checkout',$dependency.url,$target)
    } else {
        if (!(Test-Path -LiteralPath "$target/.git")) { throw "Existing dependency is not a Git checkout: $target" }
        $remote = & git -C $target remote get-url origin
        if ($LASTEXITCODE -ne 0 -or $remote -ne $dependency.url) { throw "Unexpected dependency origin: $target" }
        $changes = & git -C $target status --porcelain
        if ($LASTEXITCODE -ne 0 -or $changes) { throw "Dependency contains local changes: $target" }
    }
    Invoke-GitChecked -Arguments @('-C',$target,'fetch','--depth','1','origin',$dependency.commit)
    Invoke-GitChecked -Arguments (@('-C',$target,'sparse-checkout','set') + $dependency.directories)
    Invoke-GitChecked -Arguments @('-C',$target,'checkout','--detach',$dependency.commit)
}
Write-Host "Pinned $Chip SDK and WCH Windows RISC-V toolchain are ready."
