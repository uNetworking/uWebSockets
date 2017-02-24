#ifndef ASIO_H
#define ASIO_H

#include <boost/asio.hpp>
#include <iostream>
#include <thread>

// these should be renamed (don't add more than these, only these are used)
typedef boost::asio::ip::tcp::socket::native_type uv_os_sock_t;
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
        std::cout << "Destroying loop of thread: " << std::this_thread::get_id() << std::endl;
        delete this;
    }

    void run() {
        boost::asio::io_service::run();
    }
};

struct Timer {
    boost::asio::deadline_timer asio_timer;
    void *data;

    Timer(Loop *loop) : asio_timer(*loop) {

    }

    void start(void (*cb)(Timer *), int first, int repeat) {
        asio_timer.expires_from_now(boost::posix_time::milliseconds(first));
        asio_timer.async_wait([this, cb, repeat](const boost::system::error_code &ec) {
            if (ec != boost::asio::error::operation_aborted) {
                if (repeat) {
                    start(cb, repeat, repeat);
                }
                cb(this);
            }
        });
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }

    void stop() {
        asio_timer.cancel();
    }

    void close(uv_close_cb cb) {
        asio_timer.get_io_service().post([this]() {
            delete this;
        });
    }
};

struct Async {
    Loop *loop;
    void (*cb)(Async *);
    void *data;

    // used to hold loop open!
    Timer t;

    Async(Loop *loop) : loop(loop), t(loop) {
        std::cout << "Creating from thread: " << std::this_thread::get_id() << std::endl;
    }

    void start(void (*cb)(Async *)) {
        this->cb = cb;

        // hinder loop from closing!
        t.start([](Timer *t) {}, 60000, 60000);
    }

    void send() {
        std::cout << "Sending from thread: " << std::this_thread::get_id() << std::endl;
        //loop->post([this]() {
            std::cout << "Handling from thread: " << std::this_thread::get_id() << std::endl;
            cb(this);
        //});
    }

    void close(uv_close_cb cb) {
        t.stop();

        /*loop->post([this]() {
            delete this;
        });*/
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }
};

struct Poll {
    boost::asio::ip::tcp::socket *socket;
    void *data;
    void (*cb)(Poll *p, int status, int events);
    Loop *loop;
    boost::asio::ip::tcp::socket::native_type fd;

    Poll(Loop *loop, uv_os_sock_t fd) {
        init(loop, fd);
    }

    void init(Loop *loop, uv_os_sock_t fd) {
        this->fd = fd;
        this->loop = loop;
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
        return !socket;
    }

    boost::asio::ip::tcp::socket::native_type getFd() {
        return fd;//socket->native_handle();
    }

    void *getData() {
        return data;
    }

    void setCb(void (*cb)(Poll *p, int status, int events)) {
        this->cb = cb;
    }

    void start(int events) {
        if (events & UV_READABLE) {
            socket->async_read_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {
                if (ec != boost::asio::error::operation_aborted) {
                    start(UV_READABLE);
                    cb(this, ec ? -1 : 0, UV_READABLE);
                }
            });
        }

        if (events & UV_WRITABLE) {
            socket->async_write_some(boost::asio::null_buffers(), [this](boost::system::error_code ec, std::size_t) {
                if (ec != boost::asio::error::operation_aborted) {
                    start(UV_WRITABLE);
                    cb(this, ec ? -1 : 0, UV_WRITABLE);
                }
            });
        }
    }

    void change(int events) {
        socket->cancel();
        start(events);
    }

    void stop() {
        socket->cancel();
    }

    // delayed suicide
    void close(uv_close_cb cb) {
        socket->get_io_service().post([this]() {
            delete this;
        });
        delete socket;
        socket = nullptr;
    }

    void (*getPollCb())(Poll *, int, int) {
        return (void (*)(Poll *, int, int)) cb;
    }

    Loop *getLoop() {
        return loop;//(Loop *) &socket->get_io_service();
    }
};

#endif // ASIO_H
