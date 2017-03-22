#ifndef ASIO_H
#define ASIO_H

#include <boost/asio.hpp>

typedef boost::asio::ip::tcp::socket::native_type uv_os_sock_t;
static const int UV_READABLE = 1;
static const int UV_WRITABLE = 2;

struct Loop : boost::asio::io_service {

    static Loop *createLoop(bool defaultLoop = true) {
        return new Loop;
    }

    void destroy() {
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

    void close() {
        asio_timer.get_io_service().post([this]() {
            delete this;
        });
    }
};

struct Async {
    Loop *loop;
    void (*cb)(Async *);
    void *data;

    boost::asio::io_service::work asio_work;

    Async(Loop *loop) : loop(loop), asio_work(*loop) {
    }

    void start(void (*cb)(Async *)) {
        this->cb = cb;
    }

    void send() {
        loop->post([this]() {
            cb(this);
        });
    }

    void close() {
        loop->post([this]() {
            delete this;
        });
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }
};

struct Poll {
    boost::asio::posix::stream_descriptor *socket;
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
        socket = new boost::asio::posix::stream_descriptor(*loop, fd);
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

    void close() {
        socket->release();
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
