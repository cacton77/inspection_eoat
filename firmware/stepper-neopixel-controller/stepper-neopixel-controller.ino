/*
 * Stepper + NeoPixel Controller for Adafruit QTPy RP2040
 * 
 * Controls a TMC2209 stepper driver via UART with StallGuard stall detection,
 * and a 24-LED RGBW NeoPixel ring (white channel only).
 *
 * Core 0: Serial packet reception, parsing, and homing execution
 * Core 1: Stepper motion control, NeoPixel updates, and serial feedback
 *
 * Startup behavior:
 *   1. Initializes all hardware
 *   2. Enters command loop immediately (no homing on boot)
 *   3. Homing runs when a command packet with the homing flag is received
 *   4. Sends text status during homing, then "HOMING_COMPLETE:<totalSteps>\n"
 *   5. Motor/LED control only active after homing completes
 *
 * Hardware connections:
 *   TMC2209 UART  -> Serial2 TX (GP20) / RX (GP5)  [UART1 on RP2040]
 *   STEP          -> MI   (GP4)
 *   DIR           -> SCK  (GP6)
 *   EN            -> MO   (GP3)
 *   DIAG          -> SCL  (GP25)
 *   NeoPixel Data -> A0   (GP29)
 *   Onboard Neo   -> GP12 (power on GP11)
 *
 * ─── Command Packet (29 bytes) ──────────────────────────────────────────
 *   Byte 0:       0xFF (start marker)
 *   Byte 1:       Stepper target position (0-255, mapped to min..max range)
 *   Byte 2:       Velocity command (signed int8):
 *                   0       = position control mode
 *                   +1..127 = velocity mode forward
 *                   -1..-128= velocity mode reverse
 *   Byte 3:       Flags:
 *                   bit 0   = request homing (1=home, 0=normal)
 *   Bytes 4-27:   White channel values for 24 LEDs (0-255 each)
 *   Byte 28:      Checksum (XOR of bytes 1-27)
 *
 * ─── Feedback Packet (6 bytes, sent at ~50Hz after homing) ──────────────
 *   Byte 0:       0xFE (start marker)
 *   Byte 1:       Current position (0-255, mapped)
 *   Byte 2:       Current position high byte (raw uint16)
 *   Byte 3:       Current position low byte
 *   Byte 4:       Current velocity (signed int8)
 *   Byte 5:       Checksum (XOR of bytes 1-4)
 *
 * Requires: TMCStepper, Adafruit_NeoPixel
 * Board: Adafruit QT Py RP2040 (Earle Philhower arduino-pico core)
 */

#include <TMCStepper.h>
#include <Adafruit_NeoPixel.h>

// ─── Pin Definitions ────────────────────────────────────────────────────────
#define STEP_PIN       4
#define DIR_PIN        6
#define EN_PIN         3
#define DIAG_PIN       25
#define NEOPIXEL_PIN   29
#define ONBOARD_NEO_PIN   12
#define ONBOARD_NEO_PWR   11

// ─── Configuration ──────────────────────────────────────────────────────────
#define NUM_LEDS           24
#define NEO_TYPE           (NEO_GRBW + NEO_KHZ800)

#define TMC_UART           Serial2
#define TMC_BAUD           115200
#define DRIVER_ADDRESS     0b00
#define R_SENSE            0.11f

#define RUN_CURRENT_MA     600
#define MICROSTEPS         2
#define STALL_THRESHOLD    1
#define HOMING_SPEED_US    4000
#define MAX_POSSIBLE_STEPS 20000

#define VEL_MIN_DELAY_US   4000
#define VEL_MAX_DELAY_US   2000
#define POS_RUN_SPEED_US   4000

#define CMD_START          0xFF
#define FB_START           0xFE
#define CMD_PACKET_SIZE    29
#define FB_PACKET_SIZE     6
#define HOST_BAUD          115200

#define FB_INTERVAL_MS     20

// Command flags
#define FLAG_HOMING        0x01

// ─── Global Objects ─────────────────────────────────────────────────────────
TMC2209Stepper driver(&TMC_UART, R_SENSE, DRIVER_ADDRESS);
Adafruit_NeoPixel ring(NUM_LEDS, NEOPIXEL_PIN, NEO_TYPE);
Adafruit_NeoPixel onboardLED(1, ONBOARD_NEO_PIN, NEO_GRB + NEO_KHZ800);

// ─── Shared State ───────────────────────────────────────────────────────────
mutex_t stateMutex;

volatile bool     homingComplete    = false;
volatile bool     homingInProgress  = false;
volatile int32_t  totalSteps        = 0;
volatile int32_t  currentPosition   = 0;
volatile int32_t  targetPosition    = 0;
volatile int8_t   velocityCommand   = 0;
volatile uint8_t  ledValues[NUM_LEDS] = {0};
volatile bool     newDataAvailable  = false;
volatile int8_t   fbVelocity        = 0;

// ─── Core 0: Setup & Serial Communication ───────────────────────────────────

void setup() {
  Serial.begin(HOST_BAUD);
  delay(3000);

  Serial.println(F("=== Stepper+NeoPixel Controller v3 ==="));

  // NeoPixel ring
  ring.begin();
  ring.clear();
  ring.show();

  // Onboard NeoPixel
  pinMode(ONBOARD_NEO_PWR, OUTPUT);
  digitalWrite(ONBOARD_NEO_PWR, HIGH);
  onboardLED.begin();
  onboardLED.clear();
  onboardLED.show();
  Serial.println(F("NeoPixels: OFF"));

  // TMC2209 UART
  TMC_UART.setTX(20);
  TMC_UART.setRX(5);
  TMC_UART.begin(TMC_BAUD);
  Serial.println(F("UART initialized"));

  // Stepper control pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIAG_PIN, INPUT_PULLUP);
  digitalWrite(EN_PIN, HIGH);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);

  mutex_init(&stateMutex);

  // Configure TMC2209 (but don't enable or home yet)
  configureDriver();

  Serial.println(F("READY"));
  Serial.println(F("Awaiting commands. Send homing flag to begin."));
}

void configureDriver() {
  Serial.println(F("Configuring TMC2209..."));

  driver.begin();
  delay(10);

  uint8_t version = driver.version();
  Serial.print(F("  Driver version: 0x"));
  Serial.println(version, HEX);
  if (version == 0 || version == 0xFF) {
    Serial.println(F("  WARNING: No UART response from TMC2209!"));
  }

  driver.toff(4);
  driver.blank_time(24);
  driver.rms_current(RUN_CURRENT_MA);
  driver.microsteps(MICROSTEPS);
  driver.en_spreadCycle(false);
  driver.pwm_autoscale(true);
  driver.pwm_autograd(true);
  driver.TCOOLTHRS(0xFFFFF);
  driver.semin(0);
  driver.SGTHRS(STALL_THRESHOLD);

  Serial.println(F("  TMC2209 configured."));
}

void performHoming() {
  // Signal Core 1 to pause
  mutex_enter_blocking(&stateMutex);
  homingComplete = false;
  homingInProgress = true;
  mutex_exit(&stateMutex);

  Serial.println(F("--- Homing Sequence ---"));

  // Enable driver
  digitalWrite(EN_PIN, LOW);
  delay(100);

  Serial.println(F("  Auto-tuning..."));
  for (int i = 0; i < 200; i++) {
    stepOnce();
    delayMicroseconds(HOMING_SPEED_US);
  }
  delay(300);

  Serial.println(F("  Phase 1: Finding zero..."));
  digitalWrite(DIR_PIN, LOW);
  delay(10);
  int32_t stepsToZero = moveUntilStall(MAX_POSSIBLE_STEPS);
  Serial.print(F("  Stalled after ")); Serial.print(stepsToZero); Serial.println(F(" steps"));
  delay(200);

  digitalWrite(DIR_PIN, HIGH);
  delay(10);
  for (int i = 0; i < 50; i++) {
    stepOnce();
    delayMicroseconds(HOMING_SPEED_US);
  }
  delay(100);
  digitalWrite(DIR_PIN, LOW);
  delay(10);
  moveUntilStall(500);
  delay(200);
  Serial.println(F("  Zero position established."));

  Serial.println(F("  Phase 2: Finding maximum..."));
  digitalWrite(DIR_PIN, HIGH);
  delay(10);
  int32_t stepsToMax = moveUntilStall(MAX_POSSIBLE_STEPS);
  Serial.print(F("  Stalled after ")); Serial.print(stepsToMax); Serial.println(F(" steps"));
  delay(200);

  int32_t measuredTotal = stepsToMax - 100;
  if (measuredTotal < 100) {
    Serial.println(F("  ERROR: Travel too short!"));
    measuredTotal = 1000;
  }

  Serial.println(F("  Phase 3: Returning to zero..."));
  digitalWrite(DIR_PIN, LOW);
  delay(10);
  for (int32_t i = 0; i < stepsToMax; i++) {
    stepOnce();
    delayMicroseconds(HOMING_SPEED_US);
    if (digitalRead(DIAG_PIN) == HIGH && i > 300) {
      Serial.println(F("  Stall on return - stopping early"));
      break;
    }
  }
  delay(200);

  // Update shared state
  mutex_enter_blocking(&stateMutex);
  totalSteps = measuredTotal;
  currentPosition = 0;
  targetPosition = 0;
  velocityCommand = 0;
  homingInProgress = false;
  homingComplete = true;
  mutex_exit(&stateMutex);

  Serial.print(F("  Total range: "));
  Serial.print(measuredTotal);
  Serial.println(F(" steps"));
  Serial.println(F("--- End Homing ---"));

  // This is the key handshake line the Python controller waits for
  Serial.print(F("HOMING_COMPLETE:"));
  Serial.println(measuredTotal);
}

int32_t moveUntilStall(int32_t maxSteps) {
  int32_t steps = 0;
  int32_t ignoreSteps = 300;

  for (int32_t i = 0; i < maxSteps; i++) {
    stepOnce();
    delayMicroseconds(HOMING_SPEED_US);
    steps++;
    if (i > ignoreSteps) {
      if (digitalRead(DIAG_PIN) == HIGH) {
        return steps;
      }
    }
  }
  Serial.println(F("  WARNING: Max steps reached without stall"));
  return steps;
}

inline void stepOnce() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(STEP_PIN, LOW);
}

void loop() {
  static uint8_t rxBuf[CMD_PACKET_SIZE];
  static uint8_t rxIdx = 0;

  while (Serial.available()) {
    uint8_t b = Serial.read();

    if (rxIdx == 0) {
      if (b == CMD_START) {
        rxBuf[rxIdx++] = b;
      }
      continue;
    }

    rxBuf[rxIdx++] = b;

    if (rxIdx >= CMD_PACKET_SIZE) {
      // Validate checksum (XOR of bytes 1..27)
      uint8_t checksum = 0;
      for (int i = 1; i < CMD_PACKET_SIZE - 1; i++) {
        checksum ^= rxBuf[i];
      }

      if (checksum == rxBuf[CMD_PACKET_SIZE - 1]) {
        uint8_t posCmd = rxBuf[1];
        int8_t  velCmd = (int8_t)rxBuf[2];
        uint8_t flags  = rxBuf[3];

        // Check homing flag — this blocks Core 0 during homing
        if (flags & FLAG_HOMING) {
          performHoming();
        }

        // Apply motor/LED commands (only meaningful after homing)
        if (homingComplete) {
          mutex_enter_blocking(&stateMutex);
          targetPosition = map((int32_t)posCmd, 0, 255, 0, totalSteps);
          velocityCommand = velCmd;
          for (int i = 0; i < NUM_LEDS; i++) {
            ledValues[i] = rxBuf[4 + i];
          }
          newDataAvailable = true;
          mutex_exit(&stateMutex);
        }
      } else {
        Serial.print(F("ERR:chk expected=0x"));
        Serial.print(checksum, HEX);
        Serial.print(F(" got=0x"));
        Serial.println(rxBuf[CMD_PACKET_SIZE - 1], HEX);
      }
      rxIdx = 0;
    }
  }
}

// ─── Core 1: Motor Control, LED Updates, Feedback ───────────────────────────

void setup1() {
  // Nothing to do — we wait for homingComplete in loop1
}

uint32_t velocityToDelay(uint8_t absMag) {
  if (absMag == 0) return 0;
  return VEL_MIN_DELAY_US - (uint32_t)(VEL_MIN_DELAY_US - VEL_MAX_DELAY_US) * (absMag - 1) / 126;
}

void sendFeedback(int32_t pos, int32_t total, int8_t vel) {
  uint8_t pos8 = (total > 0) ? (uint8_t)map(pos, 0, total, 0, 255) : 0;
  uint16_t pos16 = (uint16_t)constrain(pos, 0, 65535);
  uint8_t posH = (pos16 >> 8) & 0xFF;
  uint8_t posL = pos16 & 0xFF;

  uint8_t checksum = pos8 ^ posH ^ posL ^ (uint8_t)vel;

  uint8_t pkt[FB_PACKET_SIZE];
  pkt[0] = FB_START;
  pkt[1] = pos8;
  pkt[2] = posH;
  pkt[3] = posL;
  pkt[4] = (uint8_t)vel;
  pkt[5] = checksum;

  Serial.write(pkt, FB_PACKET_SIZE);
}

void loop1() {
  static uint32_t lastFeedbackMs = 0;

  // Don't do anything until homing is complete, and pause during re-homing
  if (!homingComplete || homingInProgress) {
    delay(10);
    return;
  }

  int32_t localTarget;
  int32_t localCurrent;
  int32_t localTotal;
  int8_t  localVelCmd;
  uint8_t localLEDs[NUM_LEDS];
  bool    hasNewData;

  mutex_enter_blocking(&stateMutex);
  localTarget  = targetPosition;
  localCurrent = currentPosition;
  localTotal   = totalSteps;
  localVelCmd  = velocityCommand;
  hasNewData   = newDataAvailable;
  if (hasNewData) {
    memcpy(localLEDs, (const uint8_t*)ledValues, NUM_LEDS);
    newDataAvailable = false;
  }
  mutex_exit(&stateMutex);

  // Update NeoPixels
  if (hasNewData) {
    uint32_t sum = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
      ring.setPixelColor(i, ring.Color(0, 0, 0, localLEDs[i]));
      sum += localLEDs[i];
    }
    ring.show();

    uint8_t avg = sum / NUM_LEDS;
    onboardLED.setPixelColor(0, onboardLED.Color(avg, avg, avg));
    onboardLED.show();
  }

  // Motor control
  int8_t actualVelocity = 0;

  if (localVelCmd != 0) {
    int8_t dir = (localVelCmd > 0) ? 1 : -1;
    uint8_t absMag = (uint8_t)abs(localVelCmd);
    uint32_t stepDelay = velocityToDelay(absMag);

    bool atMinLimit = (localCurrent <= 0 && dir < 0);
    bool atMaxLimit = (localCurrent >= localTotal && dir > 0);

    if (!atMinLimit && !atMaxLimit) {
      digitalWrite(DIR_PIN, (dir > 0) ? HIGH : LOW);
      delayMicroseconds(5);
      stepOnce();
      delayMicroseconds(stepDelay);
      localCurrent += dir;
      localCurrent = constrain(localCurrent, 0, localTotal);
      actualVelocity = localVelCmd;
    } else {
      actualVelocity = 0;
      delayMicroseconds(100);
    }
  } else {
    if (localCurrent != localTarget) {
      int32_t diff = localTarget - localCurrent;
      int32_t dir = (diff > 0) ? 1 : -1;

      digitalWrite(DIR_PIN, (dir > 0) ? HIGH : LOW);
      delayMicroseconds(5);
      stepOnce();
      delayMicroseconds(POS_RUN_SPEED_US);

      localCurrent += dir;
      localCurrent = constrain(localCurrent, 0, localTotal);
      actualVelocity = (dir > 0) ? 64 : -64;
    } else {
      actualVelocity = 0;
      delayMicroseconds(100);
    }
  }

  mutex_enter_blocking(&stateMutex);
  currentPosition = localCurrent;
  fbVelocity = actualVelocity;
  mutex_exit(&stateMutex);

  // Feedback at ~50Hz
  uint32_t now = millis();
  if (now - lastFeedbackMs >= FB_INTERVAL_MS) {
    lastFeedbackMs = now;
    sendFeedback(localCurrent, localTotal, actualVelocity);
  }
}
