@echo off
setlocal EnableDelayedExpansion
echo ===============================================
echo CLRNet Runtime - Complete Build Script
echo ===============================================
echo.

:: Set environment variables
set SOLUTION_DIR=%~dp0..
set BUILD_CONFIG=Release
set BUILD_PLATFORM=ARM
set BUILD_OUTPUT=%SOLUTION_DIR%\build\bin\%BUILD_PLATFORM%\%BUILD_CONFIG%

:: Check for Visual Studio environment
if not defined VCINSTALLDIR (
    echo [ERROR] Visual Studio environment not detected
    echo Please run from Visual Studio Developer Command Prompt
    echo Or run: "C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"
    pause
    exit /b 1
)

:: Check for Windows Phone SDK
if not exist "%ProgramFiles(x86)%\Microsoft SDKs\WindowsPhoneApp\v8.1" (
    echo [ERROR] Windows Phone 8.1 SDK not found
    echo Please install Windows Phone 8.1 SDK
    pause
    exit /b 1
)

:: Prefer Windows Phone 8.1 SDK headers/libs over newer Desktop kits
call :ConfigureWp81Sdk "%ProgramFiles(x86)%\Microsoft SDKs\WindowsPhoneApp\v8.1"

echo [INFO] Building CLRNet Runtime...
echo [INFO] Configuration: %BUILD_CONFIG%
echo [INFO] Platform: %BUILD_PLATFORM%
echo [INFO] Output Directory: %BUILD_OUTPUT%
echo.

:: Create output directories
if not exist "%BUILD_OUTPUT%" mkdir "%BUILD_OUTPUT%"
if not exist "%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%" mkdir "%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%"

:: Phase 1: Build Core Runtime
echo ===============================================
echo Phase 1: Building Core Runtime Components
echo ===============================================

call :CollectSources CORE_SOURCES "%SOLUTION_DIR%\src\phase1-userland\core\*.cpp"

echo [BUILD] CLRNetCore.dll - Main runtime library
cl /nologo /W3 /O2 /MD /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /DCLRNET_EXPORTS ^
   /I"%SOLUTION_DIR%\src" /I"%SOLUTION_DIR%\include" ^
   /Fo"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\\" ^
   /Fd"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\CLRNetCore.pdb" ^
   %CORE_SOURCES% ^
   /link /OUT:"%BUILD_OUTPUT%\CLRNetCore.dll" /DLL /PDB:"%BUILD_OUTPUT%\CLRNetCore.pdb"

if errorlevel 1 (
    echo [ERROR] Failed to build CLRNetCore.dll
    goto :error
)
echo [OK] CLRNetCore.dll built successfully

echo [BUILD] CLRNetHost.exe - Runtime host executable  
cl /nologo /W3 /O2 /MD /DWIN32 /DNDEBUG /D_CONSOLE ^
   /I"%SOLUTION_DIR%\src" /I"%SOLUTION_DIR%\include" ^
   /Fo"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\\" ^
   /Fd"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\CLRNetHost.pdb" ^
   "%SOLUTION_DIR%\src\host\CLRNetHost.cpp" ^
   /link /OUT:"%BUILD_OUTPUT%\CLRNetHost.exe" /PDB:"%BUILD_OUTPUT%\CLRNetHost.pdb" ^
   "%BUILD_OUTPUT%\CLRNetCore.lib"

if errorlevel 1 (
    echo [ERROR] Failed to build CLRNetHost.exe
    goto :error
)
echo [OK] CLRNetHost.exe built successfully

:: Phase 2: Build Interop Components
echo.
echo ===============================================
echo Phase 2: Building Interop Components
echo ===============================================

call :CollectSources INTEROP_SOURCES ^
   "%SOLUTION_DIR%\src\interop\*.cpp" ^
   "%SOLUTION_DIR%\src\interop\winrt\*.cpp" ^
   "%SOLUTION_DIR%\src\interop\pinvoke\*.cpp" ^
   "%SOLUTION_DIR%\src\interop\hardware\*.cpp"

echo [BUILD] CLRNetInterop.dll - System interop layer
cl /nologo /W3 /O2 /MD /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /DCLRNET_INTEROP_EXPORTS ^
   /I"%SOLUTION_DIR%\src" /I"%SOLUTION_DIR%\include" ^
   /Fo"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\\" ^
   /Fd"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\CLRNetInterop.pdb" ^
   %INTEROP_SOURCES% ^
   /link /OUT:"%BUILD_OUTPUT%\CLRNetInterop.dll" /DLL /PDB:"%BUILD_OUTPUT%\CLRNetInterop.pdb" ^
   "%BUILD_OUTPUT%\CLRNetCore.lib" windowsapp.lib

if errorlevel 1 (
    echo [ERROR] Failed to build CLRNetInterop.dll
    goto :error
)
echo [OK] CLRNetInterop.dll built successfully

:: Phase 3: Build System Integration Components  
echo.
echo ===============================================
echo Phase 3: Building System Integration Components
echo ===============================================

call :CollectSources SYSTEM_SOURCES ^
   "%SOLUTION_DIR%\src\system\replacement\*.cpp" ^
   "%SOLUTION_DIR%\src\system\hooks\*.cpp" ^
   "%SOLUTION_DIR%\src\system\compatibility\*.cpp"

echo [BUILD] CLRNetSystem.dll - System integration layer
cl /nologo /W3 /O2 /MD /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /DCLRNET_SYSTEM_EXPORTS ^
   /I"%SOLUTION_DIR%\src" /I"%SOLUTION_DIR%\include" ^
   /Fo"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\\" ^
   /Fd"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\CLRNetSystem.pdb" ^
   %SYSTEM_SOURCES% ^
   /link /OUT:"%BUILD_OUTPUT%\CLRNetSystem.dll" /DLL /PDB:"%BUILD_OUTPUT%\CLRNetSystem.pdb" ^
   "%BUILD_OUTPUT%\CLRNetCore.lib" "%BUILD_OUTPUT%\CLRNetInterop.lib" ntdll.lib psapi.lib

if errorlevel 1 (
    echo [ERROR] Failed to build CLRNetSystem.dll
    goto :error
)
echo [OK] CLRNetSystem.dll built successfully

:: Build Test Suite
echo.
echo ===============================================
echo Building Test Suite
echo ===============================================

call :CollectSources TEST_SOURCES ^
   "%SOLUTION_DIR%\tests\core\*.cpp" ^
   "%SOLUTION_DIR%\tests\interop\*.cpp" ^
   "%SOLUTION_DIR%\tests\system\*.cpp"

echo [BUILD] CLRNetTests.exe - Test suite runner
cl /nologo /W3 /O2 /MD /DWIN32 /DNDEBUG /D_CONSOLE ^
   /I"%SOLUTION_DIR%\src" /I"%SOLUTION_DIR%\include" /I"%SOLUTION_DIR%\tests\include" ^
   /Fo"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\\" ^
   /Fd"%SOLUTION_DIR%\build\obj\%BUILD_PLATFORM%\%BUILD_CONFIG%\CLRNetTests.pdb" ^
   %TEST_SOURCES% ^
   /link /OUT:"%BUILD_OUTPUT%\CLRNetTests.exe" /PDB:"%BUILD_OUTPUT%\CLRNetTests.pdb" ^
   "%BUILD_OUTPUT%\CLRNetCore.lib" "%BUILD_OUTPUT%\CLRNetInterop.lib" "%BUILD_OUTPUT%\CLRNetSystem.lib"

if errorlevel 1 (
    echo [ERROR] Failed to build CLRNetTests.exe
    goto :error
)
echo [OK] CLRNetTests.exe built successfully

:: Create deployment packages
echo.
echo ===============================================
echo Creating Deployment Packages
echo ===============================================

set PACKAGE_DIR=%BUILD_OUTPUT%\packages
set OVERLAY_ROOT=%SOLUTION_DIR%\build\output\%BUILD_PLATFORM%-%BUILD_CONFIG%\CLRNetOverlay

echo [PACK] CLRNet overlay facades
powershell -NoProfile -ExecutionPolicy Bypass -File "%SOLUTION_DIR%\build\scripts\package-overlay.ps1" -Configuration %BUILD_CONFIG% -Platform %BUILD_PLATFORM%
if errorlevel 1 (
    echo [ERROR] Overlay packaging script failed
    goto :error
)

if not exist "%OVERLAY_ROOT%" (
    echo [ERROR] Overlay bundle was not generated at %OVERLAY_ROOT%
    goto :error
)

if not exist "%PACKAGE_DIR%\CLRNet-Overlay" mkdir "%PACKAGE_DIR%\CLRNet-Overlay"
xcopy "%OVERLAY_ROOT%" "%PACKAGE_DIR%\CLRNet-Overlay\" /E /I /Y >nul
echo [OK] Overlay package created

:: Core Runtime Package
if not exist "%PACKAGE_DIR%\CLRNet-Runtime" mkdir "%PACKAGE_DIR%\CLRNet-Runtime"
copy "%BUILD_OUTPUT%\CLRNetCore.dll" "%PACKAGE_DIR%\CLRNet-Runtime\" >nul
copy "%BUILD_OUTPUT%\CLRNetHost.exe" "%PACKAGE_DIR%\CLRNet-Runtime\" >nul
copy "%BUILD_OUTPUT%\CLRNetCore.pdb" "%PACKAGE_DIR%\CLRNet-Runtime\" >nul
copy "%BUILD_OUTPUT%\CLRNetHost.pdb" "%PACKAGE_DIR%\CLRNet-Runtime\" >nul
echo [OK] Core Runtime package created

:: Interop Package  
if not exist "%PACKAGE_DIR%\CLRNet-Interop" mkdir "%PACKAGE_DIR%\CLRNet-Interop"
copy "%BUILD_OUTPUT%\CLRNetInterop.dll" "%PACKAGE_DIR%\CLRNet-Interop\" >nul
copy "%BUILD_OUTPUT%\CLRNetInterop.pdb" "%PACKAGE_DIR%\CLRNet-Interop\" >nul
echo [OK] Interop package created

:: System Integration Package
if not exist "%PACKAGE_DIR%\CLRNet-System" mkdir "%PACKAGE_DIR%\CLRNet-System" 
copy "%BUILD_OUTPUT%\CLRNetSystem.dll" "%PACKAGE_DIR%\CLRNet-System\" >nul
copy "%BUILD_OUTPUT%\CLRNetSystem.pdb" "%PACKAGE_DIR%\CLRNet-System\" >nul
echo [OK] System Integration package created

:: Complete Package
if not exist "%PACKAGE_DIR%\CLRNet-Complete" mkdir "%PACKAGE_DIR%\CLRNet-Complete"
copy "%BUILD_OUTPUT%\*.dll" "%PACKAGE_DIR%\CLRNet-Complete\" >nul
copy "%BUILD_OUTPUT%\*.exe" "%PACKAGE_DIR%\CLRNet-Complete\" >nul
copy "%BUILD_OUTPUT%\*.pdb" "%PACKAGE_DIR%\CLRNet-Complete\" >nul
echo [OK] Complete package created

:: Verify binaries
echo.
echo ===============================================
echo Verifying Binaries
echo ===============================================

for %%f in (CLRNetCore.dll CLRNetHost.exe CLRNetInterop.dll CLRNetSystem.dll CLRNetTests.exe) do (
    if exist "%BUILD_OUTPUT%\%%f" (
        echo [OK] %%f - Built successfully
    ) else (
        echo [ERROR] %%f - Missing!
        goto :error
    )
)

:: Display build summary
echo.
echo ===============================================
echo BUILD COMPLETED SUCCESSFULLY!
echo ===============================================
echo.
echo Binaries created in: %BUILD_OUTPUT%
echo Packages created in: %PACKAGE_DIR%
echo.
echo Built components:
echo   - CLRNetCore.dll     (Core runtime)
echo   - CLRNetHost.exe     (Host executable) 
echo   - CLRNetInterop.dll  (Interop layer)
echo   - CLRNetSystem.dll   (System integration)
echo   - CLRNetTests.exe    (Test suite)
echo   - CLRNetOverlay      (Track A facade bundle)
echo.
echo Deployment packages:
echo   - CLRNet-Runtime     (Core components)
echo   - CLRNet-Interop     (Interop components)
echo   - CLRNet-System      (System components)
echo   - CLRNet-Overlay     (Track A facades)
echo   - CLRNet-Complete    (All components)
echo.
echo [SUCCESS] CLRNet Runtime build completed!
echo Your modern .NET runtime binaries are ready for deployment.
echo.
goto :end

:ConfigureWp81Sdk
set "SDK_ROOT=%~1"
if "%SDK_ROOT%"=="" goto configure_done
if not exist "%SDK_ROOT%" goto configure_done

echo [INFO] Using Windows Phone 8.1 SDK at %SDK_ROOT%

set "SDK_INCLUDE_ARM=%SDK_ROOT%\Include\marm"
set "SDK_INCLUDE_SHARED=%SDK_ROOT%\Include"
set "SDK_INCLUDE_CRT=%SDK_INCLUDE_ARM%\crt"
set "SDK_LIB_ARM=%SDK_ROOT%\Lib\ARM"
set "SDK_LIB_WIN81=%SDK_ROOT%\Lib\winv6.3\um\ARM"
set "SDK_BIN=%SDK_ROOT%\bin"
set "SDK_REF=%SDK_ROOT%\References\CommonConfiguration\Neutral"

if exist "%SDK_INCLUDE_CRT%" set "INCLUDE=%SDK_INCLUDE_CRT%;%SDK_INCLUDE_ARM%;%SDK_INCLUDE_SHARED%;%INCLUDE%"
if exist "%SDK_LIB_ARM%" set "LIB=%SDK_LIB_ARM%;%SDK_LIB_WIN81%;%LIB%"
if exist "%SDK_BIN%" set "PATH=%SDK_BIN%;%PATH%"
if exist "%SDK_ROOT%" set "WindowsSdkDir=%SDK_ROOT%\"
if exist "%SDK_REF%" set "WindowsPhoneReferencePath=%SDK_REF%"

:configure_done
goto :eof

:CollectSources
setlocal EnableDelayedExpansion
set OUTPUT_VAR=%~1
shift
set FILES=

:collect_loop
if "%~1"=="" goto collect_done
if exist "%~1" (
    for /f "delims=" %%I in ('dir /b /s "%~1" 2^>nul') do (
        set FILES=!FILES! "%%~fI"
    )
) else (
    echo [WARNING] No sources matched pattern %~1
)
shift
goto collect_loop

:collect_done
endlocal & set "%OUTPUT_VAR%=%FILES%"
goto :eof

:error
echo.
echo ===============================================
echo BUILD FAILED!
echo ===============================================
echo.
echo Please check the error messages above and ensure:
echo   1. Visual Studio 2013+ with Windows Phone 8.1 SDK is installed
echo   2. You are running from a Visual Studio Developer Command Prompt
echo   3. All source files are present and accessible
echo   4. You have sufficient disk space for build output
echo.
exit /b 1

:end
echo Press any key to continue...
pause >nul
