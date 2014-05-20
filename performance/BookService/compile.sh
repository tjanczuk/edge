#!/bin/bash
mcs -sdk:4.5 Program.cs /r:System.Net.Http.dll /r:System.ServiceModel.dll /r:System.Web.Http.dll /r:System.Web.Http.SelfHost.dll
