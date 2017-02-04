'use strict';

const uws = require('./dist/uws');

const document = Buffer.from('Hello world!');

const server = uws.http.createServer((req, res) => {
    res.end(document);
});

const wss = new uws.Server({server: server});

wss.on('connection', (ws) => {
    ws.send('Welcome to the 10s!');
});

server.listen(3000);
