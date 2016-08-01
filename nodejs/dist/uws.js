'use strict';

const http = require('http');
const EventEmitter = require('events');
function noop() {}
function abortConnection(socket, code, name) {
    socket.end('HTTP/1.1 ' + code + ' ' + name + '\r\n\r\n');
}
const uws = (() => {
    try {
        return require(`./uws_${process.platform}_${process.versions.modules}`);
    } catch (e) {
        throw new Error('Compilation of µWebSockets has failed and there is no pre-compiled binary ' +
        'available for your system. Please install a supported C++11 compiler and reinstall the module \'uws\'.');
    }
})();

class Socket {
    /**
     * Creates a Socket instance.
     *
     * @param {Object} nativeSocket Socket instance
     * @param {Server} server Server instance
     */
    constructor(nativeSocket, nativeServer) {
        this.nativeSocket = nativeSocket;
        this.nativeServer = nativeServer;
        this.onMessageListeners = [];
        this.onCloseListeners = [];
        this.onPingListeners = [];
        this.onPongListeners = [];
        this.upgradeReq = null;
    }

    set onmessage(f) {
        if (f) {
            this.onMessageListeners.push((message) => {
                f({data: Buffer.isBuffer(message) ? new Uint8Array(message).buffer : message});
            });
        }
    }

    set onclose(f) {
        if (f) {
            this.onCloseListeners.push(() => {
                f();
            });
        }
    }

    emit(eventName, arg1, arg2) {
        if (eventName === 'message') {
            this.onMessageListeners.forEach(function(f) { f(arg1); });
        } else if (eventName === 'close') {
            this.onCloseListeners.forEach(function(f) { f(arg1, arg2); });
        } else if (eventName === 'ping') {
            this.onPingListeners.forEach(function(f) { f(arg1); });
        } else if (eventName === 'pong') {
            this.onPongListeners.forEach(function(f) { f(arg1); });
        }
        return this;
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
            this.onMessageListeners.push(f);
        } else if (eventName === 'close') {
            this.onCloseListeners.push(f);
        } else if (eventName === 'ping') {
            this.onPingListeners.push(f);
        } else if (eventName === 'pong') {
            this.onPongListeners.push(f);
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
        var socket = this;
        function once() {
            f();
            socket.removeListener(eventName, once);
        }
        if (eventName === 'message') {
            this.onMessageListeners.push(once);
        } else if (eventName === 'close') {
            this.onCloseListeners.push(once);
        } else if (eventName === 'ping') {
            this.onPingListeners.push(once);
        } else if (eventName === 'pong') {
            this.onPongListeners.push(once);
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
            this.onMessageListeners = [];
        }
        if (!eventName || eventName === 'close') {
            this.onCloseListeners = [];
        }
        if (!eventName || eventName === 'ping') {
            this.onPingListeners = [];
        }
        if (!eventName || eventName === 'pong') {
            this.onPongListeners = [];
        }
        return this;
    }

    /**
     * Removes one registered callback for the given eventName.
     *
     * @param {String} eventName Event name
     * @param {Function} callback
     * @public
     */
    removeListener(eventName, cb) {
        var array;
        if (eventName === 'message') {
            array = this.onMessageListeners;
        } else if (eventName === 'close') {
            array = this.onCloseListeners;
        } else if (eventName === 'ping') {
            array = this.onPingListeners;
        } else if (eventName === 'pong') {
            array = this.onPongListeners;
        }
        if (array) {
            var index = array.indexOf(cb);
            if (index >= 0) { array.splice(index, 1); }
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
            options = null;
        }
        if (!this.nativeSocket) {
            return cb && cb(new Error('not opened'));
        }

        const binary = options && options.binary || typeof message !== 'string';
        this.nativeServer.send(this.nativeSocket, message, binary ? Socket.OPCODE_BINARY : Socket.OPCODE_TEXT, cb ? (() => {
            process.nextTick(cb);
        }) : undefined);
    }

    /**
     * Sends a prepared message.
     *
     * @param {Object} preparedMessage The prepared message to send
     * @public
     */
    sendPrepared(preparedMessage) {
        /* ignore sends on closed sockets */
        if (!this.nativeSocket) {
            return;
        }

        this.nativeServer.sendPrepared(this.nativeSocket, preparedMessage);
    }

    /**
     * Sends a ping.
     *
     * @param {String|Buffer} message The message to send
     * @param {Object} options Send options
     * @param {Boolean} dontFailWhenClosed optional boolean
     * @public
     */
    ping(message, options, dontFailWhenClosed) {
        /* ignore sends on closed sockets */
        if (!this.nativeSocket) {
            return;
        }

        this.nativeServer.send(this.nativeSocket, message, Socket.OPCODE_PING);
    }

    /**
     * Phony _socket object constructed on read.
     *
     * @public
     */
    get _socket() {
        const address = this.nativeServer ? this.nativeServer.getAddress(this.nativeSocket) : new Array(3);
        return {
            remotePort: address[0],
            remoteAddress: address[1],
            remoteFamily: address[2]
        };
    }

    /**
     * Per-instance readyState constants.
     *
     * @public
     */
    get OPEN() {
        return Socket.OPEN;
    }

    get CLOSED() {
        return Socket.CLOSED;
    }

    /**
     * Returns the state of the socket (OPEN or CLOSED).
     *
     * @public
     */
    get readyState() {
        return this.nativeSocket !== null ? Socket.OPEN : Socket.CLOSED;
    }

    /**
     * Closes the socket.
     *
     * @public
     */
    close(code, data) {
        /* ignore close on closed sockets */
        if (!this.nativeSocket) return;

        /* Engine.IO, we cannot emit 'close' from within this function call */
        const nativeSocket = this.nativeSocket, nativeServer = this.nativeServer;
        process.nextTick(() => {
            nativeServer.close(nativeSocket, code, data);
        });

        this.nativeServer = this.nativeSocket = null;
    }
}

class Server extends EventEmitter {
    /**
     * Creates a Server instance.
     *
     * @param {Object} options Configuration options
     */
    constructor(options, callback) {
        super();

        var nativeOptions = Socket.PERMESSAGE_DEFLATE;
        if (options.perMessageDeflate !== undefined) {
            if (options.perMessageDeflate === false) {
                nativeOptions = 0;
            } else {
                if (options.perMessageDeflate.serverNoContextTakeover === true) {
                    nativeOptions |= Socket.SERVER_NO_CONTEXT_TAKEOVER;
                }
                if (options.perMessageDeflate.clientNoContextTakeover === true) {
                    nativeOptions |= Socket.CLIENT_NO_CONTEXT_TAKEOVER;
                }
            }
        }

        this.nativeServer = new uws.Server(0, nativeOptions, options.maxPayload === undefined ? 1048576 : options.maxPayload);

        // can these be made private?
        this._upgradeReq = null;
        this._upgradeCallback = noop;
        this._upgradeListener = null;
        this._noDelay = options.noDelay === undefined ? true : options.noDelay;
        this._lastUpgradeListener = true;

        if (!options.noServer) {
            this.httpServer = options.server ? options.server : http.createServer((request, response) => {
                // todo: default HTTP response
                response.end();
            });

            if (options.path && (!options.path.length || options.path[0] !== '/')) {
                options.path = '/' + options.path;
            }

            this.httpServer.on('upgrade', this._upgradeListener = ((request, socket, head) => {
                if (!options.path || options.path == request.url.split('?')[0].split('#')[0]) {
                    if (options.verifyClient) {
                        const info = {
                            origin: request.headers.origin,
                            secure: request.connection.authorized !== undefined || request.connection.encrypted !== undefined,
                            req: request
                        };

                        if (options.verifyClient.length === 2) {
                            options.verifyClient(info, (result, code, name) => {
                                if (result) {
                                    this.handleUpgrade(request, socket, head, (ws) => {
                                        this.emit('connection', ws);
                                    });
                                } else {
                                    abortConnection(socket, code, name);
                                }
                            });
                        } else {
                            if (options.verifyClient(info)) {
                                this.handleUpgrade(request, socket, head, (ws) => {
                                    this.emit('connection', ws);
                                });
                            } else {
                                abortConnection(socket, 400, 'Client verification failed');
                            }
                        }
                    } else {
                        this.handleUpgrade(request, socket, head, (ws) => {
                            this.emit('connection', ws);
                        });
                    }
                } else {
                    if (this._lastUpgradeListener) {
                        abortConnection(socket, 400, 'URL not supported');
                    }
                }
            }));

            this.httpServer.on('newListener', (eventName, listener) => {
                if (eventName === 'upgrade') {
                    this._lastUpgradeListener = false;
                }
            });
        }

        this.nativeServer.onDisconnection((nativeSocket, code, message, socket) => {
            socket.nativeServer = socket.nativeSocket = null;
            socket.onCloseListeners.forEach(function(f) {
                f(code, message);
            });
            this.nativeServer.setData(nativeSocket);
        });

        this.nativeServer.onMessage((nativeSocket, message, binary, socket) => {
            socket.onMessageListeners.forEach(function(f) {
                f(binary ? message : message.toString());
            });
        });

        this.nativeServer.onPing((nativeSocket, message, socket) => {
            socket.onPingListeners.forEach(function(f) {
                f(message.toString());
            });
        });

        this.nativeServer.onPong((nativeSocket, message, socket) => {
            socket.onPongListeners.forEach(function(f) {
                f(message.toString());
            });
        });

        this.nativeServer.onConnection((nativeSocket) => {
            const socket = new Socket(nativeSocket, this.nativeServer);
            this.nativeServer.setData(nativeSocket, socket);

            socket.upgradeReq = {
                url: this._upgradeReq.url,
                headers: this._upgradeReq.headers,
                connection: socket._socket
            };

            this._upgradeCallback(socket);
        });

        if (options.port) {
            if (options.host) {
                this.httpServer.listen(options.port, options.host, callback);
            } else {
                this.httpServer.listen(options.port, callback);
            }
        }
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
        const secKey = request.headers['sec-websocket-key'];
        if (secKey && secKey.length == 24) {
            socket.setNoDelay(this._noDelay);
            const ticket = this.nativeServer.transfer(socket._handle.fd === -1 ? socket._handle : socket._handle.fd, socket.ssl ? socket.ssl._external : null);
            socket.on('close', (error) => {
                this._upgradeReq = request;
                this._upgradeCallback = callback ? callback : noop;
                this.nativeServer.upgrade(ticket, secKey, request.headers['sec-websocket-extensions']);
            });
        }
        socket.destroy();
    }

    /**
     * Prepare a message for bulk sending.
     *
     * @param {String|Buffer} message The message to prepare
     * @param {Boolean} binary Binary (or text) OpCode
     * @public
     */
    prepareMessage(message, binary) {
        return this.nativeServer.prepareMessage(message, binary ? Socket.OPCODE_BINARY : Socket.OPCODE_TEXT);
    }

    /**
     * Finalize (unreference) a message after bulk sending.
     *
     * @param {Object} preparedMessage The prepared message to finalize
     * @public
     */
    finalizeMessage(preparedMessage) {
        return this.nativeServer.finalizeMessage(preparedMessage);
    }

    /**
     * Broadcast a message to all sockets.
     *
     * @param {String|Buffer} message The message to broadcast
     * @param {Object} options Broadcast options
     * @public
     */
    broadcast(message, options) {
        this.nativeServer.broadcast(message, options && options.binary || false);
    }

     /**
     * Closes the server.
     *
     * @public
     */
    close() {
        if (this._upgradeListener && this.httpServer) {
            this.httpServer.removeListener('upgrade', this._upgradeListener);
        }

        this.nativeServer.close();
    }
}

Socket.PERMESSAGE_DEFLATE = 1;
Socket.SERVER_NO_CONTEXT_TAKEOVER = 2;
Socket.CLIENT_NO_CONTEXT_TAKEOVER = 4;
Socket.OPCODE_TEXT = 1;
Socket.OPCODE_BINARY = 2;
Socket.OPCODE_PING = 9;
Socket.OPEN = 1;
Socket.CLOSED = 0;
Socket.Server = Server;
Socket.uws = uws;
module.exports = Socket;
