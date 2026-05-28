#include <Arduino.h>
#include <string.h>

// Laser Monitor firmware workflow:
// 1. Start with both physical outputs low.
// 2. Read newline-delimited USB serial commands from the dashboard.
// 3. Reply to PING for connection checks.
// 4. Apply valid STATE commands atomically to the two output pins.
// 5. Leave outputs unchanged on malformed commands or serial silence.

constexpr unsigned long SERIAL_BAUD = 9600;
constexpr uint8_t RADIATION_PIN = 2;
constexpr uint8_t BEAMS_PIN = 8;
constexpr size_t LINE_BUFFER_SIZE = 64;

char lineBuffer[LINE_BUFFER_SIZE];
size_t lineLength = 0;
bool lineOverflowed = false;

bool beamsOn = false;
bool radiationIndicator = false;

void applyOutputs();
void readSerialCommands();
void finishLine();
void handleCommand(const char *command);
bool parseStateCommand(const char *command, bool &newBeamsOn, bool &newRadiationIndicator);
bool parseBoolValue(const char *text, bool &value);

void setup() {
  // Outputs default low until the dashboard sends a valid STATE command.
  pinMode(RADIATION_PIN, OUTPUT);
  pinMode(BEAMS_PIN, OUTPUT);
  applyOutputs();

  Serial.begin(SERIAL_BAUD);
}

void loop() {
  readSerialCommands();
}

void applyOutputs() {
  // HIGH is the asserted indicator state; LOW is inactive.
  digitalWrite(BEAMS_PIN, beamsOn ? HIGH : LOW);
  digitalWrite(RADIATION_PIN, radiationIndicator ? HIGH : LOW);
}

void readSerialCommands() {
  // Non-blocking line reader. Avoid Arduino String to keep SRAM usage stable.
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\n') {
      finishLine();
      continue;
    }

    if (c == '\r') {
      continue;
    }

    if (lineLength < LINE_BUFFER_SIZE - 1) {
      lineBuffer[lineLength++] = c;
    } else {
      lineOverflowed = true;
    }
  }
}

void finishLine() {
  lineBuffer[lineLength] = '\0';

  if (lineOverflowed) {
    Serial.println("ERR");
  } else if (lineLength > 0) {
    handleCommand(lineBuffer);
  }

  lineLength = 0;
  lineOverflowed = false;
}

void handleCommand(const char *command) {
  // The dashboard treats any missing or unexpected response as a lost link.
  if (strcmp(command, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  bool newBeamsOn = false;
  bool newRadiationIndicator = false;
  if (parseStateCommand(command, newBeamsOn, newRadiationIndicator)) {
    beamsOn = newBeamsOn;
    radiationIndicator = newRadiationIndicator;
    applyOutputs();
    Serial.println("OK");
    return;
  }

  Serial.println("ERR");
}

bool parseStateCommand(const char *command, bool &newBeamsOn, bool &newRadiationIndicator) {
  // Validate the whole packet before changing any output state.
  constexpr char PREFIX[] = "STATE beams=";
  constexpr char MIDDLE[] = " radiation=";

  const size_t prefixLength = strlen(PREFIX);
  if (strncmp(command, PREFIX, prefixLength) != 0) {
    return false;
  }

  const char *beamsText = command + prefixLength;
  const char *middle = strstr(beamsText, MIDDLE);
  if (middle == nullptr) {
    return false;
  }

  if (middle != beamsText + 1) {
    return false;
  }

  bool parsedBeamsOn = false;
  if (!parseBoolValue(beamsText, parsedBeamsOn)) {
    return false;
  }

  const char *radiationText = middle + strlen(MIDDLE);
  if (strlen(radiationText) != 1) {
    return false;
  }

  bool parsedRadiationIndicator = false;
  if (!parseBoolValue(radiationText, parsedRadiationIndicator)) {
    return false;
  }

  newBeamsOn = parsedBeamsOn;
  newRadiationIndicator = parsedRadiationIndicator;
  return true;
}

bool parseBoolValue(const char *text, bool &value) {
  if (text[0] == '0') {
    value = false;
    return true;
  }

  if (text[0] == '1') {
    value = true;
    return true;
  }

  return false;
}
