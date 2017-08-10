@echo off

set SELF=%~dp0
set run32=N
set run64=N

if "%1"=="" set run32=Y
if "%1"=="ia32" set run32=Y
if "%1"=="" set run64=Y
if "%1"=="x64" set run64=Y

if "%run32%"=="Y" (
	call "%SELF%\test.bat" ia32 8.2.1
	call "%SELF%\test.bat" ia32 7.10.1
	call "%SELF%\test.bat" ia32 6.11.2
)

if "%run64%"=="Y" (
	call "%SELF%\test.bat" x64 8.2.1
	call "%SELF%\test.bat" x64 7.10.1
	call "%SELF%\test.bat" x64 6.11.2
)