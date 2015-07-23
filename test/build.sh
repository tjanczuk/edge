#!/bin/bash

if [ -n "$(which dnx 2>/dev/null)" ]
then
	dnu restore
	dnu build
	cp bin/Debug/dnxcore50/test.dll Edge.Tests.CoreClr.dll
fi

if [ -n "$(which mono 2>/dev/null)" ]
then
	mcs /target:library /debug /out:Edge.Tests.dll tests.cs
fi
