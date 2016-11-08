var io = require('socket.io')();
io.on('connection', function(client){
    client.on('message', function (message) {
        client.send(message);
    });
});
io.listen(3000);
