#ifndef ASIO_H
#define ASIO_H

#include <boost/asio.hpp>
#include <iostream>

// these should be renamed (don't add more than these, only these are used)
typedef int uv_os_sock_t;
typedef void uv_handle_t;
typedef void (*uv_close_cb)(uv_handle_t *);
static const int UV_READABLE = EPOLLIN | EPOLLHUP;
static const int UV_WRITABLE = EPOLLOUT;
static const int UV_VERSION_MINOR = 5;

struct Loop : boost::asio::io_service {

    static Loop *createLoop(bool defaultLoop = true) {
        return new Loop;
    }

    void destroy() {

    }

    void run() {
        boost::asio::io_service::run();
    }
};

struct Async {

    Async(Loop *loop) {

    }

    void start(void (*cb)(Async *)) {

    }

    void send() {

    }

    void close(uv_close_cb cb) {

    }

    void setData(void *data) {

    }

    void *getData() {
        return nullptr;
    }
};

struct Timer {

    Timer(Loop *loop) {

    }

    void start(void (*cb)(Timer *), int first, int repeat) {

    }

    void setData(void *data) {

    }

    void *getData() {
        return nullptr;
    }

    void stop() {

    }

    void close(uv_close_cb cb) {

    }
};

struct Poll {
    boost::asio::ip::tcp::socket *socket;
    void *data;
    void (*cb)(Poll *p, int status, int events);

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        socket = new boost::asio::ip::tcp::socket(*loop);
        socket->assign(boost::asio::ip::tcp::v4(), fd);
        socket->non_blocking(true);
    }

    Poll() {

    }

    ~Poll() {
    }

    void setData(void *data) {
        this->data = data;
    }

    bool isClosing() {
        return 0;
    }

    uv_os_sock_t getFd() {
        return socket->native_handle();
    }

    void *getData() {
        return data;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        std::cout << "Poll::setCb" << std::endl;
        this->cb = cb;
    }

    void read_handler(boost::system::error_code ec, std::size_t) {
        socket->async_read_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {

            cb(this, ec.value(), UV_READABLE);
            read_handler(ec, 0);
        });
    }

    void write_handler(boost::system::error_code ec, std::size_t) {
        socket->async_read_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {
            cb(this, ec.value(), UV_WRITABLE);
            write_handler(ec, 0);
        });
    }

    void start(int events) {
        std::cout << "Poll::start" << std::endl;

        if (events & UV_READABLE) {
            socket->async_read_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {
                read_handler(ec, 0);
            });
        }

        if (events & UV_WRITABLE) {
            socket->async_write_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {
                write_handler(ec, 0);
            });
        }
    }

    void change(int events) {
        std::cout << "Poll::change" << std::endl;

        socket->cancel();
        start(events);
    }

    void stop() {
        std::cout << "Poll::stop" << std::endl;
        socket->cancel();
    }

    void close(uv_close_cb cb) {

    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) cb;
    }

    Loop *getLoop() {
        return (Loop *) &socket->get_io_service();
    }
};

#endif // ASIO_H
