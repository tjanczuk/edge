@echo off
set SELF=%~dp0
if "%1" equ "" (
    echo Usage: build.bat debug^|release "{version} {version}" ...
    echo e.g. build.bat release "0.8.22 0.10.0"
    exit /b -1
)

SET FLAVOR=%1
shift
if "%FLAVOR%" equ "" set FLAVOR=release
WHERE iojs
if %ERRORLEVEL% NEQ 0 (
    set RUNNINGIOJS=0
    for %%i in (node.exe) do set NODEEXE=%%~$PATH:i
) else (
    set RUNNINGIOJS=1
    for %%i in (iojs.exe) do set NODEEXE=%%~$PATH:i
)
if not exist "%NODEEXE%" (
    echo Cannot find node.exe
    popd
    exit /b -1
)
for %%i in ("%NODEEXE%") do set NODEDIR=%%~dpi
SET DESTDIRROOT=%SELF%\..\lib\native\win32
set VERSIONS=
:harvestVersions
if "%1" neq "" (
    set VERSIONS=%VERSIONS% %1
    shift
    goto :harvestVersions
)
if "%VERSIONS%" equ "" set VERSIONS=0.10.0
pushd %SELF%\..
for %%V in (%VERSIONS%) do call :build ia32 x86 %%V 
for %%V in (%VERSIONS%) do call :build x64 x64 %%V 
popd

exit /b 0

:build

set DESTDIR=%DESTDIRROOT%\%1\%3

for /F "tokens=1 delims=." %%a in ("%3%") do (
    if "%%a" equ "0" (
        set TARGETIOJS=0
        set TARGETNODEEXE=%DESTDIR%\node.exe
    ) else (
        set TARGETIOJS=1
        set TARGETNODEEXE=%DESTDIR%\iojs.exe
    )
)

if exist "%TARGETNODEEXE%" goto gyp
if not exist "%DESTDIR%\NUL" mkdir "%DESTDIR%"
echo Downloading node.exe %2 %3...
"%NODEEXE%" %SELF%\download.js %2 %3 "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Cannot download node.exe %2 v%3
    exit /b -1
)

:gyp

echo Building edge.node %FLAVOR% for node.js %2 v%3

if "%RUNNINGIOJS%" equ "1" (
    SET GYP=%NODEDIR%\node_modules\pangyp\bin\node-gyp.js
    if not exist "%GYP%" (
        echo Cannot find pangyp at %GYP%. Make sure to install with npm install pangyp -g
        exit /b -1
    )
    "%TARGETNODEEXE%" "%GYP%" configure build --msvs_version=2013 -%FLAVOR%
) else (
    set GYP=%APPDATA%\npm\node_modules\node-gyp\bin\node-gyp.js
    if not exist "%GYP%" (
        echo Cannot find node-gyp at %GYP%. Make sure to install with npm install node-gyp -g
        exit /b -1
    )
    "%TARGETNODEEXE%" "%GYP%" configure build --msvs_version=2013 -%FLAVOR%
)

if %ERRORLEVEL% neq 0 (
    echo Error building edge.node %FLAVOR% for node.js %2 v%3
    exit /b -1
)

echo %DESTDIR%
copy /y .\build\%FLAVOR%\edge.node "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Error copying edge.node %FLAVOR% for node.js %2 v%3
    exit /b -1
)

copy /y "%DESTDIR%\..\msvcr120.dll" "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Error copying msvcr120.dll %FLAVOR% to %DESTDIR%
    exit /b -1
)

echo Success building edge.node %FLAVOR% for node.js %2 v%3
