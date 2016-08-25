project('µWebSockets', 'cpp', default_options : ['cpp_std=c++11'])

inc = include_directories('src')

thread_dep = dependency('threads')
zlib_dep = dependency('zlib')
ssl_dep = dependency('openssl')
uv_dep = dependency('libuv')

subdir('src')
subdir('examples')
