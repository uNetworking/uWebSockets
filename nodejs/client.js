var WebSocket = require('./dist/uws');
var ws = new WebSocket('ws://echo.websocket.org');

ws.on('open', function open() {
    console.log('Connected!');
    ws.send('This will be sent!');
});

ws.on('error', function error() {
    console.log('Error connecting!');
});

ws.on('message', function(data, flags) {
    console.log('Message: ' + data);
});

ws.on('close', function(code, message) {
    console.log('Disconnection: ' + code + ', ' + message);
});
