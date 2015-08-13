set SELF=%~dp0

csc /target:library /debug /out:"%~dp0\Edge.Tests.dll" "%~dp0\tests.cs" 
if %ERRORLEVEL% NEQ 0 echo Test build failed & exit /b -1
cmd /c "cd ""%SELF%"" && dnu restore && dnu build"
if %ERRORLEVEL% NEQ 0 echo Test build failed & exit /b -1
copy "%SELF%bin\Debug\dnxcore50\test.dll" "%SELF%Edge.Tests.CoreClr.dll"