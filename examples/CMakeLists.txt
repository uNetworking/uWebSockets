add_executable(echo echo.cpp)
target_include_directories(echo PUBLIC ../src)
target_link_libraries (echo LINK_PUBLIC uWS)

add_executable(multithreaded_echo multithreaded_echo.cpp)
target_include_directories(multithreaded_echo PUBLIC ../src)
target_link_libraries (multithreaded_echo LINK_PUBLIC uWS)
