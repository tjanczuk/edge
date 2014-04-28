Foundations:

101_hello_lambda.js - prescriptive interface pattern
102_hello_function.js - multiline function comment
103_hello_file.js - separate file
104_add7_class.js - entire class instead of lambda
105_add7_dll.js - pre-compiled DLL
    On Windows: 
    	csc.exe /target:library /debug Sample105.cs
    On Mono (MacOS, Linux):
        mcs -sdk:4.5 Sample105.cs -target:library
	start repl, attach VS, call func, show debugging
106_marshal_v82cls.js - data from V8 to CLR
107_marshal_clr2v8.js - data from CLR to V8
108_func.js - marshal func from v8 to CLR and call node from .NET
109_sync.js - async and sync calling conventions

Scenarios:

201_worker.js - CPU bound operation
202_sql.js - access SQL with async ADO.NET
	set OWIN_SQL_CONNECTION_STRING=Data Source=localhost;Initial Catalog=Northwind;Integrated Security=True
	reference assemblies
206_registery - read/write registery operations
