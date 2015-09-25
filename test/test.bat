rem usage: test.bat [ia32|x64 {version}], e.g. test.bat x64 0.10.0
@echo off
set NODEEXE=node.exe
if "%1" neq "" if "%2" neq "" (
	for /F "tokens=1 delims=." %%a in ("%2%") do (
	    if "%%a" equ "0" (
	        set NODEEXE=%~dp0\..\lib\native\win32\%1\%2\node.exe
	    ) else (
	        set NODEEXE=%~dp0\..\lib\native\win32\%1\%2\iojs.exe
	    )
	)
)
echo Using node.js: %NODEEXE%
call "%~dp0\build.bat"
if %ERRORLEVEL% NEQ 0 exit /b -1;
pushd "%~dp0\.."
WHERE iojs
if %ERRORLEVEL% NEQ 0 (
	set RUNNINGIOJS=0
    for %%i in (node.exe) do set INSTALLEDNODEEXE=%%~$PATH:i
    set MOCHABIN=%APPDATA%\npm\node_modules\mocha\bin\mocha
) else (
	set RUNNINGIOJS=1
    for %%i in (iojs.exe) do set INSTALLEDNODEEXE=%%~$PATH:i
)
for %%i in ("%INSTALLEDNODEEXE%") do set INSTALLEDNODEDIR=%%~dpi
if "%RUNNINGIOJS%" equ "1" (
    set MOCHABIN=%INSTALLEDNODEDIR%\node_modules\mocha\bin\mocha
) else (
    set MOCHABIN=%APPDATA%\npm\node_modules\mocha\bin\mocha
)
"%NODEEXE%" "%MOCHABIN%" -R spec
popd
echo Finished running tests using node.js: %NODEEXE%
