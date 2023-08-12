#include "App.h"
#include "HttpResponse.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <stdint.h>
#include <stdlib.h>
#include <utility> // std::pair

/*
A simple HTTP file server, using synchronous file
transfer. Good for a amall project with a few clients, 
not ideal for higher traffic servers because the 
process hangs while a file is delivered.
*/

#define SERVER_IP "192.168.0.1"
#define SERVER_PORT 8000

#define SEND_BLOCK_SIZE_BYTES 4096

template <bool SSL>
class HTTPFileStream {
    private:
        uWS::HttpResponse<SSL> * res;

        int file_size = 0;
        std::ifstream file;
        char * buf;

        std::pair<bool, bool> send_block(size_t offset, int64_t &actually_read) {
            file.seekg(offset);
            file.read(buf, SEND_BLOCK_SIZE_BYTES); //sets failbit on files shorter than SEND_BLOCK_SIZE_BYTES, reaches eof early
            actually_read = (file.tellg() == -1 ? file_size : (int64_t)file.tellg()) - offset; //file.tellg() returns -1 if you tried to read too many chars from the file

            actually_read = actually_read > 0 ? actually_read : 0; // truncate anything < 0

            return res->tryEnd(std::string_view( buf, actually_read ), file_size);
        }

        void send_stream() {
            bool retry;
            bool completed;
            do {
                int64_t actually_read;
                auto try_end_result = send_block(res->getWriteOffset(), actually_read);
                retry = try_end_result.first && !try_end_result.second; // ok && !completed
                completed = try_end_result.second;
            } while(retry);

            if(completed) {
                delete this;
            }
        }

        /**
         * @brief Construct a new HTTPFileStream object, start transferring file 
         */
        HTTPFileStream(uWS::HttpResponse<SSL> * res, std::string relative_path) {
            this->res = res;
            buf = (char *)malloc(SEND_BLOCK_SIZE_BYTES);

            res->writeStatus(uWS::HTTP_200_OK);

            file_size = std::filesystem::file_size(HTTPFileStream::web_root + relative_path);
            file.open(HTTPFileStream::web_root + relative_path, std::ios_base::in | std::ios_base::binary);

            //Pass handlers for later
            res->onWritable([this](int offset) {
                this->send_stream();
                return true;
            })->onAborted([this, relative_path]() {
                std::cout << "HTTP file stream aborted. File: " << relative_path << std::endl;
                this->res->close();
                delete this;
            });

            send_stream(); //start sending -- if tryEnd() is never called, onWritable()'s callback will never be called
        }

        ~HTTPFileStream() {
            file.close();
            free(buf);
        } 

    public:
        static std::string web_root;

        /**
         * @brief Stream a file over HTTP. All paths must start with a '/', 
         * treating web_root as the root of your page. i.e. if you wanted an 
         * image from web_root/images/foo.png, you would pass '/images/foo.png'
         * 
         * @return 0 on success, -1 if file not found.
        */
        static int stream_file(uWS::HttpResponse<SSL> * res, std::string relative_path) {
            if(!std::filesystem::exists(HTTPFileStream::web_root + relative_path)) return -1;

            (new HTTPFileStream<SSL>(res, relative_path));
            return 0;
        }
};

template <bool SSL>
std::string HTTPFileStream<SSL>::web_root = "/path/to/web/files";


int main() {
	uWS::App().get("/*", [](auto *res, auto * req) {
	    std::cout << "Serving file " << req->getUrl() << "\n";

        std::string path(req->getUrl());
        std::string ext = "";
        if(path.find('.') != std::string::npos) {
            ext = path.substr(path.find_last_of('.'));
        }

        if(path.at(0) != '/') {
            path.insert(0, "/"); //HTTPFileStream parameter convention -- every path starts with "/" (i.e. "/images/foo.png")
        }

        if(path.at(path.length() - 1) == '/') { //if no file specified, assume index.html
            res->writeHeader("Content-Type", "text/html");
            path += "index.html";
        }
        else if(ext == ".css") {
            res->writeHeader("Content-Type", "text/css");
        }
        else if(ext == ".js") {
            res->writeHeader("Content-Type", "text/javascript");
        }
        else if(ext == ".jpeg" || ext == ".jpg") {
            res->writeHeader("Content-Type", "image/jpeg");
        }
        else if(ext == ".png") {
            res->writeHeader("Content-Type", "image/png");
        }
        else if(ext == ".ico") {
            res->writeHeader("Content-Type", "image/vnd.microsoft.icon");
        }
        else {
            res->writeHeader("Content-Type", "text/plain");
        }

        if(HTTPFileStream<false>::stream_file(res, path) == -1) {
            res->writeStatus("404 not found.");
            res->end("File not found.");
        }
	}).listen(SERVER_IP, SERVER_PORT, [](auto *listen_socket) {
	    if (listen_socket) {
			std::cout << "Listening on port " << SERVER_PORT << std::endl;
	    }
	}).run();

	std::cout << "Failed to listen on port 3000" << std::endl;
}
