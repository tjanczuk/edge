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
            'src/edgejavascriptconverter.cpp',
            'src/v8synchronizationcontext.cpp',
            'src/clrfuncreflectionwrap.cpp'
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