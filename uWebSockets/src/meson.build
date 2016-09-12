prog_sources = [
	'EventSystem.cpp',
	'Extensions.cpp',
	'HTTPSocket.cpp',
	'Network.cpp',
	'Server.cpp',
	'UTF8.cpp',
	'WebSocket.cpp'
]

inst_headers = [
	'EventSystem.h',
	'Server.h',
	'WebSocket.h',
	'uWS.h'
]

install_headers(inst_headers, subdir: 'uWS')

uWS_lib = library('uWS', prog_sources, include_directories: inc,
		dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])
