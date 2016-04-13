/* echo server */

const http = require('http');
const httpServer = http.createServer((request, response) => {
  response.end('Hello');
});

const WebSocketServer = require('./dist/uws').Server;
const wss = new WebSocketServer({ path: 'echo', server: httpServer });

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    ws.send(message, { binary: Buffer.isBuffer(message) });
  });
});

wss.on('error', function error(e) {
  console.log('Error: ' + e);
});

httpServer.listen(3000);
