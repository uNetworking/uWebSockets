'use strict';

const http = require('http');
//const https = require('https'); // should we ever create an https server?
const EventEmitter = require('events');
const EE_ERROR = "Registering more than one listener to a WebSocket is not supported.";
function noop() {}
const uws = (() => {
    try {
        return require(`./uws_${process.platform}_${process.versions.modules}`);
    } catch (e) {
        console.error('Error: Compilation of µWebSockets has failed and there is no pre-compiled binary ' +
        'available for your system. Please install a supported C++ compiler and reinstall the module \'uws\'.');
        process.exit(-1);
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
        this.onmessage = noop;
        this.onclose = noop;
        this.upgradeReq = null;
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
        } else if (!eventName || eventName === 'close') {
            this.onclose = noop;
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
        if (eventName === 'message' && this.onmessage === cb) {
            this.onmessage = noop;
        }
        if (eventName === 'close' && this.onclose === cb) {
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
            options = null;
        }
        if (!this.nativeSocket) {
            return cb && cb(new Error('not opened'));
        }

        const binary = options && options.binary || typeof message !== 'string';
        this.nativeServer.send(this.nativeSocket, message, binary);
        return cb && cb(null);
    }

    /**
     * Phony _socket object constructed on read.
     *
     * @public
     */
    get _socket() {
        return {
            remoteAddress: this.nativeServer.getAddress(this.nativeSocket),
            remotePort: this.nativeServer.getPort(this.nativeSocket)
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

        this.nativeServer.close(this.nativeSocket, code, data);
        this.server = this.nativeSocket = undefined;
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
        this.nativeServer = new uws.Server(0);

        // can these be made private?
        this._upgradeReq = null;
        this._upgradeCallback = noop;

        if (!options.noServer) {
            this.httpServer = options.server ? options.server : http.createServer((request, response) => {
                // todo: default HTTP response
                response.end();
            });

            if (options.path && (!options.path.length || options.path[0] !== '/')) {
                options.path = '/' + options.path;
            }

            this.httpServer.on('upgrade', (request, socket, head) => {
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
                                    // todo: send code & message
                                    socket.end();
                                }
                            });
                        } else {
                            if (options.verifyClient(info)) {
                                this.handleUpgrade(request, socket, head, (ws) => {
                                    this.emit('connection', ws);
                                });
                            } else {
                                // todo: send code & message
                                socket.end();
                            }
                        }
                    } else {
                        this.handleUpgrade(request, socket, head, (ws) => {
                            this.emit('connection', ws);
                        });
                    }
                } else {
                    socket.end();
                }
            });
        }

        this.nativeServer.onDisconnection((nativeSocket, code, message, socket) => {
            socket.onclose(code, message);
            this.nativeServer.setData(nativeSocket);
        });

        this.nativeServer.onMessage((nativeSocket, message, binary, socket) => {
            socket.onmessage(binary ? message : message.toString());
        });

        this.nativeServer.onConnection((nativeSocket) => {
            const socket = new Socket(nativeSocket, this.nativeServer);
            this.nativeServer.setData(nativeSocket, socket);

            socket.upgradeReq = this._upgradeReq;
            this._upgradeCallback(socket);
        });

        if (options.port) {
            this.httpServer.listen(options.port, callback);
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
        const ticket = this.nativeServer.transfer(socket._handle.fd, socket.ssl ? socket.ssl._external : null);
        socket.on('close', (error) => {

            this._upgradeReq = {
                url: request.url,
                headers: request.headers
            };

            this._upgradeCallback = callback ? callback : noop;
            this.nativeServer.upgrade(ticket, request.headers['sec-websocket-key']);
        });
        socket.destroy();
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
        this.nativeServer.close();
    }

}

exports.Server = Server;
exports.uws = uws;