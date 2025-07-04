// main.ino

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// === Wi-Fi ===
const char* SSID     = "YungHub";
const char* PASSWORD = "yungyung";

// === Pins ===
constexpr int PIN_SERVO_LEFT   = D0;
constexpr int PIN_SERVO_RIGHT  = D1;
constexpr int PIN_TRIG         = D2;
constexpr int PIN_ECHO         = D3;
constexpr int PIN_BUTTON       = D4;  // Active-HIGH
constexpr int LEFT_S           = D5;
constexpr int MIDDLE_S         = D6;
constexpr int RIGHT_S          = D7;


// === Calibration ===
Preferences prefs;
int leftOffset  = 0;
int rightOffset = 0;

// === Servo angles ===
constexpr int ANGLE_CENTER = 90;
constexpr int ANGLE_FASTF  = 135;
constexpr int ANGLE_FASTB  = 45;
inline int centerAngle(int o){ return ANGLE_CENTER + o; }
inline int forwardAngle(int o){ return ANGLE_FASTF  + o; }
inline int backwardAngle(int o){ return ANGLE_FASTB + o; }

// === Modes ===
enum Mode { MANUAL, DISTANCE, LINE_FOLLOW };
Mode currentMode = MANUAL;
const char* modeNames[] = { "MANUAL", "DISTANCE", "LINE FOLLOW" };

// === Timing & debounce ===
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 100;
unsigned long lastSensorTime  = 0;
const unsigned long SENSOR_INTERVAL_MS = 500;

// === Ultrasonic ===
#define SOUND_SPEED_CM_PER_US 0.034f
#define ECHO_TIMEOUT_US       30000UL

// === Servers ===
WebServer        httpServer(80);
WebSocketsServer wsServer(81);

// === Servos ===
Servo servoLeft, servoRight;

// === HTML ===
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
    <img src="https://raw.flemingsociety.com/logo.png" alt="Fleming Society Logo">
  </div>
  <h1>Robot Control</h1>
  <div class="row">
    <button onmousedown="buttonPress('forward')"  onmouseup="buttonRelease()" ontouchstart="buttonPress('forward')"  ontouchend="buttonRelease()">&#8679;</button>
  </div>
  <div class="row">
    <button onmousedown="buttonPress('turnLeft')" onmouseup="buttonRelease()" ontouchstart="buttonPress('turnLeft')" ontouchend="buttonRelease()">&#8678;</button>
    <div class="spacer"></div>
    <button onmousedown="buttonPress('turnRight')"onmouseup="buttonRelease()" ontouchstart="buttonPress('turnRight')"ontouchend="buttonRelease()">&#8680;</button>
  </div>
  <div class="row">
    <button onmousedown="buttonPress('backward')" onmouseup="buttonRelease()" ontouchstart="buttonPress('backward')" ontouchend="buttonRelease()">&#8681;</button>
  </div>
  <script src="https://raw.flemingsociety.com/robot/script.js"></script>
</body>
</html>
)rawliteral";

// === Prototypes ===
void stopMotors();
void spinMotor(const String& cmd);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
float readDistanceCM();
void checkModeButton();
void handleDistanceMode();
void lineFollowing();

void setup() {
  Serial.begin(115200);

  // calibration
  prefs.begin("servoCalib", false);
  leftOffset  = prefs.getInt("leftOffset", 0);
  rightOffset = prefs.getInt("rightOffset",0);
  Serial.printf("Calibration: L=%d, R=%d\n", leftOffset, rightOffset);
  Serial.println("L/l adjust left, R/r adjust right, S to save");

  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  servoLeft.attach(PIN_SERVO_LEFT);
  servoRight.attach(PIN_SERVO_RIGHT);
  stopMotors();

  // Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());

  // HTTP & WS
  httpServer.on("/", [](){ httpServer.send(200,"text/html",htmlPage); });
  httpServer.begin();
  wsServer.begin();
  wsServer.onEvent(webSocketEvent);
}

void loop() {
  httpServer.handleClient();
  wsServer.loop();
  checkModeButton();
  if (currentMode == DISTANCE) handleDistanceMode();
  if (currentMode == LINE_FOLLOW) lineFollowing();

  // serial calibration
  if (Serial.available()) {
    char c = Serial.read();
    bool changed=false;
    switch(c) {
      case 'L': leftOffset++;  changed=true; break;
      case 'l': leftOffset--;  changed=true; break;
      case 'R': rightOffset++; changed=true; break;
      case 'r': rightOffset--; changed=true; break;
      case 'S':
        prefs.putInt("leftOffset", leftOffset);
        prefs.putInt("rightOffset", rightOffset);
        Serial.println("Saved.");
        break;
    }
    if (changed) {
      Serial.printf("Offsets: L=%d R=%d\n", leftOffset, rightOffset);
      stopMotors();
    }
  }
}

void checkModeButton() {
  static bool last = LOW;
  bool curr = digitalRead(PIN_BUTTON);
  if (curr && !last && millis()-lastButtonPress>DEBOUNCE_MS) {
    currentMode = Mode((currentMode+1)%3);
    Serial.printf("Mode → %s\n", modeNames[currentMode]);
    stopMotors();
    lastButtonPress = millis();
  }
  last = curr;
}

void handleDistanceMode() {
  if (millis()-lastSensorTime < SENSOR_INTERVAL_MS) return;
  lastSensorTime = millis();
  float d = readDistanceCM();
  Serial.printf("Dist=%.1fcm\n", d);

  static bool turnLeftNext = true;
  if (d>0 && d<30.0f) {
    if (turnLeftNext) { Serial.println("Avoid: turnLeft");  spinMotor("turnLeft"); }
    else             { Serial.println("Avoid: turnRight"); spinMotor("turnRight"); }
    turnLeftNext = !turnLeftNext;
  } else {
    Serial.println("Path clear: forward");
    spinMotor("forward");
  }
}

float readDistanceCM() {
  digitalWrite(PIN_TRIG,HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG,LOW);
  long dur = pulseInLong(PIN_ECHO,HIGH,ECHO_TIMEOUT_US);
  return dur==0 ? -1 : dur * SOUND_SPEED_CM_PER_US / 2.0f;
}

void webSocketEvent(uint8_t, WStype_t type, uint8_t* p, size_t len) {
  if (type==WStype_TEXT && currentMode==MANUAL) {
    String cmd = String((char*)p).substring(0,len);
    spinMotor(cmd);
  }
}

void spinMotor(const String& cmd) {
  if      (cmd=="forward")   { servoLeft.write(forwardAngle(leftOffset));  servoRight.write(backwardAngle(rightOffset)); }
  else if (cmd=="backward")  { servoLeft.write(backwardAngle(leftOffset)); servoRight.write(forwardAngle(rightOffset));  }
  else if (cmd=="turnLeft")  { servoLeft.write(backwardAngle(leftOffset)); servoRight.write(backwardAngle(rightOffset)); }
  else if (cmd=="turnRight") { servoLeft.write(forwardAngle(leftOffset));  servoRight.write(forwardAngle(rightOffset));  }
  else                        stopMotors();
}

void stopMotors() {
  servoLeft.write(centerAngle(leftOffset));
  servoRight.write(centerAngle(rightOffset));
}

void lineFollowing(){
  int left = digitalRead(LEFT_S);
  int middle = digitalRead(MIDDLE_S);
  int right = digitalRead(RIGHT_S);

  // used for backtracking
  static bool lost = false;
  // if any of the sensors find the line, it is not lost
  if(left || middle || right) lost = false;

  if(!left && middle && !right){
    // forward
    spinMotor("forward");
  }
  else{
    if(left && middle && right){
      // stop
      stopMotors();
    }
    else if(!left && !middle && !right){
      // if its the first time all sensors lose, go backwards
      if(!lost){
        spinMotor("backward");
        lost = true;
      }
      else{
        // stop on the second time
        stopMotors();
      }
    }
    else if(left){
      // turn left
      servoLeft.write(75);
      servoRight.write(75);
    }
    else if(right){
      servoLeft.write(105);
      servoRight.write(105);
    }
  }
  

  Serial.print("left:");
  Serial.print(left);
  
  Serial.print("  middle");
  Serial.print(middle);

  
  Serial.print("  right:");
  Serial.println(right);
  delay(150);
}
