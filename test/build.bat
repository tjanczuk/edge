csc /target:library /debug /out:"%~dp0\Edge.Tests.dll" "%~dp0\tests.cs" 
if %ERRORLEVEL% NEQ 0 echo Test build failed & exit /b -1
