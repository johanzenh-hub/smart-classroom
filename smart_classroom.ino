// Smart Classroom (ESP32): PIR relay automation, local dashboard, and Render cloud dashboard
// Pins: PIR OUT=G27, Relay IN=G26, Buzzer+=G25, Green LED=G18, Red LED=G19
// Serial Monitor: 115200 baud
// Install the "WebSockets" library by Markus Sattler from Arduino Library Manager.

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "device_config.h"

const char RENDER_ROOT_CA[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)PEM";

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
unsigned long lastCloudSnapshot = 0;
unsigned long lastCloudError = 0;
unsigned long lastWifiRetry = 0;
int lastPir = -1;
int lastCountdown = -1;
bool dashboardStarted = false;
bool cloudConfigured = true;

struct CloudCommand {
  char id[33];
  bool automation;
  bool value;
};

struct CloudStatus {
  bool pir;
  bool relay;
  bool automation;
  bool manualOverride;
  bool greenLed;
  bool redLed;
  bool buzzer;
  unsigned long secondsRemaining;
  unsigned long offDelaySeconds;
};

QueueHandle_t cloudCommandQueue = nullptr;
QueueHandle_t cloudStatusQueue = nullptr;

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

int cloudRequest(const char *method, const char *path, const String &payload, String &responseBody) {
  if (!cloudConfigured || WiFi.status() != WL_CONNECTED) {
    return -1;
  }

  WiFiClientSecure client;
  client.setCACert(RENDER_ROOT_CA);
  HTTPClient http;
  String url = String(CLOUD_BASE_URL) + path;
  if (!http.begin(client, url)) {
    return -1;
  }
  http.setConnectTimeout(2500);
  http.setTimeout(2500);
  http.addHeader("Authorization", String("Bearer ") + DEVICE_TOKEN);
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
  }

  int result = strcmp(method, "POST") == 0
      ? http.POST(payload)
      : http.GET();
  if (result > 0) {
    responseBody = http.getString();
  }
  http.end();
  return result;
}

void reportCloudFailure(int result) {
  unsigned long now = millis();
  if (result < 0 || result >= 400) {
    if (now - lastCloudError >= 30000) {
      lastCloudError = now;
      Serial.print("Cloud request failed (HTTP ");
      Serial.print(result);
      Serial.println("). Check Wi-Fi, Render URL, and device token.");
    }
  }
}

void publishCloudStatus(const CloudStatus &status) {
  String payload;
  payload.reserve(220);
  payload = "{\"pir\":";
  payload += status.pir ? "true" : "false";
  payload += ",\"relay\":";
  payload += status.relay ? "true" : "false";
  payload += ",\"automation\":";
  payload += status.automation ? "true" : "false";
  payload += ",\"manualOverride\":";
  payload += status.manualOverride ? "true" : "false";
  payload += ",\"greenLed\":";
  payload += status.greenLed ? "true" : "false";
  payload += ",\"redLed\":";
  payload += status.redLed ? "true" : "false";
  payload += ",\"buzzer\":";
  payload += status.buzzer ? "true" : "false";
  payload += ",\"secondsRemaining\":";
  payload += status.secondsRemaining;
  payload += ",\"offDelaySeconds\":";
  payload += status.offDelaySeconds;
  payload += "}";

  String responseBody;
  int result = cloudRequest("POST", "/device/status", payload, responseBody);
  reportCloudFailure(result);
}

void acknowledgeCloudCommand(const String &commandId) {
  String payload = "{\"id\":\"";
  payload += commandId;
  payload += "\"}";
  String responseBody;
  int result = cloudRequest("POST", "/device/ack", payload, responseBody);
  reportCloudFailure(result);
}

void pollCloudCommand(String &lastAcceptedCommandId) {
  String responseBody;
  int result = cloudRequest("GET", "/device/command", "", responseBody);
  if (result == 204) {
    return;
  }
  if (result != 200) {
    reportCloudFailure(result);
    return;
  }

  int firstSeparator = responseBody.indexOf('|');
  int secondSeparator = responseBody.indexOf('|', firstSeparator + 1);
  if (firstSeparator <= 0 || secondSeparator <= firstSeparator + 1) {
    Serial.println("Cloud command rejected: invalid response format.");
    return;
  }

  String commandId = responseBody.substring(0, firstSeparator);
  String commandType = responseBody.substring(firstSeparator + 1, secondSeparator);
  String commandValue = responseBody.substring(secondSeparator + 1);
  bool validCommandId = commandId.length() == 32;
  for (unsigned int i = 0; validCommandId && i < commandId.length(); i++) {
    char character = commandId.charAt(i);
    validCommandId = (character >= '0' && character <= '9') ||
                     (character >= 'a' && character <= 'f');
  }
  if (!validCommandId || (commandValue != "0" && commandValue != "1") ||
      (commandType != "automation" && commandType != "bulb")) {
    Serial.println("Cloud command rejected: invalid command fields.");
    return;
  }

  if (commandId != lastAcceptedCommandId) {
    CloudCommand command;
    commandId.toCharArray(command.id, sizeof(command.id));
    command.automation = commandType == "automation";
    command.value = commandValue == "1";
    xQueueOverwrite(cloudCommandQueue, &command);
    lastAcceptedCommandId = commandId;
  }
  acknowledgeCloudCommand(commandId);
}

void cloudTask(void *parameter) {
  String lastAcceptedCommandId;
  unsigned long lastCommandPoll = 0;
  unsigned long lastStatusPost = 0;
  CloudStatus latestStatus = {};
  bool hasStatus = false;

  while (true) {
    unsigned long now = millis();
    if (cloudConfigured && WiFi.status() == WL_CONNECTED) {
      if (now - lastCommandPoll >= 1500) {
        lastCommandPoll = now;
        pollCloudCommand(lastAcceptedCommandId);
      }
      if (xQueueReceive(cloudStatusQueue, &latestStatus, 0) == pdTRUE) {
        hasStatus = true;
      }
      if (hasStatus && now - lastStatusPost >= 2500) {
        lastStatusPost = now;
        publishCloudStatus(latestStatus);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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

  cloudConfigured =
      strncmp(CLOUD_BASE_URL, "https://", 8) == 0 &&
      strstr(CLOUD_BASE_URL, "YOUR-RENDER-SERVICE") == nullptr &&
      strlen(DEVICE_TOKEN) >= 32 &&
      strstr(DEVICE_TOKEN, "PASTE_THE_RENDER_DEVICE_TOKEN_HERE") == nullptr;
  if (!cloudConfigured) {
    Serial.println("Cloud controls disabled: configure CLOUD_BASE_URL and DEVICE_TOKEN in device_config.h.");
  } else {
    cloudCommandQueue = xQueueCreate(1, sizeof(CloudCommand));
    cloudStatusQueue = xQueueCreate(1, sizeof(CloudStatus));
    if (cloudCommandQueue == nullptr || cloudStatusQueue == nullptr) {
      cloudConfigured = false;
      Serial.println("Cloud controls disabled: unable to allocate FreeRTOS queues.");
    }
  }

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
  if (cloudConfigured &&
      xTaskCreate(cloudTask, "cloud", 16384, nullptr, 1, nullptr) != pdPASS) {
    cloudConfigured = false;
    Serial.println("Cloud controls disabled: unable to start the network task.");
  }
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

  if (cloudConfigured && cloudCommandQueue != nullptr) {
    CloudCommand command;
    if (xQueueReceive(cloudCommandQueue, &command, 0) == pdTRUE) {
      if (command.automation) {
        setAutomation(command.value);
      } else {
        setManualBulb(command.value);
      }
    }
  }

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

  if (cloudConfigured && cloudStatusQueue != nullptr && now - lastCloudSnapshot >= 1000) {
    lastCloudSnapshot = now;
    CloudStatus status = {
      pir == HIGH,
      relayOn,
      automationEnabled,
      manualBulbOverride,
      greenLedOn,
      redLedOn,
      buzzerOn,
      0,
      OFF_DELAY_MS / 1000
    };
    if (automationEnabled && !manualBulbOverride && relayOn && pir == LOW) {
      unsigned long elapsed = now - lastMotion;
      if (elapsed < OFF_DELAY_MS) {
        status.secondsRemaining = (OFF_DELAY_MS - elapsed + 999) / 1000;
      }
    }
    xQueueOverwrite(cloudStatusQueue, &status);
  }

  delay(20);
}
