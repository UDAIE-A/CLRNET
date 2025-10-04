@echo off
echo ===============================================
echo CLRNet Runtime - Binary Verification Script
echo ===============================================
echo.

set BUILD_DIR=%~dp0..\bin\ARM\Release
set PACKAGE_DIR=%BUILD_DIR%\packages

echo [INFO] Checking binaries in: %BUILD_DIR%
echo.

:: Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo [ERROR] Build directory not found: %BUILD_DIR%
    echo Please run build-all.bat first
    goto :error
)

:: Define expected binaries
set CORE_BINARIES=CLRNetCore.dll CLRNetHost.exe
set INTEROP_BINARIES=CLRNetInterop.dll  
set SYSTEM_BINARIES=CLRNetSystem.dll
set TEST_BINARIES=CLRNetTests.exe

echo ===============================================
echo Verifying Core Runtime Binaries
echo ===============================================

for %%f in (%CORE_BINARIES%) do (
    if exist "%BUILD_DIR%\%%f" (
        echo [OK] %%f
        :: Check file size (should be reasonable)
        for %%s in ("%BUILD_DIR%\%%f") do (
            if %%~zs GTR 0 (
                echo     Size: %%~zs bytes
            ) else (
                echo [WARNING] %%f has zero size
            )
        )
        
        :: Check if it's a valid PE file (Windows executable)
        "%SystemRoot%\System32\dumpbin.exe" /headers "%BUILD_DIR%\%%f" | findstr /C:"machine (ARM)" >nul 2>&1
        if errorlevel 1 (
            echo [WARNING] %%f may not be ARM architecture
        ) else (
            echo     Architecture: ARM [OK]
        )
    ) else (
        echo [ERROR] Missing: %%f
        set HAS_ERRORS=1
    )
    echo.
)

echo ===============================================
echo Verifying Interop Binaries  
echo ===============================================

for %%f in (%INTEROP_BINARIES%) do (
    if exist "%BUILD_DIR%\%%f" (
        echo [OK] %%f
        for %%s in ("%BUILD_DIR%\%%f") do (
            echo     Size: %%~zs bytes
        )
    ) else (
        echo [ERROR] Missing: %%f
        set HAS_ERRORS=1
    )
    echo.
)

echo ===============================================
echo Verifying System Integration Binaries
echo ===============================================

for %%f in (%SYSTEM_BINARIES%) do (
    if exist "%BUILD_DIR%\%%f" (
        echo [OK] %%f
        for %%s in ("%BUILD_DIR%\%%f") do (
            echo     Size: %%~zs bytes
        )
    ) else (
        echo [ERROR] Missing: %%f  
        set HAS_ERRORS=1
    )
    echo.
)

echo ===============================================
echo Verifying Test Binaries
echo ===============================================

for %%f in (%TEST_BINARIES%) do (
    if exist "%BUILD_DIR%\%%f" (
        echo [OK] %%f
        for %%s in ("%BUILD_DIR%\%%f") do (
            echo     Size: %%~zs bytes
        )
    ) else (
        echo [WARNING] Missing: %%f (test binary)
    )
    echo.
)

echo ===============================================
echo Verifying Deployment Packages
echo ===============================================

if exist "%PACKAGE_DIR%" (
    echo [OK] Package directory exists
    
    if exist "%PACKAGE_DIR%\CLRNet-Runtime" (
        echo [OK] Runtime package directory
        dir /b "%PACKAGE_DIR%\CLRNet-Runtime\*.dll" "%PACKAGE_DIR%\CLRNet-Runtime\*.exe" 2>nul | find /c /v "" > temp_count.txt
        set /p FILE_COUNT=<temp_count.txt
        del temp_count.txt
        echo     Contains files (approximate)
    ) else (
        echo [WARNING] Runtime package directory missing
    )
    
    if exist "%PACKAGE_DIR%\CLRNet-Complete" (
        echo [OK] Complete package directory
    ) else (
        echo [WARNING] Complete package directory missing  
    )
) else (
    echo [WARNING] Package directory not found: %PACKAGE_DIR%
    echo Run build-all.bat to create deployment packages
)

echo.
echo ===============================================
echo Binary Functionality Test
echo ===============================================

:: Test CLRNetHost.exe basic functionality
if exist "%BUILD_DIR%\CLRNetHost.exe" (
    echo [TEST] Testing CLRNetHost.exe --help
    "%BUILD_DIR%\CLRNetHost.exe" --help >nul 2>&1
    if errorlevel 1 (
        echo [WARNING] CLRNetHost.exe may have issues (exit code: %errorlevel%)
    ) else (
        echo [OK] CLRNetHost.exe responds to commands
    )
    
    echo [TEST] Testing CLRNetHost.exe --info  
    "%BUILD_DIR%\CLRNetHost.exe" --info >nul 2>&1
    if errorlevel 1 (
        echo [WARNING] CLRNetHost.exe info command may have issues
    ) else (
        echo [OK] CLRNetHost.exe info command works
    )
) else (
    echo [SKIP] CLRNetHost.exe not found - cannot test functionality
)

echo.
echo ===============================================
echo Dependency Check
echo ===============================================

:: Check for required system DLLs
set SYSTEM_DEPS=kernel32.dll user32.dll msvcr120.dll

for %%d in (%SYSTEM_DEPS%) do (
    if exist "%SystemRoot%\System32\%%d" (
        echo [OK] System dependency: %%d
    ) else (
        echo [WARNING] Missing system dependency: %%d
    )
)

:: Check Visual C++ Runtime
reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\12.0\VC\Runtimes\arm" >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Visual C++ 2013 ARM Runtime may not be installed
) else (
    echo [OK] Visual C++ 2013 ARM Runtime detected
)

echo.
echo ===============================================
echo Security Check
echo ===============================================

:: Check if binaries are signed (optional for development)
for %%f in (%CORE_BINARIES% %INTEROP_BINARIES% %SYSTEM_BINARIES%) do (
    if exist "%BUILD_DIR%\%%f" (
        signtool verify /pa "%BUILD_DIR%\%%f" >nul 2>&1
        if errorlevel 1 (
            echo [INFO] %%f is not digitally signed (expected for development)
        ) else (
            echo [OK] %%f is digitally signed
        )
    )
)

echo.
echo ===============================================
echo VERIFICATION SUMMARY
echo ===============================================

if defined HAS_ERRORS (
    echo [RESULT] VERIFICATION FAILED
    echo Some critical binaries are missing or invalid
    echo Please run build-all.bat to rebuild the runtime
    goto :error
) else (
    echo [RESULT] VERIFICATION PASSED
    echo All critical binaries are present and appear valid
    echo.
    echo Your CLRNet Runtime binaries are ready for deployment!
    echo.
    echo Next steps:
    echo 1. Test on Windows Phone 8.1 emulator
    echo 2. Deploy to physical Windows Phone 8.1 device  
    echo 3. Run test applications to verify functionality
    echo 4. Monitor performance and stability
)

echo.
goto :end

:error
echo.
echo ===============================================
echo VERIFICATION FAILED
echo ===============================================
echo.
echo Please check the errors above and:
echo 1. Ensure build-all.bat completed successfully
echo 2. Check that Visual Studio and WP8.1 SDK are properly installed
echo 3. Verify you have the correct build tools for ARM compilation
echo 4. Run build-all.bat again if necessary
echo.
exit /b 1

:end
echo Press any key to continue...
pause >nul