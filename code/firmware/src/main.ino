#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>         // ESP32 Servo library

// WiFi credentials
const char* ssid     = "YungHub";
const char* password = "yungyung";

// HTTP server on port 80
WebServer server(80);
// WebSocket server on port 81
WebSocketsServer webSocket(81);

// Servo objects
Servo servoLeft;
Servo servoRight;

// Servo control pins (signal pins)
const int SERVO_LEFT_PIN  = D0;
const int SERVO_RIGHT_PIN = D1;

// Pulse widths for continuous rotation servos
const int SERVO_STOP  = 90;   // 1.5 ms pulse = stop
const int SERVO_FASTF = 135;  // faster forward
const int SERVO_FASTB = 45;   // faster backward

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    // Read up to 3 characters of command
    char buf[4] = {0};
    memcpy(buf, payload, min(length, sizeof(buf) - 1));
    uint8_t cmd = atoi(buf);
    sendServoCommand(cmd);
  }
}

// Map a command code to servo outputs
// 1 = forward, 2 = backward, 3 = turn left, 4 = turn right, default = stop
void sendServoCommand(uint8_t cmd) {
  Serial.printf("Servo cmd: %u\n", cmd);
  switch (cmd) {
    case 1: // forward: both servos forward
      servoLeft.write(SERVO_FASTF);
      servoRight.write(SERVO_FASTF);
      break;
    case 2: // backward: both servos backward
      servoLeft.write(SERVO_FASTB);
      servoRight.write(SERVO_FASTB);
      break;
    case 3: // turn left: left backward, right forward
      servoLeft.write(SERVO_FASTB);
      servoRight.write(SERVO_FASTF);
      break;
    case 4: // turn right: left forward, right backward
      servoLeft.write(SERVO_FASTF);
      servoRight.write(SERVO_FASTB);
      break;
    default: // stop
      servoLeft.write(SERVO_STOP);
      servoRight.write(SERVO_STOP);
      break;
  }
}

// HTML page to serve
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Omni Directional Robot Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" type="text/css" href="https://raw.flemingsociety.com/robot/style.css">
</head>
<body>
  <div class="logo-container">
    <img src="https://raw.flemingsociety.com/logo.png" alt="Fleming Society Logo">
  </div>
  <div class="container">
    <h1>Omni Directional Robot Control</h1>
    <div class="row">
      <button id="btnUp"    onmousedown="buttonPress('1')" onmouseup="buttonRelease()" ontouchstart="buttonPress('1')" ontouchend="buttonRelease()">&#8679;</button>
    </div>
    <div class="row">
      <button id="btnLeft"  onmousedown="buttonPress('3')" onmouseup="buttonRelease()" ontouchstart="buttonPress('3')" ontouchend="buttonRelease()">&#8678;</button>
      <div class="spacer"></div>
      <button id="btnRight" onmousedown="buttonPress('4')" onmouseup="buttonRelease()" ontouchstart="buttonPress('4')" ontouchend="buttonRelease()">&#8680;</button>
    </div>
    <div class="row">
      <button id="btnDown"  onmousedown="buttonPress('2')" onmouseup="buttonRelease()" ontouchstart="buttonPress('2')" ontouchend="buttonRelease()">&#8681;</button>
    </div>
  </div>
  <script src="https://raw.flemingsociety.com/robot/script.js"></script>
</body>
</html>
)rawliteral";

// Serve the control page
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nConnected! IP=%s\n", WiFi.localIP().toString().c_str());

  // Start HTTP server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started.");

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started.");

  // Attach servos to pins
  servoLeft.attach(SERVO_LEFT_PIN);
  servoRight.attach(SERVO_RIGHT_PIN);

  // Ensure servos are stopped at startup
  sendServoCommand(0);
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
