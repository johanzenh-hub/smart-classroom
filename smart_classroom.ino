// Smart Classroom (ESP32): PIR -> Relay with off-delay, Serial Monitor, and WebSocket dashboard
// Pins: PIR OUT=G27, Relay IN=G26, Buzzer+=G25, Green LED=G18, Red LED=G19
// Serial Monitor: 115200 baud
// Install the "WebSockets" library by Markus Sattler from Arduino Library Manager.

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

WebServer server(80);
WebSocketsServer webSocket(81);

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Smart Classroom</title>
  <style>
    *{box-sizing:border-box}body{margin:0;padding:24px;background:#101827;color:#e8eef8;font:16px Arial,sans-serif}
    main{max-width:760px;margin:auto}h1{margin-bottom:6px}.muted{color:#9aabc2}
    .status{margin:20px 0;padding:12px 16px;border-radius:10px;background:#253247}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px}
    .card{padding:20px;border-radius:12px;background:#1b293b}.label{color:#9aabc2;font-size:14px}
    .value{margin-top:10px;font-size:24px;font-weight:bold}.on{color:#67e8a5}.off{color:#ff8585}
    .timer{height:8px;margin-top:14px;overflow:hidden;border-radius:8px;background:#34445b}
    .bar{height:100%;width:0;background:#67e8a5;transition:width .2s}
    .controls{margin-top:14px;display:flex;gap:8px}
    button{flex:1;padding:10px;border:0;border-radius:8px;background:#34445b;color:#e8eef8;font-weight:bold;cursor:pointer}
    button.selected{background:#16834f}button:disabled{opacity:.5;cursor:wait}
    #controlStatus{min-height:20px;margin-top:12px}
  </style>
</head>
<body>
  <main>
    <h1>Smart Classroom</h1>
    <div class="muted">Live ESP32 sensor and output status</div>
    <div class="status" id="connection">Connecting to ESP32...</div>
    <section class="grid">
      <article class="card"><div class="label">Motion sensor</div><div class="value" id="pir">--</div></article>
      <article class="card">
        <div class="label">Automation</div><div class="value" id="automation">--</div>
        <div class="controls"><button id="automationOn">ON</button><button id="automationOff">OFF</button></div>
      </article>
      <article class="card">
        <div class="label">Bulb / relay</div><div class="value" id="relay">--</div>
        <div class="label" id="bulbMode">--</div>
        <div class="controls"><button id="bulbOn">ON</button><button id="bulbOff">OFF</button></div>
        <div class="label">Manual setting stays active until automation is toggled.</div>
      </article>
      <article class="card"><div class="label">Green LED</div><div class="value" id="green">--</div></article>
      <article class="card"><div class="label">Red LED</div><div class="value" id="red">--</div></article>
      <article class="card"><div class="label">Buzzer</div><div class="value" id="buzzer">--</div></article>
      <article class="card">
        <div class="label">Time until relay turns off</div>
        <div class="value" id="countdown">--</div>
        <div class="timer"><div class="bar" id="bar"></div></div>
      </article>
    </section>
    <div class="muted" id="controlStatus" role="status"></div>
  </main>
  <script>
    const connection = document.getElementById('connection');
    const controlStatus = document.getElementById('controlStatus');
    const controlButtons = [...document.querySelectorAll('.controls button')];
    const sendControl = async (control, value) => {
      controlButtons.forEach(button => { button.disabled = true; });
      controlStatus.textContent = 'Sending command...';
      try {
        const response = await fetch(`/api/${control}?${control === 'automation' ? 'enabled' : 'on'}=${value ? '1' : '0'}`, { method: 'POST' });
        if (!response.ok) throw new Error(`Command failed (${response.status})`);
        controlStatus.textContent = 'Command sent.';
      } catch (error) {
        controlStatus.textContent = error.message;
      } finally {
        controlButtons.forEach(button => { button.disabled = false; });
      }
    };
    document.getElementById('automationOn').onclick = () => sendControl('automation', true);
    document.getElementById('automationOff').onclick = () => sendControl('automation', false);
    document.getElementById('bulbOn').onclick = () => sendControl('bulb', true);
    document.getElementById('bulbOff').onclick = () => sendControl('bulb', false);
    const connect = () => {
      const socket = new WebSocket(`ws://${location.hostname}:81/`);
      socket.onopen = () => { connection.textContent = 'Connected to ESP32'; connection.className = 'status on'; };
      socket.onclose = () => {
        connection.textContent = 'Disconnected - retrying...';
        connection.className = 'status off';
        setTimeout(connect, 2000);
      };
      socket.onerror = () => socket.close();
      socket.onmessage = event => {
        const data = JSON.parse(event.data);
        const show = (id, active, yes, no) => {
          const element = document.getElementById(id);
          element.textContent = active ? yes : no;
          element.className = `value ${active ? 'on' : 'off'}`;
        };
        show('pir', data.pir, 'MOTION', 'NO MOTION');
        show('relay', data.relay, 'ON', 'OFF');
        show('automation', data.automation, 'ON', 'OFF');
        document.getElementById('bulbMode').textContent =
          data.manualOverride ? 'Manual control' : 'Automatic control';
        document.getElementById('automationOn').classList.toggle('selected', data.automation);
        document.getElementById('automationOff').classList.toggle('selected', !data.automation);
        document.getElementById('bulbOn').classList.toggle('selected', data.relay);
        document.getElementById('bulbOff').classList.toggle('selected', !data.relay);
        show('green', data.greenLed, 'ON', 'OFF');
        show('red', data.redLed, 'ON', 'OFF');
        show('buzzer', data.buzzer, 'RINGING', 'OFF');
        document.getElementById('countdown').textContent =
          data.automation && !data.manualOverride && data.relay && !data.pir
            ? `${data.secondsRemaining} s` : '--';
        document.getElementById('bar').style.width =
          data.automation && !data.manualOverride && data.relay && !data.pir
            ? `${(data.secondsRemaining / data.offDelaySeconds) * 100}%` : '0%';
      };
    };
    connect();
  </script>
</body>
</html>
)rawliteral";

const int PIR_PIN    = 27;
const int RELAY_PIN  = 26;
const int BUZZER_PIN = 25;
const int GREEN_LED  = 18;
const int RED_LED    = 19;

// ---- Settings ----
const bool DEBUG            = true;    // Print status every 500 ms
const bool RELAY_ACTIVE_LOW = true;    // Flip to false if the relay acts backwards
const unsigned long OFF_DELAY_MS = 10000;  // Time without motion before turning off
const unsigned long WARMUP_MS    = 10000;  // PIR stabilization
const unsigned long BUZZER_DURATION_MS = 1000;

bool relayOn = false;
bool automationEnabled = true;
bool manualBulbOverride = false;
bool greenLedOn = false;
bool redLedOn = true;
bool buzzerOn = false;
unsigned long lastMotion = 0;
unsigned long buzzerStartedAt = 0;
unsigned long lastPrint = 0;
unsigned long lastBroadcast = 0;
unsigned long lastWifiRetry = 0;
int lastPir = -1;
int lastCountdown = -1;
bool dashboardStarted = false;

void setRelay(bool on) {
  digitalWrite(RELAY_PIN, (on == RELAY_ACTIVE_LOW) ? LOW : HIGH);
  greenLedOn = on;
  redLedOn = !on;
  digitalWrite(GREEN_LED, greenLedOn ? HIGH : LOW);
  digitalWrite(RED_LED, redLedOn ? HIGH : LOW);
  relayOn = on;
}

void setAutomation(bool enabled) {
  automationEnabled = enabled;
  manualBulbOverride = false;
  if (automationEnabled) {
    lastMotion = millis();
  } else {
    setRelay(false);
    buzzerOn = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
  Serial.print("AUTOMATION: ");
  Serial.println(automationEnabled ? "ON" : "OFF");
}

void setManualBulb(bool on) {
  manualBulbOverride = true;
  setRelay(on);
  Serial.print("BULB: ");
  Serial.println(relayOn ? "ON (manual)" : "OFF (manual)");
}

void startBuzzer() {
  buzzerOn = true;
  buzzerStartedAt = millis();
  digitalWrite(BUZZER_PIN, HIGH);
  if (DEBUG) Serial.println("BUZZER: beep");
}

void updateBuzzer(unsigned long now) {
  if (buzzerOn && now - buzzerStartedAt >= BUZZER_DURATION_MS) {
  buzzerOn = false;
  digitalWrite(BUZZER_PIN, LOW);
  if (DEBUG) Serial.println("BUZZER: off");
  }
}

void broadcastStatus(unsigned long now, int pir, bool greenLedOn, bool redLedOn) {
  unsigned long secondsRemaining = 0;
  if (automationEnabled && !manualBulbOverride && relayOn && pir == LOW) {
    unsigned long elapsed = now - lastMotion;
    if (elapsed < OFF_DELAY_MS) {
      secondsRemaining = (OFF_DELAY_MS - elapsed + 999) / 1000;
    }
  }

  String message;
  message.reserve(160);
  message = "{\"pir\":";
  message += pir == HIGH ? "true" : "false";
  message += ",\"relay\":";
  message += relayOn ? "true" : "false";
  message += ",\"automation\":";
  message += automationEnabled ? "true" : "false";
  message += ",\"manualOverride\":";
  message += manualBulbOverride ? "true" : "false";
  message += ",\"greenLed\":";
  message += greenLedOn ? "true" : "false";
  message += ",\"redLed\":";
  message += redLedOn ? "true" : "false";
  message += ",\"buzzer\":";
  message += buzzerOn ? "true" : "false";
  message += ",\"secondsRemaining\":";
  message += secondsRemaining;
  message += ",\"offDelaySeconds\":";
  message += OFF_DELAY_MS / 1000;
  message += "}";
  webSocket.broadcastTXT(message);
}

void startDashboard() {
  if (dashboardStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/api/automation", HTTP_POST, []() {
    if (!server.hasArg("enabled") || (server.arg("enabled") != "0" && server.arg("enabled") != "1")) {
      server.send(400, "text/plain", "Expected enabled=0 or enabled=1");
      return;
    }
    setAutomation(server.arg("enabled") == "1");
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/bulb", HTTP_POST, []() {
    if (!server.hasArg("on") || (server.arg("on") != "0" && server.arg("on") != "1")) {
      server.send(400, "text/plain", "Expected on=0 or on=1");
      return;
    }
    setManualBulb(server.arg("on") == "1");
    server.send(200, "text/plain", "OK");
  });
  server.begin();
  webSocket.begin();
  dashboardStarted = true;
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());
  Serial.println("WebSocket server started on port 81.");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Set relay OFF level first, then make it an output to avoid a startup click.
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  setRelay(false);

  Serial.println("=== Smart Classroom ===");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    startDashboard();
  } else {
    Serial.println("Wi-Fi connection failed. Dashboard unavailable; PIR/relay will continue.");
  }

  Serial.print("Relay mode: ");
  Serial.println(RELAY_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
  Serial.println("PIR warming up (10 s). Stay away from the sensor...");
  for (int s = WARMUP_MS / 1000; s > 0; s--) {
    if (DEBUG) {
      Serial.print("Warm-up: ");
      Serial.print(s);
      Serial.print(" s | PIR raw = ");
      Serial.println(digitalRead(PIR_PIN));
    }
    delay(1000);
  }
  Serial.println("Ready.");
}

void loop() {
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    startDashboard();
  } else if (now - lastWifiRetry >= 10000) {
    lastWifiRetry = now;
    Serial.println("Retrying Wi-Fi connection...");
    WiFi.reconnect();
  }

  if (dashboardStarted && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    webSocket.loop();
  }
  updateBuzzer(now);

  int pir = digitalRead(PIR_PIN);

  if (pir != lastPir) {
    Serial.print(">>> PIR changed to ");
    Serial.println(pir == HIGH ? "HIGH (motion)" : "LOW (no motion)");
    if (pir == HIGH && automationEnabled && !manualBulbOverride) {
      startBuzzer();
      Serial.println("BUZZER: motion detected");
    }
    lastPir = pir;
  }

  if (automationEnabled && !manualBulbOverride && pir == HIGH) {
    lastMotion = now;
    lastCountdown = -1;
    if (!relayOn) {
      setRelay(true);
      Serial.println("RELAY: ON (motion detected)");
    }
  }

  if (automationEnabled && !manualBulbOverride && relayOn && (now - lastMotion >= OFF_DELAY_MS)) {
    setRelay(false);
    Serial.print("RELAY: OFF (no motion for ");
    Serial.print(OFF_DELAY_MS / 1000);
    Serial.println(" s)");
    startBuzzer();
  }

  if (automationEnabled && !manualBulbOverride && relayOn && pir == LOW) {
    unsigned long elapsed = now - lastMotion;
    int secondsRemaining =
        (OFF_DELAY_MS - elapsed + 999) / 1000;
    if (secondsRemaining != lastCountdown) {
      Serial.print("No motion: relay turns off in ");
      Serial.print(secondsRemaining);
      Serial.println(" s");
      lastCountdown = secondsRemaining;
    }
  }

  if (DEBUG && now - lastPrint >= 500) {
    lastPrint = now;
    Serial.print("PIR=");
    Serial.print(pir);
    Serial.print(" | Relay=");
    Serial.print(relayOn ? "ON" : "OFF");
    Serial.print(" | Green LED=");
    Serial.print(greenLedOn ? "ON" : "OFF");
    Serial.print(" | Red LED=");
    Serial.print(redLedOn ? "ON" : "OFF");
    Serial.print(" | Buzzer=");
    Serial.print(buzzerOn ? "RINGING" : "OFF");
    Serial.print(" | Pin26=");
    Serial.print(digitalRead(RELAY_PIN));
    if (automationEnabled && !manualBulbOverride && relayOn && pir == LOW) {
      Serial.print(" | Off in ");
      Serial.print((OFF_DELAY_MS - (now - lastMotion) + 999) / 1000);
      Serial.print(" s");
    }
    Serial.println();
  }

  if (dashboardStarted && WiFi.status() == WL_CONNECTED && now - lastBroadcast >= 500) {
    lastBroadcast = now;
    broadcastStatus(now, pir, greenLedOn, redLedOn);
  }

  delay(20);
}
