#include <ESP32Servo.h>

// pins for IR sensors
#define LEFT_S 2
#define RIGHT_S 3
#define MID_S 4
// IR sensors go high when pointed at the line, else is low. Tune using the pot on the sensor

// pins for servos
#define LEFT_SERVO_PIN 0
#define RIGHT_SERVO_PIN 1

// calibration for neutral point for motor to stop
int neutralPulse1 = 1500;
int neutralPulse2 = 1500;

Servo leftServo, rightServo;

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

  leftServo.writeMicroseconds(pulse1);
  rightServo.writeMicroseconds(pulse2);
}

  
void setup() {
  Serial.begin(115200);
  // Instructions for calibration
  Serial.println("Servo calibration mode:\n  w/s: adjust both neutrals\n  a/z: front (leftServo)\n  d/c: back (rightServo)\n  After tuning, power cycle or press any control button.");
  Serial.printf("Neutral1=%d, Neutral2=%d\n", neutralPulse1, neutralPulse2);

  // Setup servos
  leftServo.setPeriodHertz(50);
  rightServo.setPeriodHertz(50);
  leftServo.attach(LEFT_SERVO_PIN, 500, 2500);
  rightServo.attach(RIGHT_SERVO_PIN, 500, 2500);

  // Initial stop
  sendServoCommand(0);

  // For the IR sensors
  pinMode(LEFT_S, INPUT);   
  pinMode(RIGHT_S, INPUT); 
}

void loop() {
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

  int L = digitalRead(LEFT_S);
  int R = digitalRead(RIGHT_S);

  // turn left
  if(!L && R){
    sendServoCommand(3);
  }
  // turn right
  else if (L && !R){
    sendServoCommand(4);
  }
  // go forward
  else if(!L && !R){
    sendServoCommand(1);
  }
  // stop
  else{
    sendServoCommand(0);
  }  
}
