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
      'include_dirs' : [
          "<!(node -e \"require('nan')\")"
      ],
      'sources': [ 
        'src/common/v8synchronizationcontext.cpp'
      ],
      'conditions': [
        ['OS=="win"'
        , {
            'sources+': [ 
              'src/dotnet/edge.cpp',
              'src/dotnet/utils.cpp', 
              'src/dotnet/clrfunc.cpp',
              'src/dotnet/clrfuncinvokecontext.cpp',
              'src/dotnet/nodejsfunc.cpp',
              'src/dotnet/nodejsfuncinvokecontext.cpp',
              'src/dotnet/persistentdisposecontext.cpp',
              'src/dotnet/clrfuncreflectionwrap.cpp',
              'src/dotnet/clractioncontext.cpp'
            ]
          }
        , {
            'sources+': [
              'src/mono/clractioncontext.cpp',
              'src/mono/clrfunc.cpp',
              'src/mono/clrfuncinvokecontext.cpp',
              'src/mono/edge.cpp',
              'src/mono/edge.h',
              'src/mono/monoembedding.cpp',
              'src/mono/task.cpp',
              'src/mono/dictionary.cpp',
              'src/mono/nodejsfunc.cpp',
              'src/mono/nodejsfuncinvokecontext.cpp',
              'src/mono/utils.cpp',
            ]
          , 'include_dirs': [
              '<!@(pkg-config mono-2 --cflags-only-I | sed s/-I//g)'
            ]
          , 'link_settings': {
              'libraries': [
                '<!@(pkg-config mono-2 --libs)'
              ],
            }
          }
        ]
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
  , {
      'target_name': 'build_managed',
      'conditions': [[ 'OS!="win"', {
        'type': 'none',
        'dependencies': [ 'edge' ],
        'actions': [
          {
            'action_name': 'compile_mono_embed',
            'inputs': [
              'src/mono/*.cs'
            ],
            'outputs': [
              'src/mono/monoembedding.exe'
            ],
            'conditions': [
              ['OS=="win"', {
                'action': ['csc', '-target:exe', '-out:build/$(BUILDTYPE)/MonoEmbedding.exe', 'src/mono/*cs']
                }, {
                'action': ['dmcs', '-sdk:4.5', '-target:exe', '-out:build/$(BUILDTYPE)/MonoEmbedding.exe', 'src/mono/*.cs']
                }
              ]
            ]
          }
        ]
      }]]
    }    
  ]
}