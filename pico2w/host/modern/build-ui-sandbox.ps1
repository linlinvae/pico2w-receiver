# SPDX-License-Identifier: Apache-2.0
# Compile JS with the same locked esbuild executable directly, without Node
# spawning a service. Keep the already compiled CSS; refuse to run without it.
$ErrorActionPreference='Stop'
$ui=Join-Path $PSScriptRoot 'ui'
Push-Location $ui
try {
    $htmlPath=Join-Path $ui 'dist/index.html'
    $html=Get-Content -LiteralPath $htmlPath -Raw
    if($html -notmatch 'href="\.\/assets\/(.+\.css)"'){throw 'Run the full Vite build first to generate CSS'}
    if(!(Test-Path -LiteralPath (Join-Path $ui ('dist/assets/'+$Matches[1])))){throw 'Compiled CSS missing'}
    & node (Join-Path $ui 'node_modules/typescript/bin/tsc') --noEmit
    if($LASTEXITCODE -ne 0){throw 'TypeScript check failed'}
    $entry=Join-Path $ui '.build-entry.tsx'
    $source=(Get-Content -LiteralPath (Join-Path $ui 'src/main.tsx') -Raw).Replace("import './index.css'",'')
    # Resolve App relative to this temporary entry, retaining the original code.
    $source=$source.Replace("from './App'","from './src/App'")
    [IO.File]::WriteAllText($entry,$source,(New-Object Text.UTF8Encoding($false)))
    $esbuild=Get-ChildItem -LiteralPath (Join-Path $ui 'node_modules/.pnpm') -Filter esbuild.exe -Recurse | Select-Object -First 1 -ExpandProperty FullName
    if(!$esbuild){throw 'Locked esbuild executable missing'}
    $temp=Join-Path $ui 'dist/assets/desktop-temp.js'
    $flags=@('--bundle','--minify','--format=esm','--platform=browser','--target=es2022','--jsx=automatic','--define:process.env.NODE_ENV="production"',('--outfile='+$temp))
    & $esbuild $entry @flags
    if($LASTEXITCODE -ne 0){throw 'Direct JS build failed'}
    $hash=(Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash.Substring(0,12).ToLowerInvariant()
    $name='desktop-'+$hash+'.js'
    Copy-Item -LiteralPath $temp -Destination (Join-Path $ui ('dist/assets/'+$name)) -Force
    $html=[regex]::Replace($html,'src="\.\/assets\/[^"\s]+\.js"',('src="./assets/'+$name+'"'))
    [IO.File]::WriteAllText($htmlPath,$html,(New-Object Text.UTF8Encoding($false)))
    Write-Output ('Direct locked esbuild build completed: '+$name)
} finally {Pop-Location}
