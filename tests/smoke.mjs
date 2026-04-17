/* This smoke test runs against the dedicated SmokeTest program */

var crc32 = (function () {
    var table = new Uint32Array(256);
    for (var i = 256; i--;) {
        var tmp = i;
        for (var k = 8; k--;) {
            tmp = tmp & 1 ? 3988292384 ^ tmp >>> 1 : tmp >>> 1;
        }
        table[i] = tmp;
    }
    return function (data) {
        var crc = -1; // Begin with all bits set ( 0xffffffff )
        for (var i = 0, l = data.length; i < l; i++) {
            crc = crc >>> 8 ^ table[crc & 255 ^ data[i]];
        }
        return (crc ^ -1) >>> 0; // Apply binary NOT
    };
})();

async function chunkedCrc32Test(array) {

    console.log("Making chunked request with body size: " + array.length);

    const stream = new ReadableStream(/*{type: "bytes"}, */{
        async start(controller) {
            await 1;
            controller.enqueue(array);
            controller.close();
        },
    });

    const r = await fetch("http://localhost:3000", {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream' },
        body: stream,
        duplex: 'half',
    });

    /* Download the response body (it's a crc32 hash plus newline) */
    const body = await r.body.getReader().read();

    /* Make a crc32 comparison of the two (mind the newline in one!) */
    const got = new TextDecoder().decode(body.value);

    const want = crc32(array).toString(16);
    if (got.toString().slice(0, -1) !== want.toString()) {
        throw new Error("failed chunked test");
    }
}

async function fixedCrc32Test(array) {
    console.log("Making request with body size: " + array.length);

    /* Send it with content-length */
    const res = await fetch("http://localhost:3000", { keepalive: true, headers: { 'Content-Type': 'text/plain' }, method: "POST", body: array });

    /* Download the response body (it's a crc32 hash plus newline) */
    const body = await res.body.getReader().read();

    /* Make a crc32 comparison of the two (mind the newline in one!) */
    const got = new TextDecoder().decode(body.value);
    const want = crc32(array).toString(16);
    if (got.toString().slice(0, -1) !== want.toString()) {
        throw new Error("failed test");
    }
}

async function readBodySlowly(response) {
    const reader = response.body.getReader();
    const chunks = [];
    let total = 0;

    while (true) {
        const {done, value} = await reader.read();
        if (done) {
            break;
        }

        chunks.push(value);
        total += value.length;
        await new Promise((resolve) => setTimeout(resolve, 5));
    }

    const body = new Uint8Array(total);
    let offset = 0;
    for (const chunk of chunks) {
        body.set(chunk, offset);
        offset += chunk.length;
    }
    return body;
}

function expectFilled(body, size, value, label) {
    if (body.length !== size) {
        throw new Error(label + " failed: expected body size " + size + ", got " + body.length);
    }

    for (let i = 0; i < body.length; i++) {
        if (body[i] !== value) {
            throw new Error(label + " failed: unexpected byte at offset " + i);
        }
    }
}

async function streamingWriteTest() {
    console.log("Making streaming write request");
    const res = await fetch("http://localhost:3000/write");
    expectFilled(await readBodySlowly(res), 128 * 1024, "a".charCodeAt(0), "write");
}

async function streamingTryWriteTest() {
    console.log("Making tryWrite request");
    const res = await fetch("http://localhost:3000/trywrite");
    expectFilled(await readBodySlowly(res), 16 * 1024 * 1024, "x".charCodeAt(0), "tryWrite");
}

async function streamingTryWriteEndTest() {
    console.log("Making tryWrite-end request");
    const res = await fetch("http://localhost:3000/trywrite-end");
    expectFilled(await readBodySlowly(res), 32 * 1024 * 1024, "y".charCodeAt(0), "tryWrite-end");
}

/* Maximum chunk size is less than 256mb */
const sizes = [0, 0, 32, 32, 128, 256, 1024, 65536, 1024 * 1024, 1024 * 1024 * 128, 0, 0, 32, 32];
for (let i = 0; i < sizes.length; i++) {

    /* Create buffer with random data */
    const array = new Uint8Array(sizes[i]);
    //if (sizes[i] <= 65536) {
    //self.crypto.getRandomValues(array);
    //} else {
    array.fill(Math.random() * 255);
    //}

    /* Do this for all methods */
    await fixedCrc32Test(array);
    await chunkedCrc32Test(array);
}

await streamingWriteTest();
await streamingTryWriteTest();
await streamingTryWriteEndTest();

console.log("Done!");
