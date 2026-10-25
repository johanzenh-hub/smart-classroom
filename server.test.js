const assert = require("node:assert/strict");
const { after, before, test } = require("node:test");
const { createApp } = require("./server");

const deviceToken = "a".repeat(64);
const dashboardPassword = "a-unique-dashboard-password";
const basicAuthorization = `Basic ${Buffer.from(`admin:${dashboardPassword}`).toString("base64")}`;
let server;
let baseUrl;

before(async () => {
  server = createApp({ deviceToken, dashboardUsername: "admin", dashboardPassword });
  await new Promise(resolve => server.listen(0, "127.0.0.1", resolve));
  baseUrl = `http://127.0.0.1:${server.address().port}`;
});

after(async () => {
  await new Promise((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
});

test("requires a long device token and dashboard password", () => {
  assert.throws(
    () => createApp({ deviceToken: "short", dashboardUsername: "admin", dashboardPassword }),
    /DEVICE_TOKEN/,
  );
  assert.throws(
    () => createApp({ deviceToken, dashboardUsername: "admin", dashboardPassword: "short" }),
    /DASHBOARD_PASSWORD/,
  );
});

test("protects the dashboard and dashboard API with Basic authentication", async () => {
  const page = await fetch(baseUrl);
  assert.equal(page.status, 401);
  assert.match(page.headers.get("www-authenticate"), /Basic/);
  const status = await fetch(`${baseUrl}/api/status`);
  assert.equal(status.status, 401);
  const authorized = await fetch(baseUrl, { headers: { Authorization: basicAuthorization } });
  assert.equal(authorized.status, 200);
  assert.match(await authorized.text(), /Remote ESP32 status and controls/);
});

test("requires a device bearer token and publishes validated status", async () => {
  const denied = await fetch(`${baseUrl}/device/command`);
  assert.equal(denied.status, 401);

  const auth = { Authorization: `Bearer ${deviceToken}` };
  const status = {
    pir: false,
    relay: true,
    automation: true,
    manualOverride: false,
    greenLed: true,
    redLed: false,
    buzzer: false,
    secondsRemaining: 4,
    offDelaySeconds: 10,
  };
  const update = await fetch(`${baseUrl}/device/status`, {
    method: "POST",
    headers: { ...auth, "Content-Type": "application/json" },
    body: JSON.stringify(status),
  });
  assert.equal(update.status, 204);
  const result = await fetch(`${baseUrl}/api/status`, { headers: { Authorization: basicAuthorization } });
  const payload = await result.json();
  assert.equal(payload.online, true);
  assert.deepEqual(payload.status, status);
});

test("queues authenticated controls until the device acknowledges them", async () => {
  const auth = { Authorization: `Bearer ${deviceToken}` };
  const unauthorized = await fetch(`${baseUrl}/api/command`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ type: "bulb", value: true }),
  });
  assert.equal(unauthorized.status, 401);

  const invalid = await fetch(`${baseUrl}/api/command`, {
    method: "POST",
    headers: { Authorization: basicAuthorization, "Content-Type": "application/json" },
    body: JSON.stringify({ type: "unknown", value: true }),
  });
  assert.equal(invalid.status, 400);

  const queued = await fetch(`${baseUrl}/api/command`, {
    method: "POST",
    headers: { Authorization: basicAuthorization, "Content-Type": "application/json" },
    body: JSON.stringify({ type: "bulb", value: true }),
  });
  assert.equal(queued.status, 202);
  const { id } = await queued.json();

  const commandResponse = await fetch(`${baseUrl}/device/command`, { headers: auth });
  assert.equal(await commandResponse.text(), `${id}|bulb|1`);

  const ack = await fetch(`${baseUrl}/device/ack`, {
    method: "POST",
    headers: { ...auth, "Content-Type": "application/json" },
    body: JSON.stringify({ id }),
  });
  assert.equal(ack.status, 204);
  const empty = await fetch(`${baseUrl}/device/command`, { headers: auth });
  assert.equal(empty.status, 204);
});
