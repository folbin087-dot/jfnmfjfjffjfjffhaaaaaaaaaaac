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
echo Building JNI project (ALL ARCHITECTURES)
echo ========================================
echo.
echo NDK: %NDK%
echo Project: %~dp0jni
echo.

:: Clean previous build
if exist "%~dp0jni\libs" (
    echo Cleaning previous build...
    rmdir /S /Q "%~dp0jni\libs"
)
if exist "%~dp0jni\obj" (
    rmdir /S /Q "%~dp0jni\obj"
)

:: Backup Application.mk
copy /Y "%~dp0jni\Application.mk" "%~dp0jni\Application.mk.bak" >nul

:: Build x86_64
echo.
echo ========================================
echo Building x86_64...
echo ========================================
echo APP_ABI := x86_64 > "%~dp0jni\Application.mk"
echo APP_STL := c++_static >> "%~dp0jni\Application.mk"

cd /d "%~dp0jni"
"%NDK%\ndk-build.cmd" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk

if errorlevel 1 (
    echo.
    echo [ERROR] x86_64 build failed!
    copy /Y "%~dp0jni\Application.mk.bak" "%~dp0jni\Application.mk" >nul
    pause
    exit /b 1
)

:: Build arm64-v8a
echo.
echo ========================================
echo Building arm64-v8a...
echo ========================================
echo APP_ABI := arm64-v8a > "%~dp0jni\Application.mk"
echo APP_STL := c++_static >> "%~dp0jni\Application.mk"

cd /d "%~dp0jni"
"%NDK%\ndk-build.cmd" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk

if errorlevel 1 (
    echo.
    echo [ERROR] arm64-v8a build failed!
    copy /Y "%~dp0jni\Application.mk.bak" "%~dp0jni\Application.mk" >nul
    pause
    exit /b 1
)

:: Restore Application.mk
copy /Y "%~dp0jni\Application.mk.bak" "%~dp0jni\Application.mk" >nul
del "%~dp0jni\Application.mk.bak"

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.

:: Show output files
if exist "libs\x86_64\cheat.sh" (
    for %%A in ("libs\x86_64\cheat.sh") do (
        set /a KB=%%~zA / 1024
        echo x86_64:    libs\x86_64\cheat.sh ^(!KB! KB^)
    )
)

if exist "libs\arm64-v8a\cheat.sh" (
    for %%A in ("libs\arm64-v8a\cheat.sh") do (
        set /a KB=%%~zA / 1024
        echo arm64-v8a: libs\arm64-v8a\cheat.sh ^(!KB! KB^)
    )
)

echo.
pause
exit /b 0
