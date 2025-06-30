#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>

// ==== Function Prototypes ====
void stopMotors();
void sendServoCommand(uint8_t cmd);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
float readDistance();
void checkButton();
void handleDistanceMode();

// ==== WiFi Credentials ====
const char* ssid     = "earthworm";
const char* password = "12345678";

// ==== Pin Definitions ====
const int SERVO_LEFT_PIN   = D0;
const int SERVO_RIGHT_PIN  = D1;
const int TRIG_PIN         = D2;
const int ECHO_PIN         = D3;
const int BUTTON_PIN       = D4;  // Active-HIGH pushbutton

// ==== Servo Setup ====
Servo servoLeft;
Servo servoRight;

const int SERVO_STOP  = 90;
const int SERVO_FASTF = 135;
const int SERVO_FASTB = 45;

// ==== Modes ====
enum Mode { MANUAL = 0, DISTANCE = 1, LINE_FOLLOW = 2 };
Mode currentMode = MANUAL;
const char* modeNames[] = {  "MANUAL", "DISTANCE", "LINE FOLLOW" };

// ==== Timing ====
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300;

unsigned long lastSensorTime = 0;
const unsigned long sensorInterval = 200;
#define SOUND_SPEED 0.034
#define ECHO_TIMEOUT 30000

int turnDirection = 0;
unsigned long lastTurnToggle = 0;

// ==== Web Interfaces ====
WebServer server(80);
WebSocketsServer webSocket(81);

// ==== HTML Page (same as original) ====
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

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  // Pin config
  pinMode(BUTTON_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach servos
  servoLeft.attach(SERVO_LEFT_PIN);
  servoRight.attach(SERVO_RIGHT_PIN);
  stopMotors();

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\nConnected! IP=%s\n", WiFi.localIP().toString().c_str());

  // Web services
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ==== Loop ====
void loop() {
  server.handleClient();
  webSocket.loop();

  checkButton();

  switch (currentMode) {
    case DISTANCE:
      handleDistanceMode();
      break;
    case MANUAL:
      // handled via WebSocket
      break;
    case LINE_FOLLOW:
      // future implementation
      break;
  }
}

// ==== Handle Pushbutton ====
void checkButton() {
  static bool lastState = LOW;
  bool currentState = digitalRead(BUTTON_PIN);

  if (currentState == HIGH && lastState == LOW && millis() - lastButtonPress > debounceDelay) {
    currentMode = static_cast<Mode>((currentMode + 1) % 3);
    Serial.printf("Mode changed to: %s\n", modeNames[currentMode]);
    stopMotors();
    lastButtonPress = millis();
  }

  lastState = currentState;
}

// ==== Obstacle Avoidance ====
void handleDistanceMode() {
  unsigned long now = millis();

  if (now - lastSensorTime >= sensorInterval) {
    lastSensorTime = now;
    float distance = readDistance();
    Serial.printf("Distance: %.2f cm\n", distance);

    if (distance > 0 && distance < 5.0) {
      if (turnDirection == 0) {
        servoLeft.write(SERVO_FASTB);
        servoRight.write(SERVO_FASTF);
        Serial.println("← Distance: Turning LEFT");
      } else {
        servoLeft.write(SERVO_FASTF);
        servoRight.write(SERVO_FASTB);
        Serial.println("→ Distance: Turning RIGHT");
      }
    } else if (distance > 10.0) {
      servoLeft.write(SERVO_FASTF);
      servoRight.write(SERVO_FASTF);
      Serial.println("↑ Distance: Moving FORWARD");
      turnDirection = random(0, 2);
    } else {
      stopMotors();
      Serial.println("Distance: No echo / too close");
    }
  }
}

// ==== WebSocket Events ====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT && currentMode == MANUAL) {
    char buf[4] = {0};
    memcpy(buf, payload, min(length, sizeof(buf) - 1));
    uint8_t cmd = atoi(buf);
    sendServoCommand(cmd);
  }
}

// ==== Manual Commands ====
void sendServoCommand(uint8_t cmd) {
  Serial.printf("Web cmd: %u\n", cmd);
  switch (cmd) {
    case 4: // forward
      servoLeft.write(SERVO_FASTF);
      servoRight.write(SERVO_FASTF);
      break;
    case 3: // backward
      servoLeft.write(SERVO_FASTB);
      servoRight.write(SERVO_FASTB);
      break;
    case 2: // turn left
      servoLeft.write(SERVO_FASTB);
      servoRight.write(SERVO_FASTF);
      break;
    case 1: // turn right
      servoLeft.write(SERVO_FASTF);
      servoRight.write(SERVO_FASTB);
      break;
    default:
      stopMotors();
      break;
  }
}

// ==== Stop Motors ====
void stopMotors() {
  servoLeft.write(SERVO_STOP);
  servoRight.write(SERVO_STOP);
  Serial.println("Motors stopped");
}

// ==== Read Distance ====
float readDistance() {
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseInLong(ECHO_PIN, HIGH, ECHO_TIMEOUT);
  if (duration == 0) return -1.0;
  return duration * SOUND_SPEED / 2;
}
