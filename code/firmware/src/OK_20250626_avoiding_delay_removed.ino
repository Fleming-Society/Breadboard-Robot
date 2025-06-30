// Ultrasonic obstacle avoidance with LED feedback (ready for motor control swap)
// State machine: NORMAL (forward) and EVADING (left/right)
// Designed for ESP32-C3 with non-blocking logic and online expansion in mind

const int trigPin = D0;
const int echoPin = D1;
const int greenLED = D4;   // Simulates turning RIGHT
const int yellowLED = D5;  // Simulates forward motion
const int redLED = D6;     // Simulates turning LEFT

#define SOUND_SPEED 0.034  // cm per microsecond
#define ECHO_TIMEOUT 30000 // Timeout in microseconds (30 ms = ~5 meters)

enum RobotState { NORMAL, EVADING };
RobotState currentState = NORMAL;

float distanceCm = 0;
unsigned long lastSensorTime = 0;
unsigned long sensorInterval = 200;  // Sensor refresh interval (ms)

unsigned long lastBlinkTime = 0;
bool ledState = false;
int turnDirection = 0;  // 0 = LEFT (red), 1 = RIGHT (green)

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  randomSeed(micros()); // Seed for random left/right decisions
}

void loop() {
  unsigned long now = millis();

  // --- SENSOR SAMPLING ---
  if (now - lastSensorTime >= sensorInterval) {
    lastSensorTime = now;
    distanceCm = readDistance();

    Serial.print("Distance (cm): ");
    Serial.println(distanceCm);

    // Trigger EVADING if obstacle is too close
    if (currentState == NORMAL && distanceCm > 0 && distanceCm < 5.0) {
      currentState = EVADING;
      turnDirection = random(0, 2); // 0 = LEFT, 1 = RIGHT
      Serial.println(turnDirection == 0 ? "← Turning LEFT" : "→ Turning RIGHT");
      lastBlinkTime = now;
      ledState = false;
    }

    // Exit EVADING if obstacle cleared
    if (currentState == EVADING && distanceCm > 10.0) {
      currentState = NORMAL;
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, LOW);
      digitalWrite(yellowLED, LOW);
      Serial.println("↑ Obstacle cleared. Resuming forward.");
    }
  }

  // --- LED / MOTOR FEEDBACK ---
  if (now - lastBlinkTime >= 250) {
    lastBlinkTime = now;
    ledState = !ledState;

    if (currentState == EVADING) {
      if (turnDirection == 0) {
        turnLeft();
      } else {
        turnRight();
      }
    } else {
      moveForward();
    }
  }
}

// --- CLEAN DISTANCE MEASUREMENT FUNCTION ---
float readDistance() {
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);              // Sensor requires ≥10 μs trigger pulse
  digitalWrite(trigPin, LOW);

  long duration = pulseInLong(echoPin, HIGH, ECHO_TIMEOUT);
  if (duration == 0) return -1.0;     // Timeout — no echo received
  return duration * SOUND_SPEED / 2;  // Convert to cm
}

// --- TURN LEFT ---
void turnLeft() {
  // TODO: Replace with motor control logic (e.g., motorA reverse, motorB forward)
  digitalWrite(redLED, ledState);
  digitalWrite(greenLED, LOW);
  digitalWrite(yellowLED, LOW);
}

// --- TURN RIGHT ---
void turnRight() {
  // TODO: Replace with motor control logic (e.g., motorA forward, motorB reverse)
  digitalWrite(greenLED, ledState);
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, LOW);
}

// --- MOVE FORWARD ---
void moveForward() {
  // TODO: Replace with motor control logic (e.g., both motors forward)
  digitalWrite(greenLED, LOW);
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, ledState);
}
