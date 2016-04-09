/* ws interface for Node.js developers */

const NativeServer = require('./uws_' + process.platform + '_' + process.versions.modules).Server;
const EventEmitter = require('events');
const util = require('util');

function Socket(nativeSocket, server) {
    EventEmitter.call(this);
    var self = this;
    this.nativeSocket = nativeSocket;
    this.server = server;

    this.send = function(message, options) {
        if (self.nativeSocket !== null) {
            var binary = false;
            if (options !== undefined) {
                binary = options.binary;
            }
            self.server.nativeServer.send(self.nativeSocket, message, binary);
        } else {
            /* ignore sends on closed sockets */
        }
    }

    this.close = function() {
        if (self.nativeSocket !== null) {
            self.server.nativeServer.close(self.nativeSocket);
            self.nativeSocket = null;
        } else {
            /* ignore close on closed sockets */
        }
    }
}

module.exports.Server = function Server(options) {
    EventEmitter.call(this);

    /* we currently do only support port as option */
    this.nativeServer = new NativeServer(options.port);
    var self = this;

    this.close = function () {
        self.nativeServer.close();
    }
    this.broadcast = function (message, options) {
        /* only listen to binary option */
        this.nativeServer.broadcast(message, options.binary);
    };

    /* original connection callback */
    this.connectionCallback = function (nativeSocket) {
        var socket = new Socket(nativeSocket, self);
        self.nativeServer.setData(nativeSocket, socket);
    };

    /* this function needs to handle the upgrade directly! */
    this.handleUpgrade = function (request, socket, upgradeHead, callback) {
        /* register a special connection handler */
        self.nativeServer.onConnection(function (nativeSocket) {
            var socket = new Socket(nativeSocket, self);
            self.nativeServer.setData(nativeSocket, socket);

            /* internal variables */
            socket.upgradeReq = request;
            socket.readyState = socket.OPEN;
            callback(socket);

            /* todo: reset connection handler when the upgrade queue is empty */
            /* this will probably never be noticed as you don't mix upgrade handling */
        });

        /* upgrades will be handled immediately */
        self.nativeServer.upgrade(socket._handle.fd, request.headers['sec-websocket-key']);
        socket.destroy();
    };

    this.nativeServer.onConnection(this.connectionCallback);
    this.nativeServer.onDisconnection(function (nativeSocket) {
        var socket = self.nativeServer.getData(nativeSocket);
        socket.emit('close');
    });

    this.nativeServer.onMessage(function (nativeSocket, message, binary) {
        var socket = self.nativeServer.getData(nativeSocket);
        if (binary) {
            socket.emit('message', message);
        } else {
            /* we are supposed to pass text as strings */
            socket.emit('message', message.toString());
        }
    });
};

util.inherits(Socket, EventEmitter);
util.inherits(module.exports.Server, EventEmitter);
