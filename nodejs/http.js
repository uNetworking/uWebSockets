const native = require('./dist/uws').native;

const serverGroup = native.server.group.create();

native.server.group.onHttpRequest(serverGroup, (external) => {
    native.server.respond(external, "Hello Node.js!");
});

native.server.group.listen(serverGroup, 3000);
