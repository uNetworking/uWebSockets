default:
	clang -flto -O3 -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c load_test.c -o load_test -lssl -lcrypto
	clang -flto -O3 -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c scale_test.c -o scale_test -lssl -lcrypto
