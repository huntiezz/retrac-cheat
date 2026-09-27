# Builds Retrac.sln (Release | x64). Used locally and in GitHub Actions.
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$sln = Join-Path $repoRoot 'Retrac.sln'
if (-not (Test-Path $sln)) {
    throw "Solution not found: $sln"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere not found. Install Visual Studio 2022 with Desktop development with C++."
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
    Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild not found. Install the MSBuild component via Visual Studio Installer.'
}

Write-Host "MSBuild: $msbuild"
Write-Host "Building: $sln (Release|x64)"

& $msbuild $sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Join-Path $repoRoot 'bin\x64\Release\Retrac.exe'
if (Test-Path $exe) {
    Write-Host "OK: $exe"
} else {
    Write-Warning "Build finished but Retrac.exe not found at $exe"
}
