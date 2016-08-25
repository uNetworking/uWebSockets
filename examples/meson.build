echoexe = executable('echo', 'echo.cpp',
        include_directories : inc,
        link_with : uWSlib)

multiechoexe = executable('multithreaded_echo', 'multithreaded_echo.cpp',
		include_directories : inc,
		link_with : uWSlib, dependencies: [thread_dep])
