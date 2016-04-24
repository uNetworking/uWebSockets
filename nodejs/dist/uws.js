'use strict';

const EventEmitter = require('events');

const uws = require(`./uws_${process.platform}_${process.versions.modules}`);
const NativeServer = uws.Server;
const EE_ERROR = "registering more than one listener to websocket";

function noop() {}

class Socket {
    /**
     * Creates a Socket instance.
     *
     * @param {Object} nativeSocket Socket instance
     * @param {Server} server Server instance
     */
    constructor(nativeSocket, server) {
        this.nativeSocket = nativeSocket;
        this.server = server;
        this.onmessage = noop;
        this.onclose = noop;
    }

    /**
     * Registers a callback for given eventName.
     *
     * @param {String} eventName Event name
     * @param {Function} f Event listener
     * @public
     */
    on(eventName, f) {
        if (eventName === 'message') {
            if (this.onmessage !== noop) {
                throw Error(EE_ERROR);
            }
            this.onmessage = f;
        } else if (eventName === 'close') {
            if (this.onclose !== noop) {
                throw Error(EE_ERROR);
            }
            this.onclose = f;
        }
        return this;
    }

    /**
     * Registers a callback for given eventName to be executed once.
     *
     * @param {String} eventName Event name
     * @param {Function} f Event listener
     * @public
     */
    once(eventName, f) {
        if (eventName === 'message') {
            if (this.onmessage !== noop) {
                throw Error(EE_ERROR);
            }
            this.onmessage = () => {
                f();
                this.onmessage = noop;
            };
        } else if (eventName === 'close') {
            if (this.onclose !== noop) {
                throw Error(EE_ERROR);
            }
            this.onclose = () => {
                f();
                this.onclose = noop;
            };
        }
        return this;
    }

    /**
     * Removes all registered callbacks for the given eventName
     * or, in the case of no eventName, all registered callbacks.
     *
     * @param {String} [eventName] Event name
     * @public
     */
    removeAllListeners(eventName) {
        if (!eventName || eventName === 'message') {
            this.onmessage = noop;
        }
        if (!eventName || eventName === 'close') {
            this.onclose = noop;
        }
        return this;
    }

    /**
     * Sends a message.
     *
     * @param {String|Buffer} message The message to send
     * @param {Object} options Send options
     * @param {Function} cb optional callback
     * @public
     */
    send(message, options, cb) {
        /* ignore sends on closed sockets */
        if (typeof options === 'function') {
            cb = options;
            options = {};
        }
        if (!this.nativeSocket) {
            return cb && cb(new Error('not opened'));
        }

        const binary = options && options.binary || false;
        this.server.nativeServer.send(this.nativeSocket, message, binary);
        return cb && cb(null);
    }

    /**
     * Phony _socket object constructed on read.
     *
     * @public
     */
    get _socket() {
        return {
            remoteAddress: this.server.nativeServer.getAddress(this.nativeSocket),
            remotePort: this.server.nativeServer.getPort(this.nativeSocket)
        };
    }

    /**
     * Closes the socket.
     *
     * @public
     */
    close(code, data) {
        /* ignore close on closed sockets */
        if (!this.nativeSocket) return;

        this.server.nativeServer.close(this.nativeSocket, code, data);
        this.server = this.nativeSocket = undefined;
    }
}

class Server extends EventEmitter {
    /**
     * Creates a Server instance.
     *
     * @param {Object} options Configuration options
     */
    constructor(options) {
        super();

        /* undefined port will resolve to 0 which is okay */
        this.nativeServer = new NativeServer(options.port);

        /* emit error if the server is in a broken state */
        this.on('newListener', (eventName, f) => {
            if (eventName === 'error' && this.nativeServer.error) {
                f(Error('EADDRINUSE'));
            }
        });

        /* only register events if the server is valid */
        if (!this.nativeServer.error) {
            /* support server & path options */
            if (options.server) {
                /* we need to get paths with slash prefix */
                if (!options.path.length || options.path[0] != '/') {
                    options.path = '/' + options.path;
                }

                options.server.on('upgrade', (request, socket, head) => {
                    /* is this our path? */
                    if (options.path == request.url.split('?')[0].split('#')[0]) {
                        /* verify client */
                        if (options.verifyClient) {
                            const info = {
                                origin: request.headers.origin,
                                secure: request.connection.authorized !== undefined || request.connection.encrypted !== undefined,
                                req: request
                            };

                            if (options.verifyClient.length === 2) {
                                options.verifyClient(info, (result, code, name) => {
                                    if (result) {
                                        /* handle upgrade */
                                        this.handleUpgrade(request, socket, head, (ws) => {
                                            this.emit('connection', ws);
                                        });
                                    } else {
                                        /* todo: send code & message */
                                        socket.end();
                                    }
                                });
                            } else {
                                if (options.verifyClient(info)) {
                                    /* handle upgrade */
                                    this.handleUpgrade(request, socket, head, (ws) => {
                                        this.emit('connection', ws);
                                    });
                                } else {
                                    /* todo: send code & message */
                                    socket.end();
                                }
                            }
                        } else {
                            /* handle upgrade */
                            this.handleUpgrade(request, socket, head, (ws) => {
                                this.emit('connection', ws);
                            });
                        }
                    } else {
                        /* are we really supposed to close the connection here? */
                        socket.end();
                    }
                });
            }

            this.nativeServer.onConnection((nativeSocket) => {
                const socket = new Socket(nativeSocket, this);
                this.nativeServer.setData(nativeSocket, socket);
                this.emit('connection', socket);
            });

            this.nativeServer.onDisconnection((nativeSocket, socket) => {
                socket.onclose();
                /* make sure to clear any set data */
                this.nativeServer.setData(nativeSocket);
            });

            this.nativeServer.onMessage((nativeSocket, message, binary, socket) => {
                socket.onmessage(binary ? message : message.toString());
            });
        }
    }

    /**
     * Closes the server.
     *
     * @public
     */
    close() {
        this.nativeServer.close();
    }

    /**
     * Broadcast a message to all sockets.
     *
     * @param {String|Buffer} message The message to broadcast
     * @param {Object} options Broadcast options
     * @public
     */
    broadcast(message, options) {
        /* only listen to binary option */
        this.nativeServer.broadcast(message, options && options.binary || false);
    }

    /**
     * Handles a HTTP Upgrade request.
     *
     * @param {http.IncomingMessage} request HTTP request
     * @param {net.Socket} Socket between server and client
     * @param {Buffer} upgradeHead The first packet of the upgraded stream
     * @param {Function} callback Callback function
     * @public
     */
    handleUpgrade(request, socket, upgradeHead, callback) {
        /* register a special connection handler */
        this.nativeServer.onConnection((nativeSocket) => {
            const sock = new Socket(nativeSocket, this);
            this.nativeServer.setData(nativeSocket, sock);

            /* internal variables */
            sock.upgradeReq = request;
            callback(sock);

            /* todo: reset connection handler when the upgrade queue is empty */
            /* this will probably never be noticed as you don't mix upgrade handling */
        });

        /* upgrades will be handled immediately */
        this.nativeServer.upgrade(socket._handle.fd, request.headers['sec-websocket-key'], socket.ssl._external);
        socket.destroy();
    }
}

exports.Server = Server;
exports.uws = uws;
