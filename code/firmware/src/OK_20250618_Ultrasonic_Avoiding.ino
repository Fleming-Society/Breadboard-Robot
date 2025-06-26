// Ultrasonic obstacle avoidance with LED feedback
// State machine: NORMAL (moving forward) and EVADING (turning left or right)
// LED lines will be removed later when replaced with motor driver commands.

const int trigPin = D0;
const int echoPin = D1;
const int greenLED = D4;   // Simulates turning RIGHT
const int yellowLED = D5;  // Blinks while moving FORWARD
const int redLED = D6;     // Simulates turning LEFT

#define SOUND_SPEED 0.034  // cm per microsecond

enum RobotState { NORMAL, EVADING };
RobotState currentState = NORMAL;

float distanceCm = 0;
unsigned long lastSensorTime = 0;
unsigned long sensorInterval = 200;  // Time between ultrasonic samples (ms)

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

  // Use internal timer as a random seed
  // Can later be replaced with a rule-based turning logic (e.g. alternating turns)
  randomSeed(micros());
}

void loop() {
  unsigned long now = millis();

  // --- SENSOR SAMPLING ---
  if (now - lastSensorTime >= sensorInterval) {
    lastSensorTime = now;

    // Trigger ultrasonic pulse
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    delayMicroseconds(100); // Allow echo pin to settle (ESP32-specific)

    // Read echo time and calculate distance
    long duration = pulseIn(echoPin, HIGH);
    distanceCm = duration * SOUND_SPEED / 2;

    Serial.print("Distance (cm): ");
    Serial.println(distanceCm);

    // If object detected within 5 cm, start evasive manoeuvre
    if (currentState == NORMAL && distanceCm < 5.0) {
      currentState = EVADING;
      turnDirection = random(0, 2);  // Randomly choose LEFT (0) or RIGHT (1)
      Serial.println(turnDirection == 0 ? "← Turning LEFT" : "→ Turning RIGHT");
      lastBlinkTime = now;
      ledState = false;
    }

    // If obstacle cleared (beyond 10 cm), resume forward motion
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

// --- TURN LEFT ---
void turnLeft() {
  // TODO: Replace with motor control logic (e.g. motorA reverse, motorB forward)
  digitalWrite(redLED, ledState);     // Simulate left turn
  digitalWrite(greenLED, LOW);
  digitalWrite(yellowLED, LOW);
}

// --- TURN RIGHT ---
void turnRight() {
  // TODO: Replace with motor control logic (e.g. motorA forward, motorB reverse)
  digitalWrite(greenLED, ledState);   // Simulate right turn
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, LOW);
}

// --- MOVE FORWARD ---
void moveForward() {
  // TODO: Replace with motor control logic (e.g. both motors forward)
  digitalWrite(greenLED, LOW);
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, ledState);  // Simulate forward motion
}
