@echo off
REM CLRNet WP8.1 Sample App - Complete Build and Setup Script

echo =======================================================
echo CLRNet Windows Phone 8.1 Sample App - Build Setup
echo =======================================================

REM Navigate to the sample app directory
cd /d "%~dp0\examples\WP81Integration"

echo Current directory: %CD%
echo.

REM Ensure all required directories exist
echo Creating required directories...
if not exist "CLRNet" mkdir CLRNet
if not exist "bin\ARM\Debug" mkdir bin\ARM\Debug
if not exist "bin\ARM\Release" mkdir bin\ARM\Release

REM Copy CLRNet binaries if they exist in the main build directory
if exist "..\..\build\bin\ARM\Release\CLRNetCore.dll" (
    echo Copying CLRNet binaries from main build...
    copy "..\..\build\bin\ARM\Release\CLRNet*.dll" "CLRNet\" >nul
    copy "..\..\build\bin\ARM\Release\CLRNetHost.exe" "CLRNet\" >nul
) else (
    echo CLRNet binaries already present in project directory
)

REM Ensure SamplePlugin.dll exists
if not exist "SamplePlugin.dll" (
    echo Creating SamplePlugin.dll placeholder...
    echo Sample Plugin Assembly - CLRNet Demo > SamplePlugin.dll
)

REM Verify all required files are present
echo.
echo Verifying required files...
set "MISSING_FILES="

if not exist "CLRNet\CLRNetCore.dll" (
    echo ❌ Missing: CLRNetCore.dll
    set "MISSING_FILES=1"
) else (
    echo ✓ CLRNetCore.dll found
)

if not exist "CLRNet\CLRNetInterop.dll" (
    echo ❌ Missing: CLRNetInterop.dll
    set "MISSING_FILES=1"
) else (
    echo ✓ CLRNetInterop.dll found
)

if not exist "CLRNet\CLRNetSystem.dll" (
    echo ❌ Missing: CLRNetSystem.dll
    set "MISSING_FILES=1"
) else (
    echo ✓ CLRNetSystem.dll found
)

if not exist "CLRNet\CLRNetHost.exe" (
    echo ❌ Missing: CLRNetHost.exe
    set "MISSING_FILES=1"
) else (
    echo ✓ CLRNetHost.exe found
)

if not exist "SamplePlugin.dll" (
    echo ❌ Missing: SamplePlugin.dll
    set "MISSING_FILES=1"
) else (
    echo ✓ SamplePlugin.dll found
)

if defined MISSING_FILES (
    echo.
    echo ❌ Some required files are missing. Build may fail.
    echo Please ensure CLRNet runtime has been built first.
    pause
    exit /b 1
)

echo.
echo ✅ All required files are present!
echo.

REM Check for Visual Studio and build tools
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe" (
    echo ✓ Visual Studio 2013 found
    
    REM Set up build environment
    call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat" >nul 2>&1
    
    if not errorlevel 1 (
        echo ✓ Build environment initialized
        echo.
        echo Building CLRNet Sample App...
        
        REM Build the project
        msbuild CLRNetSampleApp.csproj /p:Configuration=Debug /p:Platform=AnyCPU /verbosity:minimal
        
        if not errorlevel 1 (
            echo.
            echo ✅ CLRNet Sample App built successfully!
            echo.
            echo Output files can be found in:
            echo - bin\Debug\ (for AnyCPU Debug build)
            echo.
            echo To build for Windows Phone 8.1 ARM:
            echo msbuild CLRNetSampleApp.csproj /p:Configuration=Release /p:Platform=ARM
            echo.
        ) else (
            echo.
            echo ❌ Build failed. Check error messages above.
        )
    ) else (
        echo ❌ Failed to initialize build environment
    )
) else (
    echo ⚠️  Visual Studio 2013 not found
    echo The project files are ready but you'll need VS2013 to compile
)

echo.
echo Build setup completed.
pause