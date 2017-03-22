'use strict';

const uws = require('./dist/uws');

// the page that will connect back over EventSource
const document = '<script>var es = new EventSource(\'/eventSource\'); es.onmessage = function(message) {document.write(\'<p><b>Server sent event:</b> \' + message.data + \'</p>\');};</script>';

const server = uws.http.createServer((req, res) => {
    if (req.url === '/') {
        res.end(document);
    } else if (req.url === '/eventSource') {
        // write the event-stream HTTP head so that we can send chunks back later
        res.writeHead(200, {'Content-Type': 'text/event-stream'});

        // start a timer that will send back events over this HTTP socket
        var interval = setInterval(() => {
            // important to not end the socket, but write instead!
            res.write('data: Some message from server here!\n\n');
        }, 1000);

        // if the client disconnects we stop the timer
        res.on('close', () => {
            clearInterval(interval);
        });
    } else {
        // todo: terminate
        console.log('Unsupported url: ' + req.url);
        res.end('Nope, nope!');
    }
});

server.listen(3000);
