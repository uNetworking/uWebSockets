project('uWebSockets', 'cpp', default_options : ['cpp_std=c++11'])

inc = include_directories('src')

thread_dep = dependency('threads')
zlib_dep = dependency('zlib', version : '>=1.0')
ssl_dep = dependency('openssl', version : '>=1.0.0')
uv_dep = dependency('libuv', version : '>=1.0')

subdir('src')
subdir('examples')
