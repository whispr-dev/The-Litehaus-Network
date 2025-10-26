// Litehaus Listener – Singapore
const http = require("http");

const REGION = process.env.LITEHAUS_REGION || "SGP";
const PORT   = parseInt(process.env.LISTENER_PORT || "4577", 10);

function now() { return new Date().toISOString(); }

const server = http.createServer((req, res) => {
  if (req.method === "POST" && req.url === "/ping") {
    let body = "";
    req.on("data", chunk => body += chunk.toString());
    req.on("end", () => {
      try {
        const data = JSON.parse(body);
        console.log(`[${now()}] [Litehaus Singapore] <- from ${data.region}: ${data.signal}`);
      } catch (err) {
        console.error(`[${now()}] Bad JSON: ${err.message}`);
      }
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ status: "ack" }));
    });
  } else if (req.method === "GET" && req.url === "/") {
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("Litehaus Listener OK\n");
  } else {
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("nope\n");
  }
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`[${now()}] [Litehaus Singapore] Listening on port ${PORT}`);
});
