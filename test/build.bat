set SELF=%~dp0
ver > nul

IF "%EDGE_USE_CORECLR%"=="1" (
	cmd /c "cd ""%SELF%"" && dnu restore"
	
	IF %ERRORLEVEL% NEQ 0 (
		ECHO Test restore failed
		EXIT /b -1
	)
) ELSE (
	csc /target:library /debug /out:"%SELF%\Edge.Tests.dll" "%SELF%\tests.cs" 
	
	IF %ERRORLEVEL% NEQ 0 (
		ECHO Test build failed
		EXIT /b -1
	)
)