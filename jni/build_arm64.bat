@echo off
chcp 65001 >nul
setlocal

:: Android NDK path
set "NDK=C:\android-ndk-r29-windows\android-ndk-r29"

:: Check if NDK exists
if not exist "%NDK%" (
    echo [ERROR] Android NDK not found at: %NDK%
    echo.
    echo Please install Android NDK r29 or update the NDK path in this script.
    echo Download from: https://developer.android.com/ndk/downloads
    pause
    exit /b 1
)

:: Check if ndk-build exists
if not exist "%NDK%\ndk-build.cmd" (
    echo [ERROR] ndk-build.cmd not found in NDK directory
    pause
    exit /b 1
)

echo ========================================
echo Building JNI project (ARM64-V8A)
echo ========================================
echo.
echo NDK: %NDK%
echo Project: %~dp0jni
echo Architecture: arm64-v8a
echo.

:: Clean previous build
if exist "%~dp0jni\libs" (
    echo Cleaning previous build...
    rmdir /S /Q "%~dp0jni\libs"
)
if exist "%~dp0jni\obj" (
    rmdir /S /Q "%~dp0jni\obj"
)

:: Set Application.mk for arm64-v8a
echo APP_ABI := arm64-v8a > "%~dp0jni\Application.mk"
echo APP_STL := c++_static >> "%~dp0jni\Application.mk"

:: Build using ndk-build
echo.
echo Building...
cd /d "%~dp0jni"
"%NDK%\ndk-build.cmd" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.

:: Show output files
if exist "libs\arm64-v8a\cheat.sh" (
    for %%A in ("libs\arm64-v8a\cheat.sh") do (
        set /a KB=%%~zA / 1024
        echo Output: libs\arm64-v8a\cheat.sh ^(!KB! KB^)
    )
    echo.
    echo To install on device:
    echo   adb push libs\arm64-v8a\cheat.sh /data/local/tmp/
    echo   adb shell chmod 755 /data/local/tmp/cheat.sh
    echo   adb shell su -c "/data/local/tmp/cheat.sh"
) else (
    echo [WARNING] Output file not found: libs\arm64-v8a\cheat.sh
)

echo.
pause
exit /b 0
