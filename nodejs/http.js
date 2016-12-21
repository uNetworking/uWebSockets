'use strict';

const WebSocketServer = require('./dist/uws').Server;
const wss = new WebSocketServer({ port: 3000, nativeHttp: true });

function onMessage(message) {
    this.send(message);
}

wss.on('connection', function(ws) {
    ws.on('message', onMessage);
});

wss.on('error', function(error) {
    console.log('Cannot start server');
});

wss.onHttpRequest((req, res) => {
    // console.log(req.url);
    // console.log(req.getHeader('user-agent'));
    res.end('Hello there! Welcome to the 90s');
});
