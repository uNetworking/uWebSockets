// example showing usage with different behavior on a path basis

const http = require('http');
const WebSocketServer = require('./dist/uws').Server;

const httpServer = http.createServer((request, response) => {
  response.end();
});

// echo server on path /echo, echoes the verbose message
const wssEcho = new WebSocketServer({ path: 'echo', server: httpServer });
wssEcho.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    ws.send(message, { binary: Buffer.isBuffer(message) });
  });
});

// scream echo server on path /scream, echoes the uppercase message
const wssScream = new WebSocketServer({ path: 'scream', server: httpServer });
wssScream.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    ws.send(message.toUpperCase(), { binary: Buffer.isBuffer(message) });
  });
});

httpServer.listen(3000);
