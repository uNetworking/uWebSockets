{
  'targets': [
    {
      'target_name': 'uws',
      'sources': [
        'src/uWS.cpp',
        'src/addon.cpp'
      ],
      'cflags_cc': [ '-std=c++11', '-fexceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'conditions': [
        ['OS=="darwin"', {
          'cflags_cc': [ '-stdlib=libc++' ]
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

