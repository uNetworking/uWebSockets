default:
	$(CXX) -std=c++17 -fsanitize=address Query.cpp -o Query
	./Query
	$(CXX) -std=c++17 -fsanitize=address ChunkedEncoding.cpp -o ChunkedEncoding
	./ChunkedEncoding
	$(CXX) -std=c++17 -fsanitize=address TopicTree.cpp -o TopicTree
	./TopicTree
	$(CXX) -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG -std=c++17 -g -fsanitize=address HttpRouter.cpp -o HttpRouter
	./HttpRouter
	$(CXX) -std=c++17 -fsanitize=address BloomFilter.cpp -o BloomFilter
	./BloomFilter
	$(CXX) -std=c++17 -fsanitize=address ExtensionsNegotiator.cpp -o ExtensionsNegotiator
	./ExtensionsNegotiator
	$(CXX) -std=c++17 -fsanitize=address HttpParser.cpp -o HttpParser
	./HttpParser

performance:
	$(CXX) -std=c++17 HttpRouter.cpp -O3 -o HttpRouter
	./HttpRouter

smoke:
	../Crc32 &
	sleep 1
	~/.deno/bin/deno run --allow-net smoke.mjs
	node smoke.mjs
	pkill Crc32

compliance:
	../EchoBody &
	sleep 1
	~/.deno/bin/deno run --allow-net ../h1spec/http_test.ts localhost 3000
	pkill EchoBody
