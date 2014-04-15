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
  'variables': {
    'mono_installation': 'C:\Mono-3.0.6'
  },
  'targets': [
    {
      'target_name': 'edge',
      'sources': [ ],
      'conditions': [
        ['RUNTIME=="mono"', {
          'sources+': [
            'src/mono/edge.h',
            'src/mono/edge.cpp',
            'src/mono/monoembed.cpp',
            'src/mono/monotask.cpp',
            'src/mono/utils.cpp',
            'src/mono/clrfunc.cpp',
            'src/mono/clrfuncinvokecontext.cpp',
            'src/mono/nodejsfunc.cpp',
            'src/mono/nodejsfuncinvokecontext.cpp',
            'src/mono/persistentdisposecontext.cpp',
            'src/mono/v8synchronizationcontext.cpp',
          ],
          'conditions': [
            ['OS=="win"', {
              'include_dirs': [
                '<(mono_installation)/include/mono-2.0',
              ],
              'link_settings': {
                'libraries': [
                  '-l<(mono_installation)/lib/mono-2.0.lib',
                ],
              },
            }, {
              'include_dirs': [
                '/Library/Frameworks/Mono.framework/Headers/mono-2.0',
              ],
              'link_settings': {
                'libraries': [
                  '/Library/Frameworks/Mono.framework/Libraries/libmono-2.0.dylib',
                ],
              },
            }]
          ]
        }, {
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
              'conditions': [
                ['RUNTIME=="mono"', 
                  {
                    'AdditionalOptions': [ '/wd4506' ] 
                  },
                  {
                    'AdditionalOptions': [ '/clr', '/wd4506' ] 
                  },
                ]
              ]
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
              'conditions': [
                ['RUNTIME=="mono"', 
                  {
                    'AdditionalOptions': [ '/wd4506' ] 
                  },
                  {
                    'AdditionalOptions': [ '/clr', '/wd4506' ] 
                  },
                ]
              ],
            },
            'VCLinkerTool': {
              'AdditionalOptions': [ '/ignore:4248' ]
            }
          }
        }
      }
    },
    {
      'target_name': 'build_managed',
      'type': 'none',
      'dependencies': [ 'edge' ],
      'actions': [
        {
          'action_name': 'compile_mono_embed',
          'inputs': [
            'src/mono/monoembedding.cs'
          ],
          'outputs': [
            'src/mono/monoembedding.exe'
          ],
          'conditions': [
            ['OS=="win"', {
              'action': ['csc', '-target:exe', '-out:../src/mono/MonoEmbedding.exe', 'src/mono/MonoEmbedding.cs']
              }, {
              'action': ['dmcs', '-sdk:4.5', '-target:exe', '-out:src/mono/MonoEmbedding.exe', 'src/mono/MonoEmbedding.cs']
              }
            ]
          ]
        }
      ]
    }
  ]
}
