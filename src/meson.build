prog_sources = [
	'EventSystem.cpp',
	'Extensions.cpp',
	'HTTPSocket.cpp',
	'Network.cpp',
	'Server.cpp',
	'UTF8.cpp',
	'WebSocket.cpp'
]

uWSlib = library('uWS', prog_sources, include_directories: inc,
		dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])
