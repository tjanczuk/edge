{
  'targets': [
    {
      'target_name': 'owin',
      'sources': [ ],
      'conditions': [
      	['OS=="win"', {
      	  'sources+': [ 'src/owin.cpp' ]
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
              'AdditionalOptions': [ '/clr' ] 
            }
          }
        }
      }
    }
  ]
}