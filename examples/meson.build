echoexe = executable('echo', 'echo.cpp',
        include_directories : inc,
        link_with : uWS_lib)

multiechoexe = executable('multithreaded_echo', 'multithreaded_echo.cpp',
		include_directories : inc,
		link_with : uWS_lib, dependencies: [thread_dep])
