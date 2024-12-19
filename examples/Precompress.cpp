#include "App.h"

int main() {
    /* ws->getUserData returns one of these */
    struct PerSocketData {
        /* Fill with user data */
    };

	/* Keeping track of last precompressed message both in original and compressed format */
	std::string originalMessage;
	std::string compressedMessage;
	std::mutex m;

	/* For demo, we create a thread that will update the precompressed message every second */
	std::thread t2([&originalMessage, &compressedMessage, &m]() {
		uWS::ZlibContext zlibContext;
		uWS::DeflationStream compressor(uWS::DEDICATED_COMPRESSOR);
		int counter = 0;

		while (true) {
			counter++;

			m.lock();
			originalMessage = "Hello you are looking at message number " + std::to_string(counter) + " and this text should be precompressed";		
			compressedMessage = compressor.deflate(&zlibContext, {originalMessage.data(), originalMessage.length()}, true);
			m.unlock();

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

	});

    uWS::App().ws<PerSocketData>("/*", {
        /* You must only use SHARED_COMPRESSOR with precompression (can't use dedicated_compressor) */
        .compression = uWS::CompressOptions(uWS::SHARED_COMPRESSOR | uWS::DEDICATED_DECOMPRESSOR),
        /* Handlers */
        .upgrade = nullptr,
        .open = [](auto */*ws*/) {
            /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */

        },
        .message = [&originalMessage, &compressedMessage, &m](auto *ws, std::string_view message, uWS::OpCode opCode) {

			/* First respond by echoing what they send us, without compression */
			ws->send(message, opCode, false);

			/* This should be wrapped up into ws->sendPrepared(PreparedMessage) in the future, experimental for now */
			m.lock();
			if (ws->hasNegotiatedCompression() && compressedMessage.length() < originalMessage.length()) {
				std::cout << "Responding with precompressed message saving " << (originalMessage.length() - compressedMessage.length()) << " bytes" << std::endl;
				ws->send({compressedMessage.data(), compressedMessage.length()}, uWS::OpCode::TEXT, uWS::CompressFlags::ALREADY_COMPRESSED);
			} else {
				ws->send({originalMessage.data(), originalMessage.length()}, uWS::OpCode::TEXT);
			}
			m.unlock();
        },
        .dropped = [](auto */*ws*/, std::string_view /*message*/, uWS::OpCode /*opCode*/) {
            /* A message was dropped due to set maxBackpressure and closeOnBackpressureLimit limit */
        },
        .drain = [](auto */*ws*/) {
            /* Check ws->getBufferedAmount() here */
        },
        .ping = [](auto */*ws*/, std::string_view) {
            /* Not implemented yet */
        },
        .pong = [](auto */*ws*/, std::string_view) {
            /* Not implemented yet */
        },
        .close = [](auto */*ws*/, int /*code*/, std::string_view /*message*/) {
            /* You may access ws->getUserData() here */
        }
    }).listen(9001, [&t2](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}
