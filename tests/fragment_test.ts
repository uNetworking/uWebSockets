// fragmentation_test.ts

/**
 * Usage:
 *   deno run --allow-net fragmentation_test.ts --host <host> --port <port> [--tls]
 *
 * If no arguments are given, defaults to localhost:80.
 * The test finishes in ~1 second (plus network latency).
 */

/* --- Command line arguments --- */
const args = Deno.args;
let host = "localhost";
let port = 80;
let useTls = false;

for (let i = 0; i < args.length; i++) {
  switch (args[i]) {
    case "--host":
      host = args[++i];
      break;
    case "--port":
      port = parseInt(args[++i], 10);
      break;
    case "--tls":
      useTls = true;
      break;
  }
}

/* --- Helper: write all bytes to a connection --- */
async function writeAll(conn: Deno.Conn, data: Uint8Array): Promise<void> {
  let offset = 0;
  while (offset < data.length) {
    const n = await conn.write(data.subarray(offset));
    if (n === 0) throw new Error("Connection closed during write");
    offset += n;
  }
}

/* --- Helper: read all bytes from a connection until EOF --- */
async function readAll(conn: Deno.Conn): Promise<Uint8Array> {
  const chunks: Uint8Array[] = [];
  const buf = new Uint8Array(4096);
  while (true) {
    const n = await conn.read(buf);
    if (n === null) break;
    if (n > 0) chunks.push(buf.subarray(0, n));
  }
  const total = chunks.reduce((sum, c) => sum + c.length, 0);
  const result = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }
  return result;
}

/* --- Test requests (raw HTTP/1.1 messages) --- */
function buildTestRequests(host: string) {
  return [
    {
      name: "GET /",
      request:
        `GET / HTTP/1.1\r\nHost: ${host}\r\nConnection: close\r\n\r\n`,
    },
    {
      name: "POST with Content-Length",
      request:
        `POST / HTTP/1.1\r\nHost: ${host}\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello`,
    },
    {
      name: "POST with chunked encoding",
      request:
        `POST / HTTP/1.1\r\nHost: ${host}\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n5\r\nhello\r\n0\r\n\r\n`,
    },
  ];
}

/* --- Main test logic --- */
async function runFragmentationTest(
  host: string,
  port: number,
  useTls: boolean,
) {
  const testRequests = buildTestRequests(host);
  const encoder = new TextEncoder();

  // Collect all splits from all requests
  interface Split {
    requestName: string;
    offset: number; // byte count of first part
    first: Uint8Array;
    second: Uint8Array;
  }

  const allSplits: Split[] = [];

  for (const { name, request } of testRequests) {
    const raw = encoder.encode(request);
    if (raw.length < 2) continue; // no split possible
    for (let i = 1; i < raw.length; i++) {
      allSplits.push({
        requestName: name,
        offset: i,
        first: raw.slice(0, i),
        second: raw.slice(i),
      });
    }
  }

  if (allSplits.length === 0) {
    console.log("No splits to test (requests too short).");
    return;
  }

  console.log(
    `Testing ${allSplits.length} fragmentation offsets across ${
      testRequests.length
    } request types...`,
  );

  // Step 1: open a connection for each split and send the first part
  const handles: Array<{
    conn?: Deno.Conn; // present only if connection succeeded
    second: Uint8Array;
    offset: number;
    requestName: string;
    error?: unknown; // present if connection or first write failed
  }> = [];

  const connect = useTls ? Deno.connectTls : Deno.connect;

  for (const split of allSplits) {
    let conn: Deno.Conn | undefined;
    let error: unknown = undefined;
    try {
      conn = await connect({ hostname: host, port });
      await writeAll(conn, split.first);
    } catch (err) {
      error = err;
      // close connection if it was opened
      if (conn) conn.close();
    }
    handles.push({
      conn,
      second: split.second,
      offset: split.offset,
      requestName: split.requestName,
      error,
    });
  }

  // Step 2: wait 1 second
  console.log("All first parts sent. Waiting 1 second...");
  await new Promise((resolve) => setTimeout(resolve, 1000));

  // Step 3: send the second part and read the response
  const results = await Promise.all(
    handles.map(async (handle) => {
      // If we already have an error, don't try to send the second part
      if (handle.error) {
        return {
          requestName: handle.requestName,
          offset: handle.offset,
          status: -1,
          error: handle.error,
          response: "",
        };
      }

      const { conn, second, offset, requestName } = handle;
      // conn must be defined here
      try {
        await writeAll(conn!, second);
        const response = await readAll(conn!);
        conn!.close();

        // Parse status line
        const responseStr = new TextDecoder().decode(response);
        const firstLine = responseStr.split("\r\n")[0] || "";
        let status = -1;
        const parts = firstLine.split(" ");
        if (parts.length >= 2) {
          status = parseInt(parts[1], 10);
        }

        return {
          requestName,
          offset,
          status,
          error: null,
          response: responseStr,
        };
      } catch (err) {
        if (conn) conn.close();
        return {
          requestName,
          offset,
          status: -1,
          error: err,
          response: "",
        };
      }
    }),
  );

  // Step 4: analyze results
  const failures = results.filter(
    (r) => r.status !== 200,
  );

  if (failures.length === 0) {
    console.log("✅ All OK – server handles fragmentation perfectly.");
    return;
  }

  // Report failures
  console.error(`❌ ${failures.length} fragmentation offset(s) failed.`);
  for (const fail of failures) {
    // Find the original split data
    const split = allSplits.find(
      (s) =>
        s.requestName === fail.requestName && s.offset === fail.offset,
    );
    if (!split) continue;

    const firstStr = new TextDecoder().decode(split.first);
    const secondStr = new TextDecoder().decode(split.second);

    console.error(`\n--- Failure ---`);
    console.error(`Request: ${fail.requestName}`);
    console.error(`Offset: ${fail.offset} bytes`);
    console.error(`Status: ${fail.status}`);
    if (fail.error) {
      console.error(`Error: ${fail.error}`);
    }
    console.error("Split point marked with '>>> SPLIT HERE <<<':");
    console.error(
      firstStr.replace(/\n/g, "\\n").replace(/\r/g, "\\r") +
        " >>> SPLIT HERE <<< " +
        secondStr.replace(/\n/g, "\\n").replace(/\r/g, "\\r"),
    );
    console.error("Server response (truncated):");
    console.error(
      fail.response.slice(0, 200) + (fail.response.length > 200 ? "..." : ""),
    );
  }
}

/* --- Run the test --- */
if (import.meta.main) {
  try {
    await runFragmentationTest(host, port, useTls);
  } catch (err) {
    console.error("Fatal error:", err);
    Deno.exit(1);
  }
}