const WebSocketServer = require('../dist/uws').Server;
const non_ssl = new WebSocketServer({ port: 3000 });
const fs = require('fs');
const https = require('https');

non_ssl.on('connection', function(ws) {
  ws.on('message', function(message) {
    ws.send(message);
  });
});

const options = {
    key: fs.readFileSync('../../ssl/key.pem'),
    cert: fs.readFileSync('../../ssl/cert.pem'),
    passphrase: '1234'
};

const httpsServer = https.createServer(options, (req, res) => {
    req.socket.end();
});

const ssl = new WebSocketServer({ server: httpsServer });

ssl.on('connection', function(ws) {
  ws.on('message', function(message) {
    ws.send(message);
  });
});

httpsServer.listen(3001);
