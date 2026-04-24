@echo off
chcp 65001 >nul
setlocal

:: set your ndk path here
set "NDK=C:\android-ndk-r29-windows\android-ndk-r29"
set "ROOT=%~dp0.."
set "CMAKE=%ROOT%\tools\cmake\bin\cmake.exe"
set "NINJA=%ROOT%\tools\cmake\bin\ninja.exe"
set "OUT=%ROOT%\build"
set "LIB=%ROOT%\libs"

set /a T=%NUMBER_OF_PROCESSORS% / 2
if %T% LSS 2 set T=2

if not exist "%NDK%" (
    echo ndk not found
    pause
    exit /b 1
)

if not exist "%CMAKE%" (
    echo cmake not found
    pause
    exit /b 1
)

call :build arm64 arm64-v8a
if errorlevel 1 goto :err

echo.
echo done
pause
exit /b 0

:err
pause
exit /b 1

:build
set "N=%~1"
set "A=%~2"
set "BP=%OUT%\%A%"
set "LP=%LIB%\%N%"

echo.
echo %N%

if not exist "%BP%" mkdir "%BP%"

if exist "%BP%\CMakeCache.txt" del /F /Q "%BP%\CMakeCache.txt"

"%CMAKE%" -S . -B "%BP%" -G "Ninja" ^
  -DCMAKE_TOOLCHAIN_FILE="%NDK%\build\cmake\android.toolchain.cmake" ^
  -DANDROID_ABI=%A% ^
  -DANDROID_PLATFORM=android-21 ^
  -DANDROID_STL=c++_static ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BP%" --parallel %T%
if errorlevel 1 exit /b 1

if not exist "%LP%" mkdir "%LP%"

if not exist "%BP%\bycmd.sh" (
    echo output not found
    exit /b 1
)

copy /Y "%BP%\bycmd.sh" "%LP%\bycmd_%N%" >nul

for %%A in ("%LP%\bycmd_%N%") do set "SZ=%%~zA"
set /a KB=%SZ% / 1024

echo ok %KB% kb

exit /b 0