#include "App.h"

/* optparse */
#define OPTPARSE_IMPLEMENTATION
#include "helpers/optparse.h"

int main(int /*argc*/, char **argv) {
    int option;
    struct optparse options;
    optparse_init(&options, argv);

    struct optparse_long longopts[] = {
        {"port", 'p', OPTPARSE_REQUIRED},
        {"help", 'h', OPTPARSE_NONE},
        {"passphrase", 'a', OPTPARSE_OPTIONAL},
        {"key", 'k', OPTPARSE_REQUIRED},
        {"cert", 'c', OPTPARSE_REQUIRED},
        {"compression_dedicated", 'Z', OPTPARSE_NONE},
        {"compression_shared", 'S', OPTPARSE_NONE},
        {"nocompression", 'z', OPTPARSE_NONE},
        {"dh_params", 'd', OPTPARSE_REQUIRED},
        {"simulate_auth_delay", 't', OPTPARSE_REQUIRED},
        {0, 0, OPTPARSE_NONE}
    };

    int port = 3000;
    auto compressionFlavour = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR_4KB | uWS::DEDICATED_DECOMPRESSOR);
    int simulateAuthDelayMS = 10;
    struct uWS::SocketContextOptions ssl_options = {};

    while ((option = optparse_long(&options, longopts, nullptr)) != -1) {
        switch (option) {
        case 'p':
            port = atoi(options.optarg);
            break;
        case 'a':
            ssl_options.passphrase = options.optarg;
            break;
        case 'c':
            ssl_options.cert_file_name = options.optarg;
            break;
        case 'k':
            ssl_options.key_file_name = options.optarg;
            break;
        case 'd':
            ssl_options.dh_params_file_name = options.optarg;
            break;
        case 'z':
            compressionFlavour = uWS::CompressOptions::DISABLED;
            break;
        case 'Z':
            compressionFlavour = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR |
                                                      uWS::DEDICATED_DECOMPRESSOR);
            break;
        case 'S':
            compressionFlavour = uWS::CompressOptions(uWS::SHARED_COMPRESSOR |
                                                      uWS::SHARED_DECOMPRESSOR);
            break;
        case 't':
            simulateAuthDelayMS = atoi(options.optarg);
            break;
        case 'h':
        case '?':
            fail:
            std::cout << "Usage: " << argv[0] << " [--help]"
                << " [--port <port>]"
                << " [--key <ssl key>]"
                << " [--cert <ssl cert>] [--passphrase <ssl key passphrase>]"
                << " [--dh_params <ssl dh params file>]"
                << " [--compression_dedicated]"
                << " [--compression_shared]"
                << " [--nocompression]"
                << " [--simulate_auth_delay]"
                << std::endl
                << "Example: " << argv[0] << " --port 50444 --key ./key.pem --cert ./cert.pem --compression_dedicated"
                << std::endl;
            return 0;
        }
    }

    char *unused_params = optparse_arg(&options);
    if (unused_params) {
        goto fail;
    }

    /* ws->getUserData returns one of these */
    struct PerSocketData {
        /* Define your user data */
        int something;
    };

    /* Keep in mind that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support.
     * You may swap to using uWS:App() if you don't need SSL */
    uWS::SSLApp(ssl_options
    ).get("/hello", [](auto *res, auto * /*req*/) {
        res->writeHeader("Content-Type", "text/html; charset=utf-8")->end("Hello HTTP!");
    }).get("/*", [](auto *res, auto * /*req*/) {
        res->writeHeader("Content-Type", "text/html; charset=utf-8")->end("Nope!");
    }).get("/test1.html", [](auto *res, auto * /*req*/) {
        res->writeHeader("Content-Type", "text/html; charset=utf-8");
        res->write(R"jk(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <link rel="icon" href="/favicon.ico" />
    <script src="./test1.js"></script>
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Websocket Connectivity Test</title>
  </head>
  <body>
    <div id="app"></div>
    <p id="start"></p>
    <div>
      <button id="btn_restart" type="button">Restart</button>
      <button id="btn_close" type="button">Close</button>
      <button id="btn_send" type="button">Send</button>
      <button id="btn_send2" type="button">2</button>
      <button id="btn_send3" type="button">3</button>
      <button id="btn_send4" type="button">4</button>
      <button id="btn_send5" type="button">5</button>
      <button id="btn_send6" type="button">6</button>
      <button id="btn_clear" type="button">Clear</button>
    </div-->
    <p id="ws_open">Not open</p>
    <p id="ws_close">Not closed</p>
    <p id="ws_error">No Error</p>
    <div id="ws_message"></div>
  </body>
</html>
)jk");
        res->end();
    }).get("/test1.js", [](auto *res, auto * /*req*/) {
        res->writeHeader("Content-Type", "text/javascript; charset=utf-8");
        res->write(R"jk(
var ws = null;
var msgs = [];
var Tstart = 0;
var Topen = 0;
var Terror = 0;
var Tclose = 0;
var Tmessage = 0;

function send(msg) {
  console.log("Will send msg of length:" + msg.length + "Starts with:" + msg.substring(0, 40))
  if (ws == null || ws.readyState !== 1) {
    console.log("queueing message:" + msg);
    msgs.push(msg);
  } else {
    ws.send(msg);
  }
}

function startup() {
  Tstart = performance.now();
  Topen = 0;
  Terror = 0;
  Tclose = 0;
  Tmessage = 0;

  document.getElementById("btn_restart").addEventListener("click", test_restart);
  document.getElementById("btn_close").addEventListener("click", test_close);
  document.getElementById("btn_send").addEventListener("click", test_send);
  document.getElementById("btn_send2").addEventListener("click", test_send2);
  document.getElementById("btn_send3").addEventListener("click", test_send3);
  document.getElementById("btn_send4").addEventListener("click", test_send4);
  document.getElementById("btn_send5").addEventListener("click", test_send5);
  document.getElementById("btn_send6").addEventListener("click", test_send6);
  document.getElementById("btn_clear").addEventListener("click", test_clear);
  document.getElementById("start").innerHTML = "Started " + Date();

  if (ws === null || (ws && ws.readyState === 3)) {
    ws = new WebSocket(
      ((window.location.protocol === "https:") ? "wss://" : "ws://") + window.location.host + "/api",
      "pghz_v1"
    );

    ws.onopen = function (event) {
      Topen = performance.now();
      console.log('onopen', event)
      document.getElementById("ws_open").innerHTML =
        "onopen " + Date() + "." + (Topen - Tstart) + " protocol:" + ws.protocol;
      while (msgs.length > 0) {
        ws.send(msgs.pop())
      }
    };

    ws.onerror = function (event) {
      Terror = performance.now();
      console.log('onerror', event)
      document.getElementById("ws_error").innerHTML = "onerror " + Date() + "." + (Terror - Tstart) + " - " + event;
    };

    ws.onclose = function (event) {
      Tclose = performance.now();
      console.log('onclose', event)
      document.getElementById("ws_close").innerHTML = "onclose " + Date() + "." + (Tclose - Tstart) + " - " + event;
    };

    ws.addEventListener('message', function (event) {
      Tmessage = performance.now()
      console.log('onmessage', event)
      var h = document.getElementById('ws_message')
      h.insertAdjacentHTML('afterbegin', "<div><p>" + Date() + "." + (Tmessage - Tstart) + " - " + JSON.stringify(event.data) + "</p></div>");
    });
  }
  //test_send();
}

function test_restart() {
  console.log("restart websocket");
  if (msgs.length > 0) {
    console.log("Dropping unsent messages: " + msgs.length);
    msg = [];
  }
  startup();
}

function test_close() {
  console.log("close websocket");
  ws.close();
}

function test_send() {
  send_ReqLogin();
}

function test_send2() {
  send_ReqLogin();
  send_ReqRosterAllocationListWithinPeriod();
}

function test_send3() {
  send_ReqLogin();
  send_ReqStaffHistoryMonthlyStats();
  send_ReqAnalysisEntityAnnotationsForEntities();
  send_ReqActivityByIdList();
}

function test_send4() {
  send_ReqLogin();
  send_ReqAnalysisEntityAnnotationsForEntities();
  send_ReqActivityByIdList();
}

function test_send5() {
  send_ReqAnalysisEntityAnnotationsForEntities();
  send_ReqActivityByIdList();
}

function test_send6() {
  send_ReqActivityByIdList();
  send_ReqAnalysisEntityAnnotationsForEntities();
}

function test_clear() {
  console.log("clearing messages");
  var h = document.getElementById('ws_message');
  while (h.firstChild) {
    h.firstChild.remove();
  }
  msgs = [];
}

function send_ReqLoginA() {
   send('a'.repeat(1672));
}

function send_ReqLogin() {
   send('{"SkyNet_MsgName":"ReqLogin","SenderId":0,"RequestNo":57,"ClientGuid":"dde35df1-1573-458c-adc0-f4c226abc962","Name":"CTICC01","Password":"' + 'jwt'.repeat(368) + 'e","Attrs":{"BoolAttributes":[{"Key":"JWT_TOKEN","SequenceNumber":null,"Value":true}],"DateAttributes":[],"DateRangeAttributes":[],"DatetimeAttributes":[],"DurationAttributes":[],"IntAttributes":[],"IntRangeAttributes":[],"FloatAttributes":[],"StringAttributes":[{"Key":"LOGIN_TYPE","Value":"Crew","SequenceNumber":null},{"Key":"DIAGNOSTICS","SequenceNumber":null,"Value":"os: null null, ip: null, app: 1.2.3.4, ui: 21.8.0.33"}]}}');
}

function send_ReqRosterAllocationListWithinPeriod() {
  send(
    '{"SkyNet_MsgName":"ReqRosterAllocationListWithinPeriod","SenderId":0,"RequestNo":384,"ClientGuid":"ef3736be-6e23-4cf4-9907-5fec00da6a4f","Scn":"0","StartDatetime":"2021-10-31T00:00:00+08:00","EndDatetime":"2021-12-04T23:59:00+08:00"}'
    );
}

function send_ReqStaffHistoryMonthlyStats() {
  send(
    '{"SkyNet_MsgName":"ReqStaffHistoryMonthlyStats","SenderId":0,"RequestNo":385,"ClientGuid":"ef3736be-6e23-4cf4-9907-5fec00da6a4f","StaffIds":["CTICP01"],"StartDatetime":"2021-11-01T00:00:00+08:00","EndDatetime":"2021-11-30T23:59:00+08:00"}'
    );
}

function send_ReqAnalysisEntityAnnotationsForEntities() {
  send(
    '{"SkyNet_MsgName":"ReqAnalysisEntityAnnotationsForEntities","SenderId":0,"RequestNo":386,"ClientGuid":"ef3736be-6e23-4cf4-9907-5fec00da6a4f","AnalysisContextRecNo":6,"ActivityRecNos":[],"LegRecNos":[17951],"RosterAllocationRecNos":[],"StaffMemberRecNos":[]}'
    );
}

function send_ReqActivityByIdList() {
  send(
    '{"SkyNet_MsgName":"ReqActivityByIdList","SenderId":0,"RequestNo":387,"ClientGuid":"ef3736be-6e23-4cf4-9907-5fec00da6a4f","Scn":"0","Ids":["32Q-1HKG01P_2021-11-08T08:10_11P"],"BoolAttrKeys":[],"DateAttrKeys":[],"DateRangeAttrKeys":[],"DatetimeAttrKeys":[],"DurationAttrKeys":[],"FloatAttrKeys":[],"IntAttrKeys":[],"IntRangeAttrKeys":[],"StringAttrKeys":[]}'
    );
}

document.addEventListener('DOMContentLoaded', startup, false);
)jk");
        res->end();
    }).ws<PerSocketData>("/*", {
        /* Settings */
        // Use uWS::DEDICATED_COMPRESSOR | uWS::DEDICATED_DECOMPRESSOR to work around IOS15
        // compression incompatibility
        // https://github.com/uNetworking/uWebSockets/issues/1347
        // example was .compression = uWS::SHARED_COMPRESSOR,
        .compression = compressionFlavour,
        .maxPayloadLength = 16 * 1024,
        .idleTimeout = 16,
        .maxBackpressure = 1 * 1024 * 1024,
        /* Handlers */
        .upgrade = [simulateAuthDelayMS](auto *res, auto *req, auto *context) {

            /* HttpRequest (req) is only valid in this very callback, so we must COPY the headers
             * we need later on while upgrading to WebSocket. You must not access req after first return.
             * Here we create a heap allocated struct holding everything we will need later on. */

            struct UpgradeData {
                std::string secWebSocketKey;
                std::string secWebSocketProtocol;
                std::string secWebSocketExtensions;
                struct us_socket_context_t *context;
                decltype(res) httpRes;
                bool aborted = false;
            } *upgradeData = new UpgradeData {
                std::string(req->getHeader("sec-websocket-key")),
                std::string(req->getHeader("sec-websocket-protocol")),
                std::string(req->getHeader("sec-websocket-extensions")),
                context,
                res
            };

            /* We have to attach an abort handler for us to be aware
             * of disconnections while we perform async tasks */
            res->onAborted([=]() {
                /* We don't implement any kind of cancellation here,
                 * so simply flag us as aborted */
                upgradeData->aborted = true;
                std::cout << "HTTP socket was closed before we upgraded it!" << std::endl;
            });

            /* Simulate checking auth for 5 seconds. This looks like crap, never write
             * code that utilize us_timer_t like this; they are high-cost and should
             * not be created and destroyed more than rarely!
             *
             * Also note that the code would be a lot simpler with capturing lambdas, maybe your
             * database client has such a nice interface? Either way, here we go!*/
            struct us_loop_t *loop = (struct us_loop_t *) uWS::Loop::get();
            struct us_timer_t *delayTimer = us_create_timer(loop, 0, sizeof(UpgradeData *));
            memcpy(us_timer_ext(delayTimer), &upgradeData, sizeof(UpgradeData *));
            us_timer_set(delayTimer, [](struct us_timer_t *t) {
                /* We wrote the upgradeData pointer to the timer's extension */
                UpgradeData *upgradeData;
                memcpy(&upgradeData, us_timer_ext(t), sizeof(UpgradeData *));

                /* Were'nt we aborted before our async task finished? Okay, upgrade then! */
                if (!upgradeData->aborted) {
                    std::cout << "Async task done, upgrading to WebSocket now!" << std::endl;

                    /* If you don't want to upgrade you can instead respond with custom HTTP here,
                    * such as res->writeStatus(...)->writeHeader(...)->end(...); or similar.*/

                    /* This call will immediately emit .open event */
                    upgradeData->httpRes->template upgrade<PerSocketData>({
                        /* We initialize PerSocketData struct here */
                        .something = 13
                    }, upgradeData->secWebSocketKey,
                        upgradeData->secWebSocketProtocol,
                        upgradeData->secWebSocketExtensions,
                        upgradeData->context);
                } else {
                    std::cout << "Async task done, but the HTTP socket was closed. Skipping upgrade to WebSocket!" << std::endl;
                }

                delete upgradeData;

                us_timer_close(t);
            }, simulateAuthDelayMS, 0);

        },
        .open = [](auto *ws) {
            /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct.
             * Here we simply validate that indeed, something == 13 as set in upgrade handler. */
            std::cout << "Something is: " << static_cast<PerSocketData *>(ws->getUserData())->something << std::endl;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            /* We simply echo whatever data we get */
            ws->send(message, opCode);
        },
        .drain = [](auto */*ws*/) {
            /* Check ws->getBufferedAmount() here */
        },
        .ping = [](auto */*ws*/, std::string_view) {
            /* You don't need to handle this one, we automatically respond to pings as per standard */
        },
        .pong = [](auto */*ws*/, std::string_view) {
            /* You don't need to handle this one either */
        },
        .close = [](auto */*ws*/, int /*code*/, std::string_view /*message*/) {
            /* You may access ws->getUserData() here, but sending or
             * doing any kind of I/O with the socket is not valid. */
        }
    }).listen(port, [port](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << port << std::endl;
        }
    }).run();
}
