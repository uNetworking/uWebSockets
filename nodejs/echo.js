/* echo server */

const WebSocketServer = require('./dist/uws').Server;
const wss = new WebSocketServer({ port: 3000 });

wss.on('connection', function connection(ws) {
  var typedArray = new Float32Array(10);
  typedArray.fill(1234);
  console.log(typedArray);
  ws.close(1011, typedArray);
  ws.on('message', function incoming(message) {
    ws.send(message, { binary: Buffer.isBuffer(message) });
  });
});

wss.on('error', function error(e) {
  console.log('Error: ' + e);
});
