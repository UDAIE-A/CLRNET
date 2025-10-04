@echo off
REM CLRNet WP8.1 Sample App - Complete Validation Script

echo =========================================================
echo CLRNet Windows Phone 8.1 Sample App - Final Validation
echo =========================================================

cd /d "%~dp0\examples\WP81Integration"

echo Current directory: %CD%
echo.

REM Clean any previous build artifacts
echo Cleaning previous build artifacts...
if exist "bin" rmdir /s /q "bin" 2>nul
if exist "obj" rmdir /s /q "obj" 2>nul
echo ✓ Build artifacts cleaned

echo.
echo Validating project structure...

REM Validate core project files
set "VALIDATION_PASSED=1"

if not exist "CLRNetSampleApp.csproj" (
    echo ❌ Missing: CLRNetSampleApp.csproj
    set "VALIDATION_PASSED="
) else (
    echo ✓ CLRNetSampleApp.csproj found
)

if not exist "Package.appxmanifest" (
    echo ❌ Missing: Package.appxmanifest
    set "VALIDATION_PASSED="
) else (
    echo ✓ Package.appxmanifest found
)

if not exist "App.xaml" (
    echo ❌ Missing: App.xaml
    set "VALIDATION_PASSED="
) else (
    echo ✓ App.xaml found
)

if not exist "App.xaml.cs" (
    echo ❌ Missing: App.xaml.cs
    set "VALIDATION_PASSED="
) else (
    echo ✓ App.xaml.cs found
)

if not exist "MainPage.xaml" (
    echo ❌ Missing: MainPage.xaml
    set "VALIDATION_PASSED="
) else (
    echo ✓ MainPage.xaml found
)

if not exist "MainPage.xaml.cs" (
    echo ❌ Missing: MainPage.xaml.cs
    set "VALIDATION_PASSED="
) else (
    echo ✓ MainPage.xaml.cs found
)

REM Validate CLRNet runtime files
echo.
echo Validating CLRNet runtime files...

if not exist "CLRNet\CLRNetCore.dll" (
    echo ❌ Missing: CLRNet\CLRNetCore.dll
    set "VALIDATION_PASSED="
) else (
    echo ✓ CLRNet\CLRNetCore.dll found
)

if not exist "CLRNet\CLRNetInterop.dll" (
    echo ❌ Missing: CLRNet\CLRNetInterop.dll
    set "VALIDATION_PASSED="
) else (
    echo ✓ CLRNet\CLRNetInterop.dll found
)

if not exist "CLRNet\CLRNetSystem.dll" (
    echo ❌ Missing: CLRNet\CLRNetSystem.dll
    set "VALIDATION_PASSED="
) else (
    echo ✓ CLRNet\CLRNetSystem.dll found
)

if not exist "CLRNet\CLRNetHost.exe" (
    echo ❌ Missing: CLRNet\CLRNetHost.exe
    set "VALIDATION_PASSED="
) else (
    echo ✓ CLRNet\CLRNetHost.exe found
)

if not exist "SamplePlugin.dll" (
    echo ❌ Missing: SamplePlugin.dll
    set "VALIDATION_PASSED="
) else (
    echo ✓ SamplePlugin.dll found
)

REM Validate asset files
echo.
echo Validating asset files...

if not exist "Assets\Logo.png" (
    echo ❌ Missing: Assets\Logo.png
    set "VALIDATION_PASSED="
) else (
    echo ✓ Assets\Logo.png found
)

if not exist "Assets\SmallLogo.png" (
    echo ❌ Missing: Assets\SmallLogo.png
    set "VALIDATION_PASSED="
) else (
    echo ✓ Assets\SmallLogo.png found
)

if not exist "Assets\SplashScreen.png" (
    echo ❌ Missing: Assets\SplashScreen.png
    set "VALIDATION_PASSED="
) else (
    echo ✓ Assets\SplashScreen.png found
)

if not exist "Properties\AssemblyInfo.cs" (
    echo ❌ Missing: Properties\AssemblyInfo.cs
    set "VALIDATION_PASSED="
) else (
    echo ✓ Properties\AssemblyInfo.cs found
)

if not exist "Properties\Default.rd.xml" (
    echo ❌ Missing: Properties\Default.rd.xml
    set "VALIDATION_PASSED="
) else (
    echo ✓ Properties\Default.rd.xml found
)

echo.
echo Validating manifest content...

REM Check for problematic manifest elements
findstr /C:"Extension" Package.appxmanifest >nul 2>&1
if not errorlevel 1 (
    echo ⚠️  Warning: Manifest contains Extension elements (may cause validation errors)
)

findstr /C:"InProcessServer" Package.appxmanifest >nul 2>&1
if not errorlevel 1 (
    echo ⚠️  Warning: Manifest contains InProcessServer elements (not supported in WP8.1)
)

findstr /C:"PhoneCapability" Package.appxmanifest >nul 2>&1
if not errorlevel 1 (
    echo ⚠️  Warning: Manifest contains PhoneCapability elements (may cause validation errors)
)

findstr /C:"DeviceCapability" Package.appxmanifest >nul 2>&1
if not errorlevel 1 (
    echo ⚠️  Warning: Manifest contains DeviceCapability elements (check for proper schema)
) else (
    echo ✓ No problematic DeviceCapability elements found
)

echo.
if defined VALIDATION_PASSED (
    echo ✅ PROJECT VALIDATION PASSED!
    echo.
    echo All required files are present and the project structure is correct.
    echo The CLRNet Windows Phone 8.1 Sample App is ready for compilation.
    echo.
    echo Next steps:
    echo 1. Open CLRNetSampleApp.csproj in Visual Studio 2013
    echo 2. Build for Windows Phone 8.1 ARM platform
    echo 3. Deploy to device or emulator
    echo.
    echo Build command: msbuild CLRNetSampleApp.csproj /p:Platform=ARM /p:Configuration=Release
) else (
    echo ❌ PROJECT VALIDATION FAILED!
    echo.
    echo Some required files are missing. Please check the errors above.
    echo Run the build setup scripts to create missing files.
)

echo.
echo Validation completed.
pause