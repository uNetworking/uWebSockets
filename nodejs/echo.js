// echo server

const WebSocketServer = require('./dist/uws').Server;
const wss = new WebSocketServer({ port: 3000, maxPayload: 0 });

var sentMessages = 0;
function sent() {
  if (++sentMessages % 10000 == 0) {
    console.log('Messages sent: ' + sentMessages);
  }
}

wss.startAutoPing(1000);

wss.on('connection', function connection(ws) {
  // console.log(ws._socket);

  //console.log('Size: ' + wss.clients.length);

  /*wss.clients.forEach((ws) => {
    ws.send('Hello dude!');
  });*/

  ws.on('pong', function pong(message) {
    console.log('Got a pong!');
  });

  // EventEmitter interface
  /*ws.on('message', function incoming(message) {
    ws.send(message, { binary: Buffer.isBuffer(message) }, sent);
  });*/

  // Web interface
  ws.onmessage = function(e) {
    ws.send(e.data);
  };

});

wss.on('error', function error(e) {
  console.log('Error: ' + e);
});
