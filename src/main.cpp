#include <Arduino.h>
#include <avr/wdt.h>
#include <string.h>

// Laser Monitor firmware workflow:
// 1. Start with both physical outputs low.
// 2. Read newline-delimited USB serial commands from the dashboard.
// 3. Reply to PING for connection checks.
// 4. Apply valid STATE commands atomically to the two output pins.
// 5. Drop beams low if valid dashboard messages stop arriving.
// 6. Reset the hardware watchdog during normal loop execution.
// 7. Leave outputs unchanged on malformed commands.

// Capture reset cause and stop any inherited watchdog before normal startup runs.
// This follows the standard avr-libc early-startup watchdog pattern.
uint8_t resetCauseMirror __attribute__((section(".noinit")));
void watchdog_early_init(void) __attribute__((naked)) __attribute__((section(".init3"))) __attribute__((used));

void watchdog_early_init(void) {
  resetCauseMirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

constexpr unsigned long SERIAL_BAUD = 9600;
constexpr uint8_t RADIATION_PIN = 2;
constexpr uint8_t BEAMS_PIN = 8;
constexpr size_t LINE_BUFFER_SIZE = 64;
constexpr unsigned long DASHBOARD_TIMEOUT_MS = 4000;

char lineBuffer[LINE_BUFFER_SIZE];
size_t lineLength = 0;
bool lineOverflowed = false;

bool beamsOn = false;
bool radiationIndicator = false;
unsigned long lastDashboardMessageMs = 0;

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

  // Reboot the controller if firmware execution stalls for about 8 seconds.
  wdt_enable(WDTO_8S);
}

void loop() {
  wdt_reset();
  readSerialCommands();

  // If dashboard protocol messages stop, force only the beams output low.
  if (beamsOn && millis() - lastDashboardMessageMs >= DASHBOARD_TIMEOUT_MS) {
    beamsOn = false;
    applyOutputs();
  }
}

void applyOutputs() {
  // HIGH is the asserted indicator state; LOW is inactive.
  digitalWrite(BEAMS_PIN, beamsOn ? HIGH : LOW);
  digitalWrite(RADIATION_PIN, radiationIndicator ? HIGH : LOW);
}

void readSerialCommands() {
  // Non-blocking line reader. Avoid Arduino String to keep SRAM usage stable.
  while (Serial.available() > 0) {
    wdt_reset();
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
    lastDashboardMessageMs = millis();
    Serial.println("PONG");
    return;
  }

  bool newBeamsOn = false;
  bool newRadiationIndicator = false;
  if (parseStateCommand(command, newBeamsOn, newRadiationIndicator)) {
    lastDashboardMessageMs = millis();
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
