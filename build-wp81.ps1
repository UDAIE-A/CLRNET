param(
    [ValidateSet("Build", "Clean", "Rebuild", "Package")]
    [string]$Target = "Build",

    [ValidateSet("ARM", "x86")]
    [string]$Platform = "ARM",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$Package
)

function Write-Status {
    param([string]$Message)
    $timestamp = Get-Date -Format "HH:mm:ss"
    Write-Host "[$timestamp] $Message"
}

function Assert-Windows {
    if ($env:OS -notlike '*Windows*' -and [System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) {
        throw "The Windows Phone 8.1 toolchain is only available on Windows."
    }
}

function Resolve-MSBuild {
    $knownPaths = @(
        (Join-Path $env:ProgramFiles(x86) 'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles(x86) 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles(x86) 'MSBuild\14.0\Bin\MSBuild.exe')
    )

    foreach ($path in $knownPaths) {
        if (Test-Path $path) {
            return $path
        }
    }

    $vswhere = Join-Path $env:ProgramFiles(x86) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -format value -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $fromPath = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Path
    }

    throw "Could not locate MSBuild. Install Visual Studio 2019/2022 with the Windows Phone 8.1 tools."
}

function Test-WP81SDK {
    $sdkPath = Join-Path $env:ProgramFiles(x86) 'Windows Phone Kits\8.1'
    if (-not (Test-Path $sdkPath)) {
        throw "Windows Phone 8.1 SDK not found. Install the Windows Phone 8.1 tooling from the Windows 10 SDK installer."
    }
}

Assert-Windows
$msbuild = Resolve-MSBuild
Test-WP81SDK

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = Join-Path $repoRoot 'CLRNet.proj'
$logRoot = Join-Path $repoRoot 'build\logs'
if (-not (Test-Path $logRoot)) {
    New-Item -ItemType Directory -Path $logRoot | Out-Null
}

$msbuildArgs = @(
    "`"$proj`"",
    "/m",
    "/nr:false",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)

switch ($Target) {
    'Clean' { $msbuildArgs += '/t:Clean' }
    'Build' { $msbuildArgs += '/t:Build' }
    'Rebuild' { $msbuildArgs += '/t:Rebuild' }
    'Package' { $msbuildArgs += '/t:Package' }
}

$binlog = Join-Path $logRoot "wp81-$Platform-$Configuration.binlog"
$msbuildArgs += "/bl:$binlog"

Write-Status "Invoking MSBuild ($Target | $Platform | $Configuration)"
Write-Status "Using MSBuild at $msbuild"
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

if ($Package -and $Target -ne 'Package') {
    Write-Status "Packaging runtime artifacts"
    & $msbuild "`"$proj`"" /t:Package /p:Configuration=$Configuration /p:Platform=$Platform /bl:"$binlog.package"
    if ($LASTEXITCODE -ne 0) {
        throw "Packaging failed with exit code $LASTEXITCODE"
    }
}

Write-Status "Build complete. Binaries in build\\bin\\$Platform\\$Configuration"
Write-Status "Packages in build\\bin\\$Platform\\$Configuration\\packages"
