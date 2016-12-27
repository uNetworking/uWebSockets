'use strict';

const uws = require('./dist/uws');
const wss = new uws.Server({ nativeHttp: true, port: 3000 });

// note: you "disable" websocket connections
wss.on('connection', function(ws) {
    ws.terminate();
});

wss.on('error', function(error) {
    console.log('Cannot start server');
});

// requires nativeHttp: true
wss.onHttpRequest((socket, req) => {
    if (req.url == '/terminate') {
        wss.close();
    }
    console.log('Request: ' + (req.verb == uws.HTTP_GET ? 'GET' : 'POST'));
    console.log('Url: ' + req.url);
    console.log(req.getHeader('user-agent'));
    if (req.data.byteLength) {
        console.log('Data: ' + String.fromCharCode.apply(null, new Uint8Array(req.data)));
        if (req.remainingBytes) {
            console.log('Remaining data bytes: ' + remainingBytes);
        }
    }
    socket.end('Hello there! Welcome to the 90s\n');
});

// wss.onHttpConnection
// wss.onHttpDisconnection
// wss.onHttpData
