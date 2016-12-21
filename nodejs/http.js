'use strict';

const native = require('./dist/uws').native;

class HttpRes {
    constructor(external) {
        this.external = external;
    }

    end(data) {
        native.server.respond(this.external, data);
    }
}

class uhttp {
    constructor(reqCb) {
        this.serverGroup = native.server.group.create();

        native.server.group.onHttpRequest(this.serverGroup, (external, url) => {
            reqCb({url: url, getHeader: native.server.getHeader}, new HttpRes(external));
        });
    }

    static createServer(reqCb) {
        return new uhttp(reqCb);
    }

    listen(port) {
        native.server.group.listen(this.serverGroup, port);
    }
}

// usage example

var server = uhttp.createServer((req, res) => {
    // console.log('Url: ' + req.url);
    // console.log('User Agent: ' + req.getHeader('user-agent'));
    res.end('Hello world!');
});

server.listen(3000);
