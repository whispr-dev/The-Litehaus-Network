// /opt/litehaus/litehaus-node.js

const http = require('http');
const { spawnSync } = require('child_process');

const REGION = process.env.REGION_NAME || "UNKNOWN";
const PORT = parseInt(process.env.LISTEN_PORT || "4567", 10);
const INTERVAL = parseInt(process.env.PING_INTERVAL_MS || "1000", 10);

// parse peer list
let PEERS = [];
try {
  PEERS = JSON.parse(process.env.PEERS_JSON || "[]");
} catch (e) {
  console.error("Could not parse PEERS_JSON:", e.message);
}

// in-memory recent events for dashboard
// shape: { ts, type: "in"|"out", peer, rttMs?, raw }
const recent = [];

// helper: keep recent array bounded
function addEvent(evt) {
  recent.push({ ...evt, ts: new Date().toISOString() });
  if (recent.length > 2000) {
    recent.splice(0, recent.length - 2000);
  }
}

// run helper binary to "process" payload (placeholder hook)
function runLitehausBinary(inputObj) {
  // we feed JSON to /opt/litehaus/litehaus --pretty or whatever flag
  // for now, just call it with no args and return stdout (we assume it prints something)
  try {
    const child = spawnSync('/opt/litehaus/litehaus', [], {
      input: JSON.stringify(inputObj),
      encoding: 'utf8'
    });
    if (child.error) {
      return { ok:false, error: child.error.message };
    }
    return { ok:true, output: child.stdout.trim() };
  } catch (err) {
    return { ok:false, error: err.message };
  }
}

// HTTP server: handle /ping and /status
const server = http.createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/ping') {
    // collect body
    let body = '';
    req.on('data', chunk => (body += chunk));
    req.on('end', () => {
      let data;
      try {
        data = JSON.parse(body || '{}');
      } catch (e) {
        res.writeHead(400, {'Content-Type':'application/json'});
        res.end(JSON.stringify({error:'bad json'}));
        return;
      }

      const fromRegion = data.from || data.region || "UNKNOWN";

      // run helper binary just to prove integration
      const processed = runLitehausBinary({
        action: "ingress",
        from: fromRegion,
        to: REGION,
        sentAt: data.ts || null,
        receivedAt: Date.now()
      });

      addEvent({
        type: "in",
        peer: fromRegion,
        raw: data,
        processed
      });

      res.writeHead(200, {'Content-Type':'application/json'});
      res.end(JSON.stringify({ status: 'ack', me: REGION }));
    });
    return;
  }

  if (req.method === 'GET' && req.url === '/status') {
    // return last ~20 events plus who we ping
    const last20 = recent.slice(-20);
    res.writeHead(200, {'Content-Type':'application/json'});
    res.end(JSON.stringify({
      region: REGION,
      peers: PEERS.map(p => p.name),
      recent: last20
    }));
    return;
  }

  // default 404
  res.writeHead(404, {'Content-Type':'text/plain'});
  res.end('Litehaus node up, but no such endpoint.\n');
});

// start listening
server.listen(PORT, '0.0.0.0', () => {
  console.log(`[${REGION}] listening on 0.0.0.0:${PORT}`);
});

// outbound heartbeat loop
async function sendHeartbeat(peer) {
  const start = Date.now();
  const payload = {
    from: REGION,
    ts: start
  };

  // we’ll do raw Node http instead of fetch to avoid version hell
  return new Promise((resolve) => {
    try {
      const urlObj = new URL(peer.url);
      const req = http.request({
        hostname: urlObj.hostname,
        port: urlObj.port || 80,
        path: urlObj.pathname,
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        timeout: 1500
      }, (resp) => {
        let respData = '';
        resp.on('data', c => respData += c);
        resp.on('end', () => {
          const rtt = Date.now() - start;

          addEvent({
            type: "out",
            peer: peer.name,
            rttMs: rtt,
            raw: { statusCode: resp.statusCode, body: respData }
          });

          resolve();
        });
      });

      req.on('timeout', () => {
        addEvent({
          type: "out",
          peer: peer.name,
          rttMs: null,
          raw: { error: 'timeout' }
        });
        req.destroy();
        resolve();
      });

      req.on('error', (err) => {
        addEvent({
          type: "out",
          peer: peer.name,
          rttMs: null,
          raw: { error: err.message }
        });
        resolve();
      });

      req.write(JSON.stringify(payload));
      req.end();
    } catch (err) {
      addEvent({
        type: "out",
        peer: peer.name,
        rttMs: null,
        raw: { error: err.message }
      });
      resolve();
    }
  });
}

// scheduler: ping all peers forever
setInterval(() => {
  PEERS.forEach(peer => {
    sendHeartbeat(peer);
  });
}, INTERVAL);