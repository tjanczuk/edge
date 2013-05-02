##
# Portions Copyright (c) Microsoft Corporation. All rights reserved. 
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#  http://www.apache.org/licenses/LICENSE-2.0  
#
# THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
# OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
# ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR 
# PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
#
# See the Apache Version 2.0 License for specific language governing 
# permissions and limitations under the License.
##
{
  'targets': [
    {
      'target_name': 'edge',
      'sources': [ ],
      'conditions': [
      	['OS=="win"', {
      	  'sources+': [ 
            'src/edge.cpp', 
            'src/utils.cpp', 
            'src/clrfunc.cpp',
            'src/clrfuncinvokecontext.cpp',
            'src/nodejsfunc.cpp',
            'src/nodejsfuncinvokecontext.cpp',
            'src/persistentdisposecontext.cpp',
            'src/v8synchronizationcontext.cpp',
            'src/clrfuncreflectionwrap.cpp',
            'src/clractioncontext.cpp'
          ]
      	}]
      ],
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # this is out of range and will generate a warning and skip adding RuntimeLibrary property:
              'RuntimeLibrary': -1, 
              # this is out of range and will generate a warning and skip adding RuntimeTypeInfo property:
              'RuntimeTypeInfo': -1, 
              'BasicRuntimeChecks': -1,
              'ExceptionHandling': '0',
              'AdditionalOptions': [ '/clr', '/wd4506' ] 
            },
            'VCLinkerTool': {
              'AdditionalOptions': [ '/ignore:4248' ]
            }
          }
        },
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # this is out of range and will generate a warning and skip adding RuntimeLibrary property:
              'RuntimeLibrary': -1, 
              # this is out of range and will generate a warning and skip adding RuntimeTypeInfo property:
              'RuntimeTypeInfo': -1, 
              'BasicRuntimeChecks': -1,
              'ExceptionHandling': '0',
              'AdditionalOptions': [ '/clr', '/wd4506' ] 
            },
            'VCLinkerTool': {
              'AdditionalOptions': [ '/ignore:4248' ]
            }
          }
        }
      }
    }
  ]
}