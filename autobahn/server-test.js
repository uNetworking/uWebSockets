// Based on https://github.com/denoland/fastwebsockets/blob/main/autobahn/server-test.js

import { $ } from "https://deno.land/x/dax/mod.ts";

const pwd = new URL(".", import.meta.url).pathname;

const AUTOBAHN_TESTSUITE_DOCKER =
  "crossbario/autobahn-testsuite:0.8.2@sha256:5d4ba3aa7d6ab2fdbf6606f3f4ecbe4b66f205ce1cbc176d6cdf650157e52242";

const server = $`uWebSockets/EchoServer`.spawn();
await $`docker run --name fuzzingserver	-v ${pwd}/fuzzingclient.json:/fuzzingclient.json:ro	-v ${pwd}/reports:/reports -p 9001:9001	--net=host --rm	${AUTOBAHN_TESTSUITE_DOCKER} wstest -m fuzzingclient -s fuzzingclient.json`.cwd(pwd);

const result = Object.values(JSON.parse(Deno.readTextFileSync("./uWebSockets/autobahn/reports/servers/index.json"))["uWebSockets"]);

function failed(name) {
  return name != "OK" && name != "INFORMATIONAL" && name != "NON-STRICT";
}

console.log(JSON.stringify(result, null, 2));

const failedtests = result.filter((outcome) => failed(outcome.behavior) || failed(outcome.behaviorClose));

console.log(
  `%c${result.length - failedtests.length} / ${result.length} tests OK`,
  `color: ${failedtests.length == 0 ? "green" : "red"}`,
);

Deno.exit(failedtests.length == 0 ? 0 : 1);
