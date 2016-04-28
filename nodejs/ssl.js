const https = require('https');
const fs = require('fs');
const wsServer = require('./dist/uws').Server;

const options = {
  key: fs.readFileSync('/home/alexhultman/µWebSockets.key.pem'),
  cert: fs.readFileSync('/home/alexhultman/µWebSockets.pem')
};

const httpsServer = https.createServer(options, (req, res) => {
    req.socket.write('Hello there');
    req.socket.end();
});

const wss = new wsServer({ server: httpsServer, path: '/' });

wss.on('connection', (ws) => {
    ws.on('message', (message) => {
        ws.send(message, { binary: Buffer.isBuffer(message) });
    });
});

httpsServer.listen(3000);
