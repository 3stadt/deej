const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A4};

const int NUM_BUTTONS = 5;
const int digitalInputs[NUM_BUTTONS] = {2, 3, 4, 5, 6};

int analogSliderValues[NUM_SLIDERS];
bool buttonPressedValues[NUM_BUTTONS];
int lastButtonStates[NUM_BUTTONS];

void setup() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(digitalInputs[i], INPUT);
    lastButtonStates[i] = LOW;
    buttonPressedValues[i] = false;
  }

  Serial.begin(9600);
}

void loop() {
  updateSliderValues();
  updateButtonValues();
  sendSliderValues(); // Actually send data (all the time)
  // printSliderValues(); // For debug
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