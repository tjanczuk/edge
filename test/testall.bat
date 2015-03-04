@echo off

set SELF=%~dp0

call %SELF%\test.bat ia32 0.12.0
call %SELF%\test.bat ia32 0.8.22
call %SELF%\test.bat ia32 0.10.0

call %SELF%\test.bat x64 0.12.0
call %SELF%\test.bat x64 0.8.22
call %SELF%\test.bat x64 0.10.0
