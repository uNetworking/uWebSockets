var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ port: 3000 });

function incoming(message) {
    this.send(message, {binary: true});
}

wss.on('connection', function connection(ws) {
  //ws.upgradeReq = null;
  ws.on('message', incoming);
});
