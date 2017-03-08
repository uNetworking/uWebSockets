var io = require('socket.io')(3000, { pingInterval: 60 * 60 * 1000 });
io.on('connection', function(client){
    client.on('message', function (message) {
        client.send(message);
    });
});
