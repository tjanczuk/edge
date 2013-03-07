@echo off
call %~dp0\build.bat
if %ERRORLEVEL% NEQ 0 exit /b -1;
pushd %~dp0\..
call mocha -R spec
popd
