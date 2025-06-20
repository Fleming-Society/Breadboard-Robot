#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid     = "";
const char* password = "";

// HTTP server on port 80
WebServer       server(80);
// WebSocket server on port 81
WebSocketsServer webSocket(81);

// Continuous-servo pin definitions
const int SERVO1_PIN = 0;  // Front servo
const int SERVO2_PIN = 1;  // Back servo

// Calibration: adjust these neutral pulses until servos reliably stop
int neutralPulse1 = 1500;
int neutralPulse2 = 1500;

Servo servo1;
Servo servo2;

// Send pulses based on command
// 1 = forward, 2 = backward, 3 = turn left, 4 = turn right, 0 = stop
void sendServoCommand(uint8_t cmd) {
  int pulse1 = neutralPulse1;
  int pulse2 = neutralPulse2;

  switch (cmd) {
    case 1: // forward
      pulse1 += 200;
      pulse2 += 200;
      break;
    case 2: // backward
      pulse1 -= 200;
      pulse2 -= 200;
      break;
    case 3: // turn left
      pulse1 -= 200;
      pulse2 += 200;
      break;
    case 4: // turn right
      pulse1 += 200;
      pulse2 -= 200;
      break;
    default: // stop
      break;
  }

  servo1.writeMicroseconds(pulse1);
  servo2.writeMicroseconds(pulse2);
}

// HTML page with reliable '0' on release
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Omni Directional Robot Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://raw.flemingsociety.com/robot/style.css">
</head>
<body>
  <div class="logo-container">
    <img src="https://raw.flemingsociety.com/logo.png" alt="Logo">
  </div>
  <div class="container">
    <h1>Omni Control</h1>
    <div class="row"><button onmousedown="send(1)" onmouseup="send(0)" ontouchstart="send(1)" ontouchend="send(0)">&#8679;</button></div>
    <div class="row">
      <button onmousedown="send(3)" onmouseup="send(0)" ontouchstart="send(3)" ontouchend="send(0)">&#8678;</button>
      <div class="spacer"></div>
      <button onmousedown="send(4)" onmouseup="send(0)" ontouchstart="send(4)" ontouchend="send(0)">&#8680;</button>
    </div>
    <div class="row"><button onmousedown="send(2)" onmouseup="send(0)" ontouchstart="send(2)" ontouchend="send(0)">&#8681;</button></div>
  </div>
  <script>
    const ws = new WebSocket(`ws://${location.hostname}:81/`);
    function send(cmd) { ws.send(cmd); }
    window.addEventListener('load', () => {
      document.querySelectorAll('button').forEach(b => {
        b.addEventListener('mouseleave', () => ws.send(0));
        b.addEventListener('touchcancel', () => ws.send(0));
      });
    });
  </script>
</body>
</html>
)rawliteral";

// Serve page
void handleRoot() { server.send(200, "text/html", htmlPage); }

// WebSocket event
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  if (type == WStype_TEXT) {
    char buf[4] = {};
    memcpy(buf, payload, min(len, sizeof(buf)-1));
    uint8_t cmd = atoi(buf);
    sendServoCommand(cmd);
  }
}

void setup() {
  Serial.begin(115200);
  // Instructions for calibration
  Serial.println("Servo calibration mode:\n  w/s: adjust both neutrals\n  a/z: front (servo1)\n  d/c: back (servo2)\n  After tuning, power cycle or press any control button.");
  Serial.printf("Neutral1=%d, Neutral2=%d\n", neutralPulse1, neutralPulse2);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

  // HTTP + WS
  server.on("/", handleRoot);
  server.begin();
  webSocket.begin(); webSocket.onEvent(webSocketEvent);

  // Setup servos
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);

  // Initial stop
  sendServoCommand(0);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  // Allow serial-based neutral calibration
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'w': neutralPulse1++; neutralPulse2++; break;
      case 's': neutralPulse1--; neutralPulse2--; break;
      case 'a': neutralPulse1++; break;
      case 'z': neutralPulse1--; break;
      case 'd': neutralPulse2++; break;
      case 'c': neutralPulse2--; break;
      default: break;
    }
    Serial.printf("Neutral1=%d, Neutral2=%d\n", neutralPulse1, neutralPulse2);
    sendServoCommand(0);
  }
}
