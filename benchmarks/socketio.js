var io = require('socket.io')(3000, { pingInterval: 60 * 60 * 1000 });

function incoming(message) {
    this.send(message);
}

io.on('connection', function(client){
    client.on('message', incoming);
});
