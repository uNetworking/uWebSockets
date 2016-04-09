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
            self.server.close(self.nativeSocket);
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

    /* these seem to match already */
    this.close = this.nativeServer.close;
    this.broadcast = this.nativeServer.broadcast;

    this.nativeServer.onConnection(function (nativeSocket) {
        var socket = new Socket(nativeSocket, self);
        self.nativeServer.setData(nativeSocket, socket);
        self.emit('connection', socket);
    });

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
