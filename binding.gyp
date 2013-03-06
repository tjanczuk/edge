{
  'targets': [
    {
      'target_name': 'owin',
      'sources': [ ],
      'conditions': [
      	['OS=="win"', {
      	  'sources+': [ 
            'src/owin.cpp', 
            'src/utils.cpp', 
            'src/clrfunc.cpp',
            'src/clrfuncinvokecontext.cpp',
            'src/nodejsfunc.cpp',
            'src/nodejsfuncinvokecontext.cpp'
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
              'AdditionalOptions': [ '/clr' ] 
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
              'AdditionalOptions': [ '/clr' ] 
            }
          }
        }
      }
    }
  ]
}