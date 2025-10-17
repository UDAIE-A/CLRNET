param(
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Release",

    [Parameter(Mandatory=$false)]
    [string]$Platform = "ARM"
)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildScript = Join-Path (Split-Path -Parent $scriptRoot) "build.ps1"

if (!(Test-Path $buildScript)) {
    Write-Host "[ERROR] build.ps1 not found at $buildScript" -ForegroundColor Red
    exit 1
}

& $buildScript -Target PackageOverlay -Configuration $Configuration -Platform $Platform
exit $LASTEXITCODE
