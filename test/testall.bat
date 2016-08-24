@echo off

set SELF=%~dp0
set run32=N
set run64=N

if "%1"=="" set run32=Y
if "%1"=="ia32" set run32=Y
if "%1"=="" set run64=Y
if "%1"=="x64" set run64=Y

if "%run32%"=="Y" (
	call "%SELF%\test.bat" ia32 6.4.0
	call "%SELF%\test.bat" ia32 4.1.1
	call "%SELF%\test.bat" ia32 5.1.0
	call "%SELF%\test.bat" ia32 0.12.0
	call "%SELF%\test.bat" ia32 0.8.22
	call "%SELF%\test.bat" ia32 0.10.0
)

if "%run64%"=="Y" (
	call "%SELF%\test.bat" x64 6.4.0
	call "%SELF%\test.bat" x64 4.1.1
	call "%SELF%\test.bat" x64 5.1.0
	call "%SELF%\test.bat" x64 0.12.0
	call "%SELF%\test.bat" x64 0.8.22
	call "%SELF%\test.bat" x64 0.10.0
)