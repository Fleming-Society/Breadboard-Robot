#include <WiFi.h>
#include <WebServer.h>

// ——— CONFIG —————————————————————————————————
const char* AP_SSID     = "XiaoESP32S3_AP";
const char* AP_PASSWORD = "12345678";
const uint8_t SWITCH_PIN = 0;    // your switch pin (must be an interrupt-capable GPIO)
// —————————————————————————————————————————————

WebServer server(80);
volatile bool switchOn = false;       // current debounced switch state
volatile bool switchChanged = false;  // ISR sets this when switch flips
bool apMode = false;                  // tracks whether AP+server are running

// HTTP handler
void handleRoot() {
  server.send(200, "text/plain", "hello world");
}

// your autonomous routine
void autonomous() {
  // … your code here …
}

// ISR: runs in interrupt context, must be very fast
void IRAM_ATTR switchISR() {
  // read the pin; this is safe in an ISR
  bool state = digitalRead(SWITCH_PIN);
  // latch into our globals
  switchOn = state;
  switchChanged = true;
}

// start AP + HTTP server
void startAP() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  apMode = true;
  Serial.printf("→ AP \"%s\" started, IP %s\n",
                AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

// stop AP + HTTP server
void stopAP() {
  server.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
  Serial.println("→ AP and server stopped");
}

void setup() {
  Serial.begin(115200);

  // configure switch pin
  pinMode(SWITCH_PIN, INPUT_PULLDOWN);
  // read initial state
  switchOn = digitalRead(SWITCH_PIN);

  // attach interrupt on any change
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, CHANGE);

  // act on initial position
  if (switchOn) {
    startAP();
  } else {
    Serial.println("→ Switch off at startup: autonomous()");
    autonomous();
  }
}

void loop() {
  // if the ISR has flagged a change, handle it now
  if (switchChanged) {
    // clear the flag
    switchChanged = false;

    // flip modes as needed
    if (switchOn && !apMode) {
      startAP();
    } else if (!switchOn && apMode) {
      stopAP();
    }
  }

  // then do the work for whichever mode we're in
  if (apMode) {
    server.handleClient();
  } else {
    autonomous();
  }

  // you can still yield to WiFi, or tweak timing here
  yield();
}
