'use strict';

const uws = require('./dist/uws');
const document = Buffer.from('Hello world!');

const server = uws.http.createServer((req, res) => {
    res.end(document);
});

server.listen(3000);
