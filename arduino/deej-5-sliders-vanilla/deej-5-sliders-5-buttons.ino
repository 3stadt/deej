const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A4};

const int NUM_BUTTONS = 5;
const int digitalInputs[NUM_BUTTONS] = {2, 3, 4, 5, 6};

int analogSliderValues[NUM_SLIDERS];
bool buttonPressedValues[NUM_BUTTONS];
int lastButtonStates[NUM_BUTTONS];

// LED state per digital pin. Adjust MAX_DIGITAL_PINS if using a Mega (up to 54).
// State values: 0 = off, 1 = on, 2 = blink.
// MAX_DIGITAL_PINS is used to create the initial array that holds the LED states
const int MAX_DIGITAL_PINS = 14;
int ledStates[MAX_DIGITAL_PINS];

const int BLINK_INTERVAL_MS = 500;
bool ledBlinkPhase = false;
unsigned long lastBlinkToggle = 0;

// Buffer for assembling incoming serial commands from the Go program
String incomingCommand = "";

void setup() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(digitalInputs[i], INPUT);
    lastButtonStates[i] = LOW;
    buttonPressedValues[i] = false;
  }
  for (int i = 0; i < MAX_DIGITAL_PINS; i++) {
    ledStates[i] = 0;
  }

  Serial.begin(9600);
}

void loop() {
  handleIncomingCommands();
  updateSliderValues();
  updateButtonValues();
  sendSliderValues(); // Actually send data (all the time)
  // printSliderValues(); // For debug
  updateLEDs();
  delay(10);
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    analogSliderValues[i] = analogRead(analogInputs[i]);
  }
}

void updateButtonValues() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int state = digitalRead(digitalInputs[i]);
    if (state == HIGH && lastButtonStates[i] == LOW) {
      buttonPressedValues[i] = true;
    }
    lastButtonStates[i] = state;
  }
}

void sendSliderValues() {
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);
    builtString += String("|");
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    builtString += String(buttonPressedValues[i] ? 1 : 0);
    if (i < NUM_BUTTONS - 1) {
      builtString += String("|");
    }
    buttonPressedValues[i] = false;
  }

  Serial.println(builtString);
}

// Read any bytes the Go program sent and accumulate them into incomingCommand.
// A newline marks the end of a command, which is then dispatched to processCommand().
void handleIncomingCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      processCommand(incomingCommand);
      incomingCommand = "";
    } else if (c != '\r') {
      incomingCommand += c;
    }
  }
}

// Process a single command string (without the trailing newline).
// Supported commands:
//   L<pin>,<state>  — set LED on <pin> to state 0 (off), 1 (on), or 2 (blink)
void processCommand(String cmd) {
  if (cmd.length() < 3 || cmd.charAt(0) != 'L') {
    return;
  }

  int commaIdx = cmd.indexOf(',');
  if (commaIdx < 2) {
    return;
  }

  int pin = cmd.substring(1, commaIdx).toInt();
  int state = cmd.substring(commaIdx + 1).toInt();

  if (pin < 0 || pin >= MAX_DIGITAL_PINS) {
    return;
  }

  ledStates[pin] = state;
  pinMode(pin, OUTPUT);

  // Apply immediately for non-blink states; blink is handled by updateLEDs()
  if (state == 0) {
    digitalWrite(pin, LOW);
  } else if (state == 1) {
    digitalWrite(pin, HIGH);
  }
}

// Drive blink-state LEDs. Called every loop iteration; the blink phase toggles
// every BLINK_INTERVAL_MS milliseconds regardless of the loop delay.
void updateLEDs() {
  unsigned long now = millis();
  if (now - lastBlinkToggle >= BLINK_INTERVAL_MS) {
    lastBlinkToggle = now;
    ledBlinkPhase = !ledBlinkPhase;
  }

  for (int pin = 0; pin < MAX_DIGITAL_PINS; pin++) {
    if (ledStates[pin] == 2) {
      digitalWrite(pin, ledBlinkPhase ? HIGH : LOW);
    }
  }
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}
