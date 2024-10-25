const encoder = new TextEncoder();
const decoder = new TextDecoder();

// Define test cases
interface TestCase {
    request: string;
    description: string;
    expectedStatus: [number, number][];
}

const testCases: TestCase[] = [
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
        description: "Valid GET request",
        expectedStatus: [[200, 299]],
    },
    {
        request: "GET / HTTP/1.1\r\nhoSt:\texample.com\r\nempty:\r\n\r\n",
        description: "Valid GET request with edge cases",
        expectedStatus: [[200, 299]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nX-Invalid[]: test\r\n\r\n",
        description: "Invalid header characters",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/1.1\r\nContent-Length: 5\r\n\r\n",
        description: "Missing Host header",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: -123456789123456789123456789\r\n\r\n",
        description: "Overflowing negative Content-Length header",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: -1234\r\n\r\n",
        description: "Negative Content-Length header",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: abc\r\n\r\n",
        description: "Non-numeric Content-Length header",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nX-Empty-Header: \r\n\r\n",
        description: "Empty header value",
        expectedStatus: [[200, 299]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nX-Bad-Control-Char: test\x07\r\n\r\n",
        description: "Header containing invalid control character",
        expectedStatus: [[400, 499]],
    },
    {
        request: "GET / HTTP/9.9\r\nHost: example.com\r\n\r\n",
        description: "Invalid HTTP version",
        expectedStatus: [[400, 499], [500, 599]],
    },
    {
        request: "Extra lineGET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
        description: "Invalid prefix of request",
        expectedStatus: [[400, 499], [500, 599]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\n\rSome-Header: Test\r\n\r\n",
        description: "Invalid line ending",
        expectedStatus: [[400, 499]],
    },
    {
        request: "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello",
        description: "Valid POST request with body",
        expectedStatus: [[200, 299], [404, 404]],
    },
    {
        request: "GET / HTTP/1.1\r\nHost: example.com\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n",
        description: "Conflicting Transfer-Encoding and Content-Length",
        expectedStatus: [[400, 499]],
    },
];

// Get host and port from command-line arguments
const [host, port] = Deno.args;
if (!host || !port) {
    console.error("Usage: deno run --allow-net tcp_http_test.ts <host> <port>");
    Deno.exit(1);
}

// Run all test cases in parallel
async function runTests() {
    const results = await Promise.all(
        testCases.map((testCase) => runTestCase(testCase, host, parseInt(port, 10)))
    );

    const passedCount = results.filter((result) => result).length;
    console.log(`\n${passedCount} out of ${testCases.length} tests passed.`);
}

// Run a single test case with a 3-second timeout on reading
async function runTestCase(testCase: TestCase, host: string, port: number): Promise<boolean> {
    try {
        const conn = await Deno.connect({ hostname: host, port });

        // Send the request
        await conn.write(encoder.encode(testCase.request));

        // Set up a read timeout promise
        const readTimeout = new Promise<boolean>((resolve) => {
            const timeoutId = setTimeout(() => {
                console.error(`❌ ${testCase.description}: Read operation timed out`);
                conn.close(); // Ensure the connection is closed on timeout
                resolve(false);
            }, 500);

            const readPromise = (async () => {
                const buffer = new Uint8Array(1024);
                try {
                    const bytesRead = await conn.read(buffer);

                    // Clear the timeout if read completes
                    clearTimeout(timeoutId);
                    const response = decoder.decode(buffer.subarray(0, bytesRead || 0));
                    const statusCode = parseStatusCode(response);

                    const isSuccess = testCase.expectedStatus.some(
                        ([min, max]) => statusCode >= min && statusCode <= max
                    );

                    console.log(
                        `${isSuccess ? "✅" : "❌"} ${testCase.description}: Response Status Code ${statusCode}, Expected ranges: ${JSON.stringify(testCase.expectedStatus)}`
                    );
                    return resolve(isSuccess);
                } catch {

                }
            })();
        });

        // Wait for the read operation or timeout
        return await readTimeout;

    } catch (error) {
        console.error(`Error in test "${testCase.description}":`, error);
        return false;
    }
}


// Parse the HTTP status code from the response
function parseStatusCode(response: string): number {
    const statusLine = response.split("\r\n")[0];
    const match = statusLine.match(/HTTP\/1\.\d (\d{3})/);
    return match ? parseInt(match[1], 10) : 0;
}

// Run all tests
runTests();
