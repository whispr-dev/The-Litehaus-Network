// ultimate-litehaus/server.js
import fetch from 'node-fetch';
import express from 'express';

const REGION = 'SINGAPORE';
const TARGET = 'http://138.68.142.181:4567/ping';
const PORT = 6789;
const INTERVAL = 250; // ms

const app = express();
app.use(express.json());

app.post('/ping', (req, res) => {
  console.log(`[${new Date().toISOString()}] [Litehaus ${REGION}] <- from ${req.body.region}: ${req.body.signal}`);
  res.json({ status: 'ack' });
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`[litehaus-${REGION.toLowerCase()}] Beacon starting on port ${PORT}, region=${REGION}`);
  console.log(`Target listener: ${TARGET}`);
  setInterval(async () => {
    try {
      const body = { region: REGION, signal: `HELLO_FROM_${REGION}` };
      const r = await fetch(TARGET, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const t = await r.text();
      console.log(`[${new Date().toISOString()}] Sent -> ${TARGET} | reply: ${t}`);
    } catch (e) {
      console.error('Ping error:', e.message);
    }
  }, INTERVAL);
});
