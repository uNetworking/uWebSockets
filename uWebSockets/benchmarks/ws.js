var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ port: 3000 });

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    ws.send(message, {binary: true});
  });
});
