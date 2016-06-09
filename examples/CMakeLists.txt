add_executable(echo echo.cpp)
target_include_directories(echo PUBLIC ../src)
target_compile_features(echo PRIVATE cxx_nullptr)
target_link_libraries (echo LINK_PUBLIC uWS)

add_executable(multithreaded_echo multithreaded_echo.cpp)
target_include_directories(multithreaded_echo PUBLIC ../src)
target_compile_features(multithreaded_echo PRIVATE cxx_nullptr)
target_link_libraries (multithreaded_echo LINK_PUBLIC uWS)

add_executable(client_echo client_echo.cpp)
target_include_directories(client_echo PUBLIC ../src)
target_compile_features(client_echo PRIVATE cxx_nullptr)
target_link_libraries (client_echo LINK_PUBLIC uWS)
