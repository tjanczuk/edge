@echo off
set SELF=%~dp0
if "%1" equ "" (
    echo Usage: build_double.bat {node_version}
    echo e.g. build_double.bat 5.9.1
    exit /b -1
)

mkdir "%SELF%\build\nuget\content\edge" > nul 2>&1
mkdir "%SELF%\build\nuget\lib\net40" > nul 2>&1
mkdir "%SELF%\build\nuget\lib\netstandard1.6" > nul 2>&1
mkdir "%SELF%\build\nuget\tools" > nul 2>&1
mkdir "%SELF%\..\src\double\Edge.js\bin\Release\net40" > nul 2>&1

if not exist "%SELF%\build\download.exe" (
	csc /out:"%SELF%\build\download.exe" "%SELF%\download.cs"
)

rem if not exist "%SELF%\build\unzip.exe" (
rem 	csc /out:"%SELF%\build\unzip.exe" /r:System.IO.Compression.FileSystem.dll "%SELF%\unzip.cs"
rem )

if not exist "%SELF%\build\repl.exe" (
	csc /out:"%SELF%\build\repl.exe" "%SELF%\repl.cs"
)

if not exist "%SELF%\build\nuget.exe" (
	"%SELF%\build\download.exe" http://nuget.org/nuget.exe "%SELF%\build\nuget.exe"
)

if not exist "%SELF%\build\%1.zip" (
	"%SELF%\build\download.exe" https://github.com/nodejs/node/archive/v%1.zip "%SELF%\build\%1.zip"
)

if not exist "%SELF%\build\node-%1" (
	rem "%SELF%\build\unzip.exe" "%SELF%\build\%1.zip" "%SELF%\build"
	pushd "%SELF%\build\"
	cscript //B ..\unzip.vbs %1.zip
	popd
)

call :build_node %1 x86
if %ERRORLEVEL% neq 0 exit /b -1
call :build_node %1 x64
if %ERRORLEVEL% neq 0 exit /b -1

if not exist "%SELF%\build\node-%1-x86\node.exe" (
	"%SELF%\build\download.exe" http://nodejs.org/dist/v%1/win-x86/node.exe "%SELF%\build\node-%1-x86\node.exe"
)

if not exist "%SELF%\build\node-%1-x64\node.exe" (
	"%SELF%\build\download.exe" http://nodejs.org/dist/v%1/win-x64/node.exe "%SELF%\build\node-%1-x64\node.exe"
)

call :build_edge %1 x86
if %ERRORLEVEL% neq 0 exit /b -1
call :build_edge %1 x64
if %ERRORLEVEL% neq 0 exit /b -1

csc /out:"%SELF%\..\src\double\Edge.js\bin\Release\net40\EdgeJs.dll" /target:library "%SELF%\..\src\double\Edge.js\dotnet\EdgeJs.cs"
if %ERRORLEVEL% neq 0 exit /b -1

cd "%SELF%\..\src\double\Edge.js"
dotnet restore
if %ERRORLEVEL% neq 0 exit /b -1
dotnet build --configuration Release --framework netstandard1.6
if %ERRORLEVEL% neq 0 exit /b -1
dotnet pack --configuration Release --no-build

if %ERRORLEVEL% neq 0 (
	echo Failure building Nuget package
	cd "%SELF%"
	exit /b -1
)

cd "%SELF%"
copy /y "%SELF%\..\src\double\Edge.js\bin\Release\*.nupkg" "%SELF%\build\nuget"
echo SUCCESS. Nuget package at %SELF%\build\nuget

exit /b 0

:build_edge

rem takes 2 parameters: 1 - node version, 2 - x86 or x64

if exist "%SELF%\build\nuget\content\edge\%2\edge_nativeclr.node" exit /b 0

set NODEEXE=%SELF%\build\node-%1-%2\node.exe
set GYP=%APPDATA%\npm\node_modules\node-gyp\bin\node-gyp.js

pushd "%SELF%\.."

"%NODEEXE%" "%GYP%" configure --msvs_version=2013
"%SELF%\build\repl.exe" ./build/edge_nativeclr.vcxproj "%USERPROFILE%\.node-gyp\%1\$(Configuration)\node.lib" "%SELF%\build\node-%1-%2\node.lib"
"%NODEEXE%" "%GYP%" build
mkdir "%SELF%\build\nuget\content\edge\%2" > nul 2>&1
copy /y build\release\edge_nativeclr.node "%SELF%\build\nuget\content\edge\%2"
copy /y "%SELF%\build\node-%1-%2\node.dll" "%SELF%\build\nuget\content\edge\%2"

popd

exit /b 0

:build_node

rem takes 2 parameters: 1 - node version, 2 - x86 or x64

if exist "%SELF%\build\node-%1-%2\node.lib" exit /b 0

pushd "%SELF%\build\node-%1"
rmdir /s /q Release

..\repl.exe node.gyp "'executable'" "'shared_library'"
if %ERRORLEVEL% neq 0 (
    echo Cannot update node.gyp 
    popd
    exit /b -1
)

call vcbuild.bat build release %2
mkdir "%SELF%\build\node-%1-%2"
copy /y .\Release\node.* "%SELF%\build\node-%1-%2"
echo Finished building Node shared library %1

popd
exit /b 0