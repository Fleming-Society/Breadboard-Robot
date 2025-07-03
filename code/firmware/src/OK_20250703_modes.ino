#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// ==== WiFi Credentials ====
const char* ssid     = "earthworm";
const char* password = "12345678";

// ==== Pin Definitions ====
const int SERVO_LEFT_PIN   = D0;
const int SERVO_RIGHT_PIN  = D1;
const int TRIG_PIN         = D2;
const int ECHO_PIN         = D3;
const int BUTTON_PIN       = D4;  // Active-HIGH pushbutton

// ==== Servo Calibration Preferences ====
Preferences prefs;
int leftOffset  = 0;
int rightOffset = 0;

// ==== Servo Pulse Definitions ====
const int SERVO_CENTER = 90;
const int SERVO_FASTF  = 135;
const int SERVO_FASTB  = 45;

inline int posCenter(int offset) { return SERVO_CENTER + offset; }
inline int posFastF (int offset) { return SERVO_FASTF  + offset; }
inline int posFastB (int offset) { return SERVO_FASTB  + offset; }

// ==== Modes ====
enum Mode { MANUAL = 0, DISTANCE = 1, LINE_FOLLOW = 2 };
Mode currentMode = MANUAL;
const char* modeNames[] = { "MANUAL", "DISTANCE", "LINE FOLLOW" };

// ==== Timing & Debounce ====
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 100;
unsigned long lastSensorTime = 0;
const unsigned long sensorInterval = 500;
bool prevrandom = LOW;
#define SOUND_SPEED 0.034
#define ECHO_TIMEOUT 30000

int turnDirection = 0;

// ==== Web Interfaces ====
WebServer server(80);
WebSocketsServer webSocket(81);

// ==== Servo Objects ====
Servo servoLeft;
Servo servoRight;

// ==== HTML Page ====
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

// ==== Function Prototypes ====
void stopMotors();
void sendServoCommand(uint8_t cmd);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
float readDistance();
void checkButton();
void handleDistanceMode();

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  // Load calibration offsets
  prefs.begin("servoCalib", false);
  leftOffset  = prefs.getInt("leftOffset", 0);
  rightOffset = prefs.getInt("rightOffset", 0);
  Serial.printf("Loaded calibration: leftOffset=%d, rightOffset=%d\n", leftOffset, rightOffset);

  // Calibration instructions
  Serial.println("Calibration commands: L/l increase/decrease left offset, R/r for right, S to save");

  // Pin config
  pinMode(BUTTON_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach servos & init stop
  servoLeft.attach(SERVO_LEFT_PIN);
  servoRight.attach(SERVO_RIGHT_PIN);
  stopMotors();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print('.');
  }
  Serial.printf("\nConnected! IP=%s\n", WiFi.localIP().toString().c_str());

  // Start HTTP & WebSocket servers
  server.on("/", [](){ server.send(200, "text/html", htmlPage); });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ==== Main Loop ====
void loop() {
  server.handleClient();
  webSocket.loop();

  checkButton();

  // Handle modes
  switch (currentMode) {
    case DISTANCE:      handleDistanceMode(); break;
    case MANUAL:       /* via WebSocket */ break;
    case LINE_FOLLOW:  /* future implementation */ break;
  }

  // Serial calibration input
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
      sendServoCommand(0); // apply new center
    }
  }
}

// ==== Check Button for Mode Switching ====
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

// ==== Distance-based Obstacle Avoidance ====
void handleDistanceMode() {
  unsigned long now = millis();
  if (now - lastSensorTime < sensorInterval) return;
  lastSensorTime = now;
  float distance = readDistance();
  Serial.printf("Distance: %.2f cm\n", distance);
  //Serial.printf("Turn Direction %d", turnDirection);
  
  if (distance > 0 && distance < 30.0) {
    if (turnDirection == 0) {
      // turn left
            Serial.printf("Turning Left");
      //servoLeft.write(posFastB(leftOffset));
  servoRight.write(posCenter(rightOffset));
  servoLeft.write(posFastB(leftOffset));
      prevrandom=LOW;
    } else {
      // turn right
      Serial.printf("Turning Right");
            servoRight.write(posFastF(rightOffset));
        servoLeft.write(posCenter(leftOffset));
     
      //servoRight.write(posFastB(rightOffset));
        
      prevrandom=LOW;
    }
  } 
  
  else {
    if (prevrandom==LOW){turnDirection = random(0, 2); prevrandom=HIGH; Serial.printf("Turn Direction Set %d", turnDirection);}
    // forward
    servoLeft.write(posFastF(leftOffset));
    servoRight.write(posFastB(rightOffset));
    
  } 
}

// ==== Read Distance from SR04 ====
float readDistance() {
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseInLong(ECHO_PIN, HIGH, ECHO_TIMEOUT);
  if (duration == 0) return -1;
  return duration * SOUND_SPEED / 2;
}

// ==== WebSocket Event Handler ====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT && currentMode == MANUAL) {
    char buf[4] = {0};
    memcpy(buf, payload, min(length, sizeof(buf)-1));
    uint8_t cmd = atoi(buf);
    sendServoCommand(cmd);
  }
}

// ==== Send Servo Commands Based on User Input ====
void sendServoCommand(uint8_t cmd) {
  switch (cmd) {
    case 1: // forward
      servoLeft.write(posFastF(leftOffset));
      servoRight.write(posFastB(rightOffset));
      break;
    case 2: // backward
      servoLeft.write(posFastB(leftOffset));
      servoRight.write(posFastF(rightOffset));
      break;
    case 3: // turn left
      servoLeft.write(posFastB(leftOffset));
      servoRight.write(posFastB(rightOffset));
      break;
    case 4: // turn right
      servoLeft.write(posFastF(leftOffset));
      servoRight.write(posFastF(rightOffset));
      break;
    default:
      stopMotors();
      break;
  }
}

// ==== Stop Both Servos ====
void stopMotors() {
  servoLeft.write(posCenter(leftOffset));
  servoRight.write(posCenter(rightOffset));
}
