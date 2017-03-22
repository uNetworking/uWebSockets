'use strict';

const WebSocketServer = require('./dist/uws').Server;
const wss = new WebSocketServer({ port: 3000 });

function onMessage(message) {
    this.send(message);
}

wss.on('connection', function(ws) {
    // warning: never attach anonymous functions to the socket!
    // that will majorly harm scalability since the scope of this
    // context will be taken hostage and never released causing major
    // memory usage increases compared to having the function created
    // outside of this context (1.2 GB vs 781 MB for 1 million connections)
    ws.on('message', onMessage);
});

wss.on('error', function(error) {
    console.log('Cannot start server');
});
