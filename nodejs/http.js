'use strict';

const uws = require('./dist/uws');

const server = uws.http.createServer((req, res) => {
    res.end('Welcome to the 90s!\n');
});

const wss = new uws.Server({server: server});

wss.on('connection', (ws) => {
    ws.send('Welcome to the 10s!');
});

server.listen(3000);
