'use strict';

const uws = require('./dist/uws');

const document = Buffer.from('Hello world!');

const server = uws.http.createServer((req, res) => {
    // handle some POST data
    if (req.method === 'POST') {
        var postString = '';
        req.on('data', (arrayBuffer) => {
            postString += Buffer.from(arrayBuffer).toString();
        }).on('end', () => {
            res.end('You posted me this: ' + postString);
        });
    // handle some GET url
    } else if (req.url === '/') {
        res.end(document);
    } else {
        res.end('Unknown request by: ' + req.headers['user-agent']);
    }
});

const wss = new uws.Server({server: server});

wss.on('connection', (ws) => {
    ws.send('Welcome to the 10s!');
});

server.listen(3000);
