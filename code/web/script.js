// script.js

const connection = new WebSocket("ws://" + window.location.hostname + ":81/");

connection.onopen  = () => console.log("WebSocket open");
connection.onclose = () => console.log("WebSocket closed");
connection.onerror = e => console.error("WebSocket error:", e);

function sendCommand(cmd) {
  if (connection.readyState === WebSocket.OPEN) {
    connection.send(cmd);
    console.log("Sent command:", cmd);
  }
}

// Map arrow-key codes to your command names
const keyMap = {
  ArrowUp:    "forward",
  ArrowDown:  "backward",
  ArrowLeft:  "turnLeft",
  ArrowRight: "turnRight"
};

// Track both keyboard and touch/button state
const activeKeys = { forward:false, backward:false, turnLeft:false, turnRight:false };
const buttonState = { forward:false, backward:false, turnLeft:false, turnRight:false };

function updateCommand() {
  if (activeKeys.forward   || buttonState.forward)   sendCommand("forward");
  else if (activeKeys.backward|| buttonState.backward)sendCommand("backward");
  else if (activeKeys.turnLeft || buttonState.turnLeft) sendCommand("turnLeft");
  else if (activeKeys.turnRight|| buttonState.turnRight)sendCommand("turnRight");
  else                                                sendCommand("stop");
}

// â€”â€” Keyboard listeners â€”â€”
document.addEventListener("keydown", e => {
  if (e.repeat) return;
  const cmd = keyMap[e.key];
  if (cmd) {
    activeKeys[cmd] = true;
    updateCommand();
  }
});

document.addEventListener("keyup", e => {
  const cmd = keyMap[e.key];
  if (cmd) {
    activeKeys[cmd] = false;
    updateCommand();
  }
});

// â€”â€” On-screen button handlers â€”â€”
function buttonPress(cmd) {
  buttonState[cmd] = true;
  updateCommand();
}

function buttonRelease() {
  // clear all four
  for (let k in buttonState) buttonState[k] = false;
  updateCommand();
}
