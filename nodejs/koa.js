var koa = require('koa');
var app = koa();
var uhttp = require('./dist/uws').http;

app.use(function *() {
  this.body = 'Hello World';
});

if (process.env.UWS_HTTP) {
  uhttp.createServer(app.callback()).listen(3000);
} else {
  app.listen(3000);
}
