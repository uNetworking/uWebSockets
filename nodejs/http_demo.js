'use strict';

const uws = require('./dist/uws');
const document = Buffer.from('Hello world!');

const server = uws.http.createServer((req, res) => {
    // handle some POST data
    if (req.method === 'POST') {
        var body = [];
        req.on('data', (chunk) => {
            body.push(Buffer.from(chunk));
        }).on('end', () => {
            res.end('You posted me this: ' + Buffer.concat(body).toString());
        });
    // handle some GET url
    } else if (req.url === '/') {
        res.end(document);
    } else {
        res.end('Unknown request by: ' + req.headers['user-agent']);
    }
});

server.listen(3000);
