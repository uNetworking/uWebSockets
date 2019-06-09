default:
# Fuzzing is only available with clang++
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 WebSocket.cpp -o WebSocket
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Http.cpp -o Http
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Extensions.cpp -o Extensions
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Handshake.cpp -o Handshake
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 PerMessageDeflate.cpp -o PerMessageDeflate -lz