#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>         // ESP32 Servo library
#include <Preferences.h>        // For storing calibration offsets

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

// Preferences for calibration storage
typedef Preferences Prefs;
Prefs prefs;

// Calibration offsets (in degrees)
int leftOffset  = 0;
int rightOffset = 0;

// Servo control pins (signal pins)
const int SERVO_LEFT_PIN  = D0;
const int SERVO_RIGHT_PIN = D1;

// SR04 ultrasonic sensor pins
const int SR04_TRIG_PIN = D3;
const int SR04_ECHO_PIN = D2;

// Base pulse widths for continuous rotation servos
const int SERVO_CENTER = 90;   // nominal stop position
const int SERVO_FASTF  = 135;  // faster forward
const int SERVO_FASTB  = 45;   // faster backward

// Compute effective positions using offsets
inline int posCenter(int offset) { return SERVO_CENTER + offset; }
inline int posFastF (int offset) { return SERVO_FASTF  + offset; }
inline int posFastB (int offset) { return SERVO_FASTB  + offset; }

// Read distance from SR04 (in cm)
long readUltrasonicCM() {
  digitalWrite(SR04_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(SR04_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SR04_TRIG_PIN, LOW);
  long duration = pulseIn(SR04_ECHO_PIN, HIGH, 30000); // timeout 30ms
  long distance = duration * 0.034 / 2;
  return distance;
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
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
      servoLeft.write(posFastF(leftOffset));
      servoRight.write(posFastF(rightOffset));
      break;
    case 2: // backward: both servos backward
      servoLeft.write(posFastB(leftOffset));
      servoRight.write(posFastB(rightOffset));
      break;
    case 3: // turn left: left backward, right forward
      servoLeft.write(posFastB(leftOffset));
      servoRight.write(posFastF(rightOffset));
      break;
    case 4: // turn right: left forward, right backward
      servoLeft.write(posFastF(leftOffset));
      servoRight.write(posFastB(rightOffset));
      break;
    default: // stop
      servoLeft.write(posCenter(leftOffset));
      servoRight.write(posCenter(rightOffset));
      break;
  }
}

// HTML page to serve (unchanged)
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

  // Load calibration offsets
  prefs.begin("servoCalib", false);
  leftOffset  = prefs.getInt("leftOffset", 0);
  rightOffset = prefs.getInt("rightOffset", 0);
  Serial.printf("Loaded calibration: leftOffset=%d, rightOffset=%d\n", leftOffset, rightOffset);

  // Provide calibration instructions over Serial
  Serial.println("Calibration commands:");
  Serial.println("  L/l : increase/decrease left offset");
  Serial.println("  R/r : increase/decrease right offset");
  Serial.println("  S   : save offsets to flash");

  // Initialize SR04 pins
  pinMode(SR04_TRIG_PIN, OUTPUT);
  pinMode(SR04_ECHO_PIN, INPUT);

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
  // Handle network
  server.handleClient();
  webSocket.loop();

  // Serial-based calibration input
  if (Serial.available()) {
    char c = Serial.read();
    bool changed = false;
    switch (c) {
      case 'L': leftOffset++; changed = true; break;
      case 'l': leftOffset--; changed = true; break;
      case 'R': rightOffset++; changed = true; break;
      case 'r': rightOffset--; changed = true; break;
      case 'S':
        prefs.putInt("leftOffset", leftOffset);
        prefs.putInt("rightOffset", rightOffset);
        Serial.println("Calibration saved.");
        break;
    }
    if (changed) {
      Serial.printf("Offsets updated: left=%d, right=%d\n", leftOffset, rightOffset);
      // Apply new stop immediately
      sendServoCommand(0);
    }
  }

  // Periodically read and print SR04 distance
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 500) {
    lastRead = millis();
    long dist = readUltrasonicCM();
    if(dist > 0) Serial.printf("Distance: %ld cm\n", dist);
    else       Serial.println("Distance: out of range");
  }
}
