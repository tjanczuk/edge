@echo off
set LIBROOT=%~dp0\..\lib\native\win32
for /F %%D in ('dir /b /ad "%LIBROOT%\ia32"') do copy /y "%LIBROOT%\ia32\msvcr110.dll" "%LIBROOT%\ia32\%%D" 2>&1 > nul
for /F %%D in ('dir /b /ad "%LIBROOT%\x64"') do copy /y "%LIBROOT%\x64\msvcr110.dll" "%LIBROOT%\x64\%%D" 2>&1 > nul
node "%~dp0\checkplatform.js"