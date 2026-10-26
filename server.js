const crypto = require("node:crypto");
const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");

const DASHBOARD_FILE = path.join(__dirname, "dashboard.html");
const MAX_BODY_BYTES = 4096;
const STATUS_FIELDS = [
  "pir",
  "relay",
  "automation",
  "manualOverride",
  "greenLed",
  "redLed",
  "buzzer",
];

function safeEqual(left, right) {
  const leftBuffer = Buffer.from(left);
  const rightBuffer = Buffer.from(right);
  return leftBuffer.length === rightBuffer.length && crypto.timingSafeEqual(leftBuffer, rightBuffer);
}

function readJson(request) {
  return new Promise((resolve, reject) => {
    let body = "";
    request.setEncoding("utf8");
    request.on("data", chunk => {
      body += chunk;
      if (Buffer.byteLength(body) > MAX_BODY_BYTES) {
        reject(new Error("Request body too large"));
        request.destroy();
      }
    });
    request.on("end", () => {
      try {
        resolve(JSON.parse(body));
      } catch {
        reject(new Error("Invalid JSON"));
      }
    });
    request.on("error", reject);
  });
}

function createApp({ deviceToken, dashboardUsername, dashboardPassword }) {
  if (!deviceToken || deviceToken.length < 32) {
    throw new Error("DEVICE_TOKEN must be set to a random value of at least 32 characters.");
  }
  if (!dashboardUsername || !dashboardPassword || dashboardPassword.length < 16) {
    throw new Error("Set DASHBOARD_USERNAME and a DASHBOARD_PASSWORD of at least 16 characters.");
  }

  let latestStatus = null;
  let lastSeen = 0;
  let pendingCommand = null;

  function send(response, status, body, contentType = "application/json; charset=utf-8") {
    response.writeHead(status, {
      "Content-Type": contentType,
      "Cache-Control": "no-store",
      "X-Content-Type-Options": "nosniff",
      "X-Frame-Options": "DENY",
      "Referrer-Policy": "no-referrer",
      "Content-Security-Policy": "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'",
      "Strict-Transport-Security": "max-age=31536000",
    });
    response.end(body);
  }

  function dashboardAuthorized(request, response) {
    const authorization = request.headers.authorization || "";
    const [scheme, encoded] = authorization.split(" ");
    if (scheme === "Basic" && encoded) {
      const decoded = Buffer.from(encoded, "base64").toString("utf8");
      const separator = decoded.indexOf(":");
      if (
        separator >= 0 &&
        safeEqual(decoded.slice(0, separator), dashboardUsername) &&
        safeEqual(decoded.slice(separator + 1), dashboardPassword)
      ) {
        return true;
      }
    }
    response.writeHead(401, {
      "WWW-Authenticate": 'Basic realm="Smart Classroom", charset="UTF-8"',
      "Cache-Control": "no-store",
      "Content-Length": "0",
    });
    response.end();
    return false;
  }

  function deviceAuthorized(request, response) {
    const authorization = request.headers.authorization || "";
    if (authorization.startsWith("Bearer ") && safeEqual(authorization.slice(7), deviceToken)) {
      return true;
    }
    send(response, 401, JSON.stringify({ error: "Unauthorized" }));
    return false;
  }

  const server = http.createServer(async (request, response) => {
    const url = new URL(request.url, "http://localhost");

    if (request.method === "GET" && url.pathname === "/healthz") {
      send(response, 200, "ok", "text/plain; charset=utf-8");
      return;
    }

    if (request.method === "GET" && url.pathname === "/") {
      if (dashboardAuthorized(request, response)) {
        send(response, 200, fs.readFileSync(DASHBOARD_FILE), "text/html; charset=utf-8");
      }
      return;
    }

    if (url.pathname.startsWith("/api/") && !dashboardAuthorized(request, response)) {
      return;
    }

    if (request.method === "GET" && url.pathname === "/api/status") {
      send(response, 200, JSON.stringify({
        online: lastSeen > 0 && Date.now() - lastSeen < 10000,
        lastSeen: lastSeen || null,
        status: latestStatus,
      }));
      return;
    }

    if (request.method === "POST" && url.pathname === "/api/command") {
      try {
        const body = await readJson(request);
        const validCommand =
          (["automation", "bulb"].includes(body?.type) && typeof body.value === "boolean") ||
          (body?.type === "delay" && Number.isInteger(body.value) &&
            [10, 60, 600, 1800].includes(body.value));
        if (
          !body ||
          !validCommand
        ) {
          send(response, 400, JSON.stringify({
            error: "Expected automation|bulb with a boolean value, or delay with 10, 60, 600, or 1800 seconds.",
          }));
          return;
        }
        pendingCommand = {
          id: crypto.randomBytes(16).toString("hex"),
          type: body.type,
          value: body.value,
        };
        send(response, 202, JSON.stringify({ id: pendingCommand.id }));
      } catch (error) {
        send(response, error.message === "Request body too large" ? 413 : 400, JSON.stringify({ error: error.message }));
      }
      return;
    }

    if (url.pathname.startsWith("/device/") && !deviceAuthorized(request, response)) {
      return;
    }

    if (request.method === "GET" && url.pathname === "/device/command") {
      if (!pendingCommand) {
        response.writeHead(204, { "Cache-Control": "no-store" });
        response.end();
        return;
      }
      const { id, type, value } = pendingCommand;
      const encodedValue = type === "delay" ? value : value ? 1 : 0;
      send(response, 200, `${id}|${type}|${encodedValue}`, "text/plain; charset=utf-8");
      return;
    }

    if (request.method === "POST" && url.pathname === "/device/ack") {
      try {
        const body = await readJson(request);
        if (typeof body?.id !== "string" || !/^[a-f0-9]{32}$/.test(body.id)) {
          send(response, 400, JSON.stringify({ error: "Expected a valid command id." }));
          return;
        }
        if (pendingCommand?.id === body.id) pendingCommand = null;
        send(response, 204, "");
      } catch (error) {
        send(response, error.message === "Request body too large" ? 413 : 400, JSON.stringify({ error: error.message }));
      }
      return;
    }

    if (request.method === "POST" && url.pathname === "/device/status") {
      try {
        const body = await readJson(request);
        const validFlags = STATUS_FIELDS.every(field => typeof body?.[field] === "boolean");
        const validCountdown =
          Number.isInteger(body?.secondsRemaining) &&
          body.secondsRemaining >= 0 &&
          body.secondsRemaining <= 3600;
        const validDelay =
          Number.isInteger(body?.offDelaySeconds) &&
          body.offDelaySeconds >= 1 &&
          body.offDelaySeconds <= 3600;
        if (!validFlags || !validCountdown || !validDelay) {
          send(response, 400, JSON.stringify({ error: "Invalid device status." }));
          return;
        }
        latestStatus = Object.fromEntries(
          [...STATUS_FIELDS, "secondsRemaining", "offDelaySeconds"].map(field => [field, body[field]]),
        );
        lastSeen = Date.now();
        send(response, 204, "");
      } catch (error) {
        send(response, error.message === "Request body too large" ? 413 : 400, JSON.stringify({ error: error.message }));
      }
      return;
    }

    send(response, 404, JSON.stringify({ error: "Not found" }));
  });

  return server;
}

if (require.main === module) {
  const server = createApp({
    deviceToken: process.env.DEVICE_TOKEN,
    dashboardUsername: process.env.DASHBOARD_USERNAME || "admin",
    dashboardPassword: process.env.DASHBOARD_PASSWORD,
  });
  const port = Number(process.env.PORT || 3000);
  server.listen(port, "0.0.0.0", () => {
    console.log(`Smart Classroom dashboard listening on port ${port}.`);
  });
}

module.exports = { createApp };
