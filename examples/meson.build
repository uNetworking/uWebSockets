echoexe = executable('echo', 'echo.cpp', include_directories : inc, link_with : uWS_lib,
                     dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])

multiechoexe = executable('multithreaded_echo', 'multithreaded_echo.cpp',
		         include_directories : inc, link_with : uWS_lib,
                 dependencies : [zlib_dep, ssl_dep, uv_dep, thread_dep])
