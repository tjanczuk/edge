@echo off
set SELF=%~dp0
if "%1" equ "" (
    echo Usage: build.bat debug^|release "{version} {version}" ...
    echo e.g. build.bat release "0.8.22 0.10.0"
    echo      or
    echo      build.bat release
    echo                ^(builds for the current node.exe release^)
    exit /b -1
)

SET FLAVOR=%1
shift
if "%FLAVOR%" equ "" set FLAVOR=release
for %%i in (node.exe) do set NODEEXE=%%~$PATH:i
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
if "%VERSIONS%" equ "" set VERSIONS=CURRENT_INSTALL
pushd %SELF%\..
for %%V in (%VERSIONS%) do call :build ia32 x86 %%V
for %%V in (%VERSIONS%) do call :build x64 x64 %%V
popd

exit /b 0

:build

set VERSION=%3
set CURRENT_INSTALL=
if "%VERSION%" == "CURRENT_INSTALL" (
    set CURRENT_INSTALL=true
    for /f "Tokens=*" %%i in ('node -pe "process.jsEngine || 'v8'"') do set NODEENGINE=%%i
    for /f "Tokens=*" %%i in ('node -pe "process.versions.node"') do set VERSION=%%i
)

if "%CURRENT_INSTALL%" == "" (
    set NODERELEASE=v%VERSION%
    set NODEEXE=%DESTDIR%\node.exe
    set GYP=%APPDATA%\npm\node_modules\node-gyp\bin\node-gyp.js
) else (
    set NODERELEASE=node-%NODEENGINE%-%VERSION%
    REM NODEEXE remains the current node.exe path unchanged. GYP uses the current node.exe install
    set GYP=%NODEDIR%\node_modules\npm\node_modules\node-gyp\bin\node-gyp.js
)

set DESTDIR=%DESTDIRROOT%\%1\%NODERELEASE%
if not exist "%DESTDIR%\NUL" mkdir "%DESTDIR%"

if exist "%NODEEXE%" goto gyp
echo Downloading node.exe %2 %VERSION%...
node "%SELF%\download.js" %2 %VERSION% "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Cannot download node.exe %2 %NODERELEASE%
    exit /b -1
)

:gyp

echo Building edge.node %FLAVOR% for node.js %2 %NODERELEASE%
if not exist "%GYP%" (
    echo Cannot find node-gyp at %GYP%. Make sure to install with npm install node-gyp -g
    exit /b -1
)

if exist ".\src\CoreCLREmbedding\bin" rmdir /s /q ".\src\CoreCLREmbedding\bin"

"%NODEEXE%" "%GYP%" configure build --msvs_version=2013 -%FLAVOR%
if %ERRORLEVEL% neq 0 (
    echo Error building edge.node %FLAVOR% for node.js %2 %NODERELEASE%
    exit /b -1
)

echo %DESTDIR%
copy /y .\build\%FLAVOR%\edge_*.node "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Error copying edge.node %FLAVOR% for node.js %2 %NODERELEASE%
    exit /b -1
)

if exist ".\build\%FLAVOR%\CoreCLREmbedding.dll" (
    copy /y .\build\%FLAVOR%\CoreCLREmbedding.dll "%DESTDIR%"
    if %ERRORLEVEL% neq 0 (
        echo Error copying CoreCLREmbedding.dll %FLAVOR% for node.js %2 %NODERELEASE%
        exit /b -1
    )
)

copy /y "%DESTDIR%\..\msvcr120.dll" "%DESTDIR%"
if %ERRORLEVEL% neq 0 (
    echo Error copying msvcr120.dll %FLAVOR% to %DESTDIR%
    exit /b -1
)

echo Success building edge.node %FLAVOR% for node.js %2 %NODERELEASE%
