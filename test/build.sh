#!/bin/bash

if [ -n "$(which dotnet 2>/dev/null)" ]
then
	dotnet restore
	dotnet build
	cp bin/Debug/netcoreapp1.0/test.dll Edge.Tests.CoreClr.dll
fi

if [ -n "$(which mono 2>/dev/null)" ]
then
	mcs /target:library /debug /out:Edge.Tests.dll tests.cs
fi
