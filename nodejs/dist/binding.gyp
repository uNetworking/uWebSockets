{
  'targets': [
    {
      'target_name': 'uws',
      'sources': [
        'src/uWS.cpp',
        'src/addon.cpp'
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags_cc': [ '-std=c++11' ],
          'cflags_cc!': [ '-fno-exceptions', '-std=gnu++0x', '-fno-rtti' ],
          'cflags!': [ '-fno-omit-frame-pointer' ],
          'ldflags!': [ '-rdynamic' ],
          'ldflags': [ '-s' ]
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'MACOSX_DEPLOYMENT_TARGET': '10.7',
            'OTHER_CFLAGS': [
              '-std=c++11', '-stdlib=libc++', '-fexceptions'
            ]
          }
        }],
        ['OS=="win"', {
          'cflags_cc': [],
          'cflags_cc!': []
        }]
       ]
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [ 'uws' ],
      'actions': [
        {
          'action_name': 'move_lib',
          'inputs': [
            '<@(PRODUCT_DIR)/uws.node'
          ],
          'outputs': [
            'uws'
          ],
          'action': ['cp', '<@(PRODUCT_DIR)/uws.node', 'uws_<!@(node -p process.platform)_<!@(node -p process.versions.modules).node']
        }
      ]
    }
  ]
}

