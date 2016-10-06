project('uWebSockets', 'cpp', default_options : ['cpp_std=c++11'])

inc = include_directories('src')

thread_dep = dependency('threads')
zlib_dep = dependency('zlib', version : '>=1.0')
ssl_dep = dependency('openssl', version : '>=1.0.0')
uv_dep = dependency('libuv', version : '>=1.0')


prog_sources = [
	'src/Extensions.cpp',
	'src/Group.cpp',
	'src/HTTPSocket.cpp',
	'src/Hub.cpp',
	'src/Networking.cpp',
	'src/Node.cpp',
	'src/Socket.cpp',
	'src/WebSocket.cpp',
	'src/WebSocketImpl.cpp'
]

inst_headers = [
	'src/Extensions.h',
	'src/WebSocketProtocol.h',
	'src/Networking.h',
	'src/WebSocket.h',
	'src/Hub.h',
	'src/Group.h',
	'src/Node.h',
	'src/Socket.h',
	'src/HTTPSocket.h',
	'src/uWS.h'
]

install_headers(inst_headers, subdir: 'uWS')

uWS_lib = library('uWS', prog_sources, include_directories: inc,
		    dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])

uWS_dep = declare_dependency(include_directories:inc, link_with: uWS_lib,
            dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])

subdir('examples')
subdir('tests')
