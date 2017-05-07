rem usage: test.bat [ia32|x64 {version}], e.g. test.bat x64 0.10.0
@echo off
set EDGE_APP_ROOT=%~dp0\bin\Debug\netcoreapp1.0
set NODEEXE=node.exe
set EDGE_USE_CORECLR=
if "%1" neq "" if "%2" neq "" set NODEEXE=%~dp0\..\lib\native\win32\%1\%2\node.exe
echo Using node.js: %NODEEXE%
rmdir /s /q "%~dp0/bin"
rmdir /s /q "%~dp0/obj"
call "%~dp0\build.bat"
if %ERRORLEVEL% NEQ 0 exit /b -1;
pushd "%~dp0\.."
"%NODEEXE%" "%APPDATA%\npm\node_modules\mocha\bin\mocha" -R spec
set EDGE_USE_CORECLR=1
REM set EDGE_DEBUG=1
popd
rmdir /s /q "%~dp0/bin"
rmdir /s /q "%~dp0/obj"
call "%~dp0\build.bat"
if %ERRORLEVEL% NEQ 0 exit /b -1;
pushd "%~dp0\.."
"%NODEEXE%" "%APPDATA%\npm\node_modules\mocha\bin\mocha" -R spec -t 10000
set EDGE_USE_CORECLR=
set EDGE_DEBUG=
popd
echo Finished running tests using node.js: %NODEEXE%
