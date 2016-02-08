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
      'target_name': 'edge_coreclr',
      'win_delay_load_hook': 'false',
      'include_dirs': [
        "<!(node -e \"require('nan')\")"
      ],
      'cflags+': [
        '-DHAVE_CORECLR -std=c++11'
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DHAVE_CORECLR'
        ]
      },
      'conditions': [
        [
          'OS=="win"',
          {
            'conditions': [
              [
                '"<!(node -e "var whereis = require(\'./tools/whereis\'); console.log(whereis(\'dnx.exe\'));")"!=""',
                {
                  'sources+': [
                    'src/common/v8synchronizationcontext.cpp',
                    'src/common/edge.cpp',
                    'src/CoreCLREmbedding/coreclrembedding.cpp',
                    'src/CoreCLREmbedding/coreclrfunc.cpp',
                    'src/CoreCLREmbedding/coreclrnodejsfunc.cpp',
                    'src/CoreCLREmbedding/coreclrfuncinvokecontext.cpp',
                    'src/CoreCLREmbedding/coreclrnodejsfuncinvokecontext.cpp',
                    'src/common/utils.cpp'
                  ]
                },
                {
                  'type': 'none'
                }
              ]
            ]
          },
          {
            'conditions': [
              [
                '"<!((which dnx) || echo not_found)"!="not_found"',
                {
                  'sources+': [
                    'src/common/v8synchronizationcontext.cpp',
                    'src/common/edge.cpp',
                    'src/CoreCLREmbedding/coreclrembedding.cpp',
                    'src/CoreCLREmbedding/coreclrfunc.cpp',
                    'src/CoreCLREmbedding/coreclrnodejsfunc.cpp',
                    'src/CoreCLREmbedding/coreclrfuncinvokecontext.cpp',
                    'src/CoreCLREmbedding/coreclrnodejsfuncinvokecontext.cpp',
                    'src/common/utils.cpp'
                  ]
                },
                {
                  'type': 'none'
                }
              ]
            ]
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
              'AdditionalOptions': [
                '/wd4506',
                '/DHAVE_CORECLR',
                '/EHsc'
              ]
            },
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4248',
                'shlwapi.lib'
              ]
            }
          }
        },
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # this is out of range and will generate a warning and skip adding RuntimeLibrary property:
              'RuntimeLibrary': 3,
              # this is out of range and will generate a warning and skip adding RuntimeTypeInfo property:
              'RuntimeTypeInfo': -1,
              'BasicRuntimeChecks': -1,
              'ExceptionHandling': '0',
              'AdditionalOptions': [
                '/wd4506',
                '/DHAVE_CORECLR',
                '/EHsc'
              ]
            },
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4248',
                'shlwapi.lib'
              ]
            }
          }
        }
      }
    },
    {
      'target_name': 'edge_nativeclr',
      'win_delay_load_hook': 'false',
      'include_dirs': [
        "<!(node -e \"require('nan')\")"
      ],
      'cflags+': [
        '-DHAVE_NATIVECLR -std=c++11'
      ],
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-DHAVE_NATIVECLR'
        ]
      },
      'conditions': [
        [
          'OS=="win"',
          {
            'sources+': [
              'src/dotnet/utils.cpp',
              'src/dotnet/clrfunc.cpp',
              'src/dotnet/clrfuncinvokecontext.cpp',
              'src/dotnet/nodejsfunc.cpp',
              'src/dotnet/nodejsfuncinvokecontext.cpp',
              'src/dotnet/persistentdisposecontext.cpp',
              'src/dotnet/clrfuncreflectionwrap.cpp',
              'src/dotnet/clractioncontext.cpp',
              'src/common/v8synchronizationcontext.cpp',
              'src/common/edge.cpp'
            ]
          },
          {
            'conditions': [
              [
                '"<!((which mono) || echo not_found)"!="not_found"',
                {
                  'sources+': [
                    'src/mono/clractioncontext.cpp',
                    'src/mono/clrfunc.cpp',
                    'src/mono/clrfuncinvokecontext.cpp',
                    'src/mono/monoembedding.cpp',
                    'src/mono/task.cpp',
                    'src/mono/dictionary.cpp',
                    'src/mono/nodejsfunc.cpp',
                    'src/mono/nodejsfuncinvokecontext.cpp',
                    'src/mono/utils.cpp',
                    'src/common/utils.cpp',
                    'src/common/v8synchronizationcontext.cpp',
                    'src/common/edge.cpp'
                  ],
                  'include_dirs': [
                    '<!@(pkg-config mono-2 --cflags-only-I | sed s/-I//g)'
                  ],
                  'link_settings': {
                    'libraries': [
                      '<!@(pkg-config mono-2 --libs)'
                    ],
                  }
                },
                {
                  'type': 'none'
                }
              ]
            ]
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
              'AdditionalOptions': [
                '/clr',
                '/wd4506',
                '/DHAVE_NATIVECLR'
              ]
            },
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4248'
              ]
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
              'AdditionalOptions': [
                '/clr',
                '/wd4506',
                '/DHAVE_NATIVECLR'
              ]
            },
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4248'
              ]
            }
          }
        }
      }
    },
    {
      'target_name': 'build_managed',
      'type': 'none',
      'dependencies': [
        'edge_nativeclr',
        'edge_coreclr'
      ],
      'conditions': [
        [
          'OS=="win"',
          {
            'conditions': [
              [
                '"<!(node -e "var whereis = require(\'./tools/whereis\'); console.log(whereis(\'dnx.exe\'));")"!=""',
                {
                  'actions+': [
                    {
                      'action_name': 'compile_coreclr_embed',
                      'inputs': [
                        'src/CoreCLREmbedding/project.json'
                      ],
                      'outputs': [
                        'src/CoreCLREmbedding/bin/$(BUILDTYPE)/dnxcore50/CoreCLREmbedding.dll'
                      ],                        
                      'action': [
                        'cd "<(module_root_dir)\\src\\CoreCLREmbedding" & dnu restore & cd "<(module_root_dir)\\src\\CoreCLREmbedding" & dnu build --configuration $(Configuration) & copy "<(module_root_dir)\\src\\CoreCLREmbedding\\bin\\$(Configuration)\\dnxcore50\\CoreCLREmbedding.dll" "<(module_root_dir)\\build\\$(Configuration)"'
                      ]
                    }
                  ],
                  'copies+': [
                    {
                      'destination': '<(module_root_dir)\\build\\$(BUILDTYPE)',
                      'files': [
                        '<(module_root_dir)\\src\\CoreCLREmbedding\\bin\\$(BUILDTYPE)\\dnxcore50\\CoreCLREmbedding.dll'
                      ]
                    }
                  ]
                }
              ]
            ]
          },
          {
            'conditions': [
              [
                '"<!((which mono) || echo not_found)"!="not_found"',
                {
                  'actions+': [
                    {
                      'action_name': 'compile_mono_embed',
                      'inputs': [
                        'src/mono/*.cs'
                      ],
                      'outputs': [
                        'build/$(BUILDTYPE)/monoembedding.exe'
                      ],
                      'action': [
                        'dmcs',
                        '-sdk:4.5',
                        '-target:exe',
                        '-out:build/$(BUILDTYPE)/MonoEmbedding.exe',
                        'src/mono/*.cs',
                        'src/common/*.cs'
                      ]
                    }
                  ]
                }
              ],
              [
                '"<!((which dnx) || echo not_found)"!="not_found"',
                {
                  'actions+': [
                    {
                      'action_name': 'restore_packages',
                      'inputs': [
                        'src/CoreCLREmbedding/project.json'
                      ],
                      'outputs': [
                        'src/CoreCLREmbedding/project.lock.json'
                      ],
                      'action': [
                        'bash',
                        '-c',
                        'cd src/CoreCLREmbedding && dnu restore'
                      ]
                    },
                    {
                      'action_name': 'compile_coreclr_embed',
                      'inputs': [
                        'src/CoreCLREmbedding/*.cs',
                        'src/common/*.cs'
                      ],
                      'outputs': [
                        'src/CoreCLREmbedding/bin/$(BUILDTYPE)/dnxcore50/CoreCLREmbedding.dll'
                      ],                        
                      'action': [
                        'bash',
                        '-c',
                        'cd src/CoreCLREmbedding && dnu build --configuration $(BUILDTYPE)'
                      ]
                    }
                  ],
                  'copies+': [
                    {
                      'destination': '<(module_root_dir)/build/$(BUILDTYPE)',
                      'files': [
                        '<(module_root_dir)/src/CoreCLREmbedding/bin/$(BUILDTYPE)/dnxcore50/CoreCLREmbedding.dll',
                        '<(module_root_dir)/src/CoreCLREmbedding/project.json',
                        '<(module_root_dir)/src/CoreCLREmbedding/project.lock.json'
                      ]
                    }
                  ]
                }
              ]
            ]
          }
        ]
      ]
    }
  ]
}
