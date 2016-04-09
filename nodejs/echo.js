/* this is the native interface, to be wrapped */

var ni = require('./dist/uws').NativeInterface;
var nativeServer = new ni.Server(3000);

nativeServer.onMessage(function (socket, message, binary) {
	nativeServer.send(socket, message, binary);
});
