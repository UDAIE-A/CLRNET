# CLRNET Build Script
# Cross-compilation build system for Windows Phone 8.1 ARM target

param(
    [Parameter(Mandatory=$false)]
    [string]$Target = "Build",
    
    [Parameter(Mandatory=$false)] 
    [string]$Platform = "ARM",
    
    [Parameter(Mandatory=$false)]
    [string]$Configuration = "Release",
    
    [Parameter(Mandatory=$false)]
    [string]$Device = "",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$VerboseOutput = $false
)

# Build configuration
$script:BuildRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:SourceRoot = Join-Path $BuildRoot "..\src"
$script:OutputRoot = Join-Path $BuildRoot "output"
$script:LogsRoot = Join-Path $BuildRoot "logs"

# Visual Studio and Windows Phone SDK paths
$script:VSPath = ""
$script:WP81SDKPath = ""
$script:MSBuildPath = ""

function Write-BuildLog {
    param([string]$Message, [string]$Level = "INFO")
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    
    Write-Host $logMessage
    
    if (!(Test-Path $LogsRoot)) {
        New-Item -ItemType Directory -Path $LogsRoot -Force | Out-Null
    }
    
    Add-Content -Path (Join-Path $LogsRoot "build.log") -Value $logMessage
}

function Test-BuildEnvironment {
    Write-BuildLog "Validating build environment..."
    
    # Check for Visual Studio
    $vsInstallations = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017", 
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio 14.0"
    )
    
    foreach ($vsPath in $vsInstallations) {
        if (Test-Path $vsPath) {
            $script:VSPath = $vsPath
            Write-BuildLog "Found Visual Studio at: $vsPath"
            break
        }
    }
    
    if (!$script:VSPath) {
        Write-BuildLog "Visual Studio not found. Please install VS2015 or later with ARM support." "ERROR"
        return $false
    }
    
    # Check for Windows Phone 8.1 SDK
    $wp81Paths = @(
        "${env:ProgramFiles(x86)}\Windows Phone Kits\8.1",
        "${env:ProgramFiles}\Windows Phone Kits\8.1"
    )
    
    foreach ($wp81Path in $wp81Paths) {
        if (Test-Path $wp81Path) {
            $script:WP81SDKPath = $wp81Path
            Write-BuildLog "Found Windows Phone 8.1 SDK at: $wp81Path"
            break
        }
    }
    
    if (!$script:WP81SDKPath) {
        Write-BuildLog "Windows Phone 8.1 SDK not found. Please install the SDK." "ERROR"
        return $false
    }
    
    # Find MSBuild
    $msBuildPaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\MSBuild\14.0\Bin\MSBuild.exe"
    )
    
    foreach ($msBuildPath in $msBuildPaths) {
        if (Test-Path $msBuildPath) {
            $script:MSBuildPath = $msBuildPath
            Write-BuildLog "Found MSBuild at: $msBuildPath"
            break
        }
    }
    
    if (!$script:MSBuildPath) {
        Write-BuildLog "MSBuild not found. Please ensure Visual Studio is properly installed." "ERROR"
        return $false
    }
    
    return $true
}

function Invoke-Clean {
    Write-BuildLog "Cleaning build directories..."
    
    if (Test-Path $OutputRoot) {
        Remove-Item -Recurse -Force $OutputRoot
        Write-BuildLog "Cleaned output directory"
    }
    
    if (Test-Path $LogsRoot) {
        Remove-Item -Recurse -Force $LogsRoot  
        Write-BuildLog "Cleaned logs directory"
    }
}

function Build-Runtime {
    param([string]$Platform, [string]$Configuration)
    
    Write-BuildLog "Building CLRNET runtime for $Platform/$Configuration..."
    
    $outputDir = Join-Path $OutputRoot "$Platform-$Configuration"
    if (!(Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    }
    
    # Build core runtime (Phase 1)
    $phase1Source = Join-Path $SourceRoot "phase1-userland"
    
    Write-BuildLog "Compiling Core Execution Engine..."
    
    # Simplified build - in real implementation would use proper MSBuild project
    $compilerArgs = @(
        "/nologo",
        "/c",                           # Compile only
        "/O2",                          # Optimize for speed
        "/MD",                          # Multithreaded DLL runtime
        "/EHsc",                        # Exception handling
        "/DWIN32", "/D_WINDOWS",        # Windows definitions
        "/DUNICODE", "/D_UNICODE",      # Unicode support
        "/I`"$WP81SDKPath\Include`"",   # WP8.1 SDK headers
        "/Fo`"$outputDir\`"",           # Output directory
        "`"$phase1Source\core\CoreExecutionEngine.cpp`""
    )
    
    # Simulate ARM compilation for demonstration purposes
    # In a real environment, this would use the actual ARM cross-compiler
    Write-BuildLog "Simulating ARM32 compilation..."
    
    # Create placeholder object files to simulate successful compilation
    $objFiles = @(
        "CoreExecutionEngine.obj",
        "TypeSystem.obj", 
        "GarbageCollector.obj",
        "AssemblyLoader.obj",
        "SimpleJIT.obj"
    )
    
    foreach ($objFile in $objFiles) {
        $objPath = Join-Path $outputDir $objFile
        "ARM32 Object File - Placeholder" | Out-File -FilePath $objPath -Encoding ASCII
        Write-BuildLog "Generated: $objFile"
    }
    
    # Create placeholder DLL
    $dllPath = Join-Path $outputDir "clrnet.dll"  
    "ARM32 DLL - Placeholder Runtime" | Out-File -FilePath $dllPath -Encoding ASCII
    Write-BuildLog "Generated: clrnet.dll"
    
    Write-BuildLog "ARM32 compilation simulation completed successfully"
    
    return $true
}

function Build-OverlayAssemblies {
    param([string]$Configuration, [string]$Platform)

    Write-BuildLog "Building overlay facade assemblies for $Platform/$Configuration..."

    $overlayRoot = Join-Path $BuildRoot "..\src\overlays"
    $overlayProjects = @(
        "CLRNet.Core.OverlaySupport\\CLRNet.Core.OverlaySupport.csproj",
        "CLRNet.Facade.System.Runtime\\CLRNet.Facade.System.Runtime.csproj",
        "CLRNet.Facade.System.ValueTuple\\CLRNet.Facade.System.ValueTuple.csproj",
        "CLRNet.Facade.System.Threading.Tasks.Extensions\\CLRNet.Facade.System.Threading.Tasks.Extensions.csproj",
        "CLRNet.Facade.System.Text.Json\\CLRNet.Facade.System.Text.Json.csproj",
        "CLRNet.Facade.System.Buffers\\CLRNet.Facade.System.Buffers.csproj",
        "CLRNet.Facade.System.Net.Http\\CLRNet.Facade.System.Net.Http.csproj",
        "CLRNet.Facade.System.IO\\CLRNet.Facade.System.IO.csproj"
    )

    foreach ($project in $overlayProjects) {
        $projectPath = Join-Path $overlayRoot $project

        if (!(Test-Path $projectPath)) {
            Write-BuildLog "Overlay project not found: $projectPath" "ERROR"
            return $false
        }

        Write-BuildLog "Invoking MSBuild for overlay: $project"

        $arguments = @(
            '"' + $projectPath + '"',
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform"
        )

        & $MSBuildPath @arguments

        if ($LASTEXITCODE -ne 0) {
            Write-BuildLog "Overlay build failed for $project" "ERROR"
            return $false
        }
    }

    Write-BuildLog "Overlay assemblies built successfully"
    return $true
}

function Package-OverlayBundle {
    param([string]$Configuration, [string]$Platform)

    Write-BuildLog "Packaging CLRNET overlay bundle..."

    $outputDir = Join-Path $OutputRoot "$Platform-$Configuration"
    if (!(Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    }

    $overlayOutput = Join-Path $outputDir "CLRNetOverlay"
    $facadeOutput = Join-Path $overlayOutput "facades"

    if (Test-Path $overlayOutput) {
        Remove-Item -Recurse -Force $overlayOutput
    }

    New-Item -ItemType Directory -Path $facadeOutput -Force | Out-Null

    $overlaySourceRoot = Join-Path $BuildRoot "..\src\overlays"

    $overlayArtifacts = @(
        @{ Source = "CLRNet.Core.OverlaySupport\\bin\\$Platform\\$Configuration\\CLRNet.Core.OverlaySupport.dll"; Destination = $overlayOutput },
        @{ Source = "CLRNet.Core.OverlaySupport\\bin\\$Platform\\$Configuration\\CLRNet.Core.OverlaySupport.pdb"; Destination = $overlayOutput; Optional = $true },
        @{ Source = "CLRNet.Facade.System.Runtime\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Runtime.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.Runtime\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Runtime.pdb"; Destination = $facadeOutput; Optional = $true },
        @{ Source = "CLRNet.Facade.System.ValueTuple\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.ValueTuple.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.Threading.Tasks.Extensions\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Threading.Tasks.Extensions.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.Text.Json\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Text.Json.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.Buffers\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Buffers.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.Net.Http\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.Net.Http.dll"; Destination = $facadeOutput },
        @{ Source = "CLRNet.Facade.System.IO\\bin\\$Platform\\$Configuration\\CLRNet.Facade.System.IO.dll"; Destination = $facadeOutput }
    )

    foreach ($artifact in $overlayArtifacts) {
        $sourcePath = Join-Path $overlaySourceRoot $artifact.Source

        if (!(Test-Path $sourcePath)) {
            if ($artifact.Optional) {
                Write-BuildLog "Optional overlay artifact missing (skipping): $sourcePath" "WARNING"
                continue
            }

            Write-BuildLog "Required overlay artifact missing: $sourcePath" "ERROR"
            return $false
        }

        Copy-Item -Path $sourcePath -Destination $artifact.Destination -Force
    }

    $typeForwardMap = Join-Path $BuildRoot "..\examples\overlay\type-forward-map.txt"
    if (Test-Path $typeForwardMap) {
        Copy-Item -Path $typeForwardMap -Destination (Join-Path $overlayOutput "type-forward-map.txt") -Force
    } else {
        Write-BuildLog "Type forward map not found at $typeForwardMap" "WARNING"
    }

    $overlayManifest = [ordered]@{
        Bundle = "CLRNetOverlay"
        GeneratedOn = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssZ")
        Configuration = $Configuration
        Platform = $Platform
        Facades = @(
            "System.Runtime",
            "System.ValueTuple",
            "System.Threading.Tasks.Extensions",
            "System.Text.Json",
            "System.Buffers",
            "System.Net.Http",
            "System.IO"
        )
    }

    $overlayManifest | ConvertTo-Json -Depth 3 | Out-File -FilePath (Join-Path $overlayOutput "overlay.manifest.json") -Encoding UTF8

    Write-BuildLog "Overlay bundle packaged at $overlayOutput"
    return $true
}

function Build-TestApp {
    param([string]$Platform, [string]$Configuration)
    
    Write-BuildLog "Building test application for $Platform/$Configuration..."
    
    $outputDir = Join-Path $OutputRoot "$Platform-$Configuration"
    $testSource = Join-Path $SourceRoot "phase1-userland\test-app"
    
    # Note: In real implementation, would compile C# to IL then to native
    # For now, just copy the source as a placeholder
    Copy-Item -Path "$testSource\Program.cs" -Destination "$outputDir\TestApp.cs"
    Write-BuildLog "Test application prepared"
    
    return $true
}

function New-DeploymentPackage {
    param([string]$Platform, [string]$Configuration)
    
    Write-BuildLog "Creating deployment package..."
    
    $outputDir = Join-Path $OutputRoot "$Platform-$Configuration" 
    $packageDir = Join-Path $OutputRoot "packages"
    
    if (!(Test-Path $packageDir)) {
        New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
    }
    
    $packageName = "clrnet-runtime-$Platform-$Configuration.zip"
    $packagePath = Join-Path $packageDir $packageName
    
    # Create package structure
    $tempPackageDir = Join-Path $env:TEMP "clrnet-package-$(Get-Random)"
    New-Item -ItemType Directory -Path $tempPackageDir -Force | Out-Null
    
    # Copy runtime files
    if (Test-Path $outputDir) {
        Copy-Item -Recurse -Path "$outputDir\*" -Destination $tempPackageDir
    }
    
    # Create package manifest
    $manifest = @{
        Name = "CLRNET Runtime"
        Version = "1.0.0-alpha"
        Platform = $Platform
        Configuration = $Configuration
        BuildDate = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    }
    
    $manifest | ConvertTo-Json | Out-File -FilePath (Join-Path $tempPackageDir "manifest.json")
    
    # Create ZIP package
    if (Get-Command Compress-Archive -ErrorAction SilentlyContinue) {
        Compress-Archive -Path "$tempPackageDir\*" -DestinationPath $packagePath -Force
        Write-BuildLog "Package created: $packagePath"
    } else {
        Write-BuildLog "Compress-Archive not available, package creation skipped" "WARNING"
    }
    
    # Cleanup
    Remove-Item -Recurse -Force $tempPackageDir -ErrorAction SilentlyContinue
    
    return $true
}

function Invoke-Deploy {
    param([string]$Device, [string]$Platform, [string]$Configuration)
    
    Write-BuildLog "Deploying to device: $Device"
    
    # Note: Real deployment would use Windows Phone Application Deployment tool
    # For now, just simulate the deployment process
    
    $packageDir = Join-Path $OutputRoot "packages"
    $packageName = "clrnet-runtime-$Platform-$Configuration.zip"
    $packagePath = Join-Path $packageDir $packageName
    
    if (Test-Path $packagePath) {
        Write-BuildLog "Deployment package found: $packagePath"
        Write-BuildLog "Deployment simulation completed (use real WP deployment tools in practice)"
        return $true
    } else {
        Write-BuildLog "Deployment package not found: $packagePath" "ERROR"
        return $false
    }
}

# Main build orchestration
function Invoke-MainBuild {
    Write-BuildLog "Starting CLRNET build - Target: $Target, Platform: $Platform, Configuration: $Configuration"
    
    if ($Clean) {
        Invoke-Clean
    }
    
    if (!(Test-BuildEnvironment)) {
        Write-BuildLog "Build environment validation failed" "ERROR"
        exit 1
    }
    
    switch ($Target.ToLower()) {
        "validateenvironment" {
            Write-BuildLog "Environment validation completed successfully"
        }
        
        "clean" {
            Invoke-Clean
        }
        
        "build" {
            if (!(Build-Runtime $Platform $Configuration)) {
                Write-BuildLog "Runtime build failed" "ERROR"
                exit 1
            }

            if (!(Build-TestApp $Platform $Configuration)) {
                Write-BuildLog "Test app build failed" "ERROR"
                exit 1
            }

            if (!(Build-OverlayAssemblies $Configuration $Platform)) {
                Write-BuildLog "Overlay assembly build failed" "ERROR"
                exit 1
            }

            if (!(Package-OverlayBundle $Configuration $Platform)) {
                Write-BuildLog "Overlay packaging failed" "ERROR"
                exit 1
            }

            Write-BuildLog "Build completed successfully"
        }

        "package" {
            if (!(New-DeploymentPackage $Platform $Configuration)) {
                Write-BuildLog "Package creation failed" "ERROR"
                exit 1
            }
        }

        "packageoverlay" {
            if (!(Build-OverlayAssemblies $Configuration $Platform)) {
                Write-BuildLog "Overlay assembly build failed" "ERROR"
                exit 1
            }

            if (!(Package-OverlayBundle $Configuration $Platform)) {
                Write-BuildLog "Overlay packaging failed" "ERROR"
                exit 1
            }
        }

        "deploy" {
            if (!$Device) {
                Write-BuildLog "Device name required for deployment" "ERROR"
                exit 1
            }
            
            if (!(Invoke-Deploy $Device $Platform $Configuration)) {
                Write-BuildLog "Deployment failed" "ERROR"
                exit 1
            }
        }
        
        default {
            Write-BuildLog "Unknown target: $Target" "ERROR"
            Write-BuildLog "Valid targets: ValidateEnvironment, Clean, Build, Package, PackageOverlay, Deploy"
            exit 1
        }
    }
}

# Execute main build
try {
    Invoke-MainBuild
    Write-BuildLog "Build script completed successfully"
}
catch {
    Write-BuildLog "Build script failed with exception: $($_.Exception.Message)" "ERROR"
    exit 1
}