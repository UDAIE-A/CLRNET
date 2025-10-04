@echo off
REM Build verification script for CLRNet WP8.1 Sample App

echo ===============================================
echo CLRNet WP8.1 Sample App - Build Verification
echo ===============================================

REM Check if Visual Studio 2013 is available
if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe" (
    echo ERROR: Visual Studio 2013 not found
    echo Please install Visual Studio 2013 with Windows Phone 8.1 SDK
    pause
    exit /b 1
)

REM Check if Windows Phone 8.1 SDK is available
if not exist "%ProgramFiles(x86)%\Microsoft SDKs\WindowsPhoneApp\v8.1" (
    echo ERROR: Windows Phone 8.1 SDK not found
    echo Please install Windows Phone 8.1 SDK
    pause
    exit /b 1
)

echo ✓ Visual Studio 2013 found
echo ✓ Windows Phone 8.1 SDK found

REM Set up build environment
echo Setting up build environment...
call "%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"

if errorlevel 1 (
    echo ERROR: Failed to initialize Visual Studio environment
    pause
    exit /b 1
)

echo ✓ Build environment initialized

REM Navigate to project directory
cd /d "%~dp0"
cd examples\WP81Integration

if not exist "CLRNetSampleApp.csproj" (
    echo ERROR: Project file not found
    echo Current directory: %CD%
    pause
    exit /b 1
)

echo ✓ Project file found

REM Build the project
echo.
echo Building CLRNet Sample App for Windows Phone 8.1...
echo.

msbuild CLRNetSampleApp.csproj /p:Configuration=Debug /p:Platform=ARM /p:OutputPath=bin\ARM\Debug\ /verbosity:minimal

if errorlevel 1 (
    echo.
    echo ❌ Build FAILED
    echo Check the error messages above for details
    pause
    exit /b 1
) else (
    echo.
    echo ✅ Build SUCCESSFUL!
    echo.
    echo Output files:
    if exist "bin\ARM\Debug\CLRNetSampleApp.exe" (
        echo ✓ CLRNetSampleApp.exe
        dir "bin\ARM\Debug\CLRNetSampleApp.*" /b
    )
    
    echo.
    echo CLRNet Sample App has been successfully built!
    echo You can now deploy it to a Windows Phone 8.1 device or emulator.
)

echo.
echo Build verification completed.
pause