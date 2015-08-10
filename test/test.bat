rem usage: test.bat [ia32|x64 {version}], e.g. test.bat x64 0.10.0
@echo off
set NODEEXE=node.exe
if "%1" neq "" if "%2" neq "" set NODEEXE=%~dp0\..\lib\native\win32\%1\%2\node.exe
echo Using node.js: %NODEEXE%
call "%~dp0\build.bat"
if %ERRORLEVEL% NEQ 0 exit /b -1;
pushd "%~dp0\.."
"%NODEEXE%" "%APPDATA%\npm\node_modules\mocha\bin\mocha" -R spec
set EDGE_USE_CORECLR=1
set EDGE_DEBUG=1
"%NODEEXE%" "%APPDATA%\npm\node_modules\mocha\bin\mocha" -R spec
set EDGE_USE_CORECLR=
set EDGE_DEBUG=
popd
echo Finished running tests using node.js: %NODEEXE%
