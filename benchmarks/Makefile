default:
	g++ -flto -march=native parser.cpp -O3 -I../uSockets/src -o parser
	clang -flto -O3 -DLIBUS_NO_SSL -DLIBUS_USE_IO_URING -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c ../uSockets/src/io_uring/*.c broadcast_test.c load_test.c scale_test.c -c
	clang++ -flto -O3 -DLIBUS_NO_SSL -I../uSockets/src ../uSockets/src/crypto/*.cpp -c -std=c++17
	clang++ -flto -O3 -DLIBUS_NO_SSL `ls *.o | grep -Ev "load_test|scale_test"` /usr/lib/liburing.a -o broadcast_test
	clang++ -flto -O3 -DLIBUS_NO_SSL `ls *.o | grep -Ev "broadcast_test|scale_test"` /usr/lib/liburing.a -o load_test
	clang++ -flto -O3 -DLIBUS_NO_SSL `ls *.o | grep -Ev "broadcast_test|load_test"` /usr/lib/liburing.a -o scale_test

