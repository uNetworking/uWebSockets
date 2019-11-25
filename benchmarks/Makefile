default:
	clang -flto -O3 -DLIBUS_USE_OPENSSL -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c broadcast_test.c -o broadcast_test -lssl -lcrypto
	clang -flto -O3 -DLIBUS_USE_OPENSSL -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c load_test.c -o load_test -lssl -lcrypto
	clang -flto -O3 -DLIBUS_USE_OPENSSL -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c scale_test.c -o scale_test -lssl -lcrypto
