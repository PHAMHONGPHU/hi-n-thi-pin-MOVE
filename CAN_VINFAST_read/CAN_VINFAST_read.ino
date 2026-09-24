#include <SPI.h>
#include <mcp_can.h>

// =============================
// PIN CONFIGURATION
// =============================
#define CAN_CS_PIN   10
#define CAN_INT_PIN   9

MCP_CAN CAN(CAN_CS_PIN);

// =============================
// CAN SETTINGS
// MCP2515 crystal = 8 MHz
// =============================
#define CAN_SPEED CAN_250KBPS
#define CAN_CLOCK MCP_8MHZ

// Keep this equal to the Serial Monitor setting. 500000 gives the Nano
// enough headroom to record short CAN bursts without changing CAN behavior.
#define SERIAL_BAUD 500000UL

// Community mapping identifies 0x0341 as charge voltage/current limits and
// charge-status fields. 0x0301 carries a closely related observed payload.
// These decoders only print serial output; they never transmit CAN data.
#define CHARGE_LIMIT_CAN_ID 0x0341UL
#define CHARGE_LIMIT_MIRROR_CAN_ID 0x0301UL
#define BATTERY_SOC_CAN_ID 0x030AUL
#define BATTERY_SOC_MIRROR_CAN_ID 0x034AUL
#define BATTERY_PRINT_INTERVAL_MS 1000UL

// Buffer
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

bool hasBatterySample = false;
unsigned char lastBatteryData[6];
unsigned long lastBatteryPrintMs = 0;

bool hasSocSample = false;
unsigned char lastSocData[4];
unsigned long lastSocPrintMs = 0;

void printMarker(const __FlashStringHelper *marker);
void checkTestMarkers();
void printChargeLimitCandidate();
void printSocCandidate();
void printCANFrame();

void setup()
{
  Serial.begin(SERIAL_BAUD);

  while (!Serial);

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F(" VinFast Evo Lite - CAN Sniffer"));
  Serial.println(F(" Arduino Nano + MCP2515"));
  Serial.println(F(" MCP2515 Crystal: 8 MHz"));
  Serial.println(F("======================================"));
  Serial.println();

  // SPI
  SPI.begin();

  // INT pin
  pinMode(CAN_INT_PIN, INPUT);

  // CS pin
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);

  Serial.println(F("Initializing MCP2515..."));

  // ---------------------------------
  // INIT CAN
  // ---------------------------------
  if (CAN.begin(MCP_ANY, CAN_SPEED, CAN_CLOCK) == CAN_OK)
  {
    Serial.println(F("MCP2515 INIT: OK"));
  }
  else
  {
    Serial.println(F("MCP2515 INIT: ERROR"));
    Serial.println(F("Check wiring / crystal / module"));
    
    while (1)
    {
      delay(1000);
    }
  }

  // ---------------------------------
  // LISTEN ONLY MODE
  // ---------------------------------
  CAN.setMode(MCP_LISTENONLY);

  Serial.println(F("CAN MODE: LISTEN ONLY"));
  Serial.println(F("CAN SPEED: 250 kbps"));
  Serial.print(F("SERIAL SPEED: "));
  Serial.println(SERIAL_BAUD);
  Serial.println(F("MARKERS: 1=KEY_ON, 0=KEY_OFF, U=UNLOCK, L=LOCK, C=CHARGER_ON, D=CHARGER_OFF"));
  Serial.println();
  Serial.println(F("Waiting for CAN frames..."));
  Serial.println(F("--------------------------------------"));
}


void loop()
{
  // These markers only label the serial log. They never transmit CAN data.
  checkTestMarkers();

  // ---------------------------------
  // CHECK CAN INTERRUPT
  // ---------------------------------
  if (digitalRead(CAN_INT_PIN) == LOW)
  {
    // ---------------------------------
    // READ CAN FRAME
    // ---------------------------------
    if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK)
    {
      printCANFrame();
      printChargeLimitCandidate();
      printSocCandidate();
    }
  }
}


// =====================================
// TEST MARKERS FROM SERIAL MONITOR
// Send one character immediately after the physical action.
// Line endings are ignored.
// =====================================
void checkTestMarkers()
{
  while (Serial.available() > 0)
  {
    char command = (char)Serial.read();

    switch (command)
    {
      case '1':
        printMarker(F("KEY_ON"));
        break;

      case '0':
        printMarker(F("KEY_OFF"));
        break;

      case 'U':
      case 'u':
        printMarker(F("UNLOCK"));
        break;

      case 'L':
      case 'l':
        printMarker(F("LOCK"));
        break;

      case 'C':
      case 'c':
        printMarker(F("CHARGER_ON"));
        break;

      case 'D':
      case 'd':
        printMarker(F("CHARGER_OFF"));
        break;

      // Ignore CR/LF and any other character so a Serial Monitor line
      // ending cannot create a false event.
      default:
        break;
    }
  }
}


void printMarker(const __FlashStringHelper *marker)
{
  Serial.print(millis());
  Serial.print(F(" ms | EVENT: "));
  Serial.println(marker);
}


// =====================================
// CHARGE-LIMIT CANDIDATE DECODER (PASSIVE)
// Community mapping for 0x0341: charge-voltage limit, charge-current limit,
// fully-charged status, and charger derating. Byte scaling is still pending
// confirmation. 0x0301 is included only as a related observed payload.
// A line is printed only when the sample changes or once per second.
// =====================================
void printChargeLimitCandidate()
{
  if ((rxId != CHARGE_LIMIT_CAN_ID && rxId != CHARGE_LIMIT_MIRROR_CAN_ID) || len < 6)
    return;

  bool changed = !hasBatterySample;

  if (!changed)
  {
    for (byte i = 0; i < 6; i++)
    {
      if (rxBuf[i] != lastBatteryData[i])
      {
        changed = true;
        break;
      }
    }
  }

  unsigned long now = millis();

  if (!changed && (now - lastBatteryPrintMs < BATTERY_PRINT_INTERVAL_MS))
    return;

  for (byte i = 0; i < 6; i++)
    lastBatteryData[i] = rxBuf[i];

  hasBatterySample = true;
  lastBatteryPrintMs = now;

  uint16_t voltageMv = ((uint16_t)rxBuf[0] << 8) | rxBuf[1];
  int16_t currentRaw = (int16_t)(((uint16_t)rxBuf[2] << 8) | rxBuf[3]);

  Serial.print(now);
  Serial.print(F(" ms | CHARGE_LIMIT_CANDIDATE | V_LIMIT_RAW="));
  Serial.print(voltageMv);
  Serial.print(F(" | V_LIMIT_CANDIDATE="));
  Serial.print(voltageMv / 1000);
  Serial.print('.');
  uint16_t millivolts = voltageMv % 1000;
  if (millivolts < 100) Serial.print('0');
  if (millivolts < 10) Serial.print('0');
  Serial.print(millivolts);
  Serial.print(F(" | I_LIMIT_RAW="));
  Serial.print(currentRaw);
  Serial.print(F(" | FULL_STATUS_RAW="));
  Serial.print(rxBuf[4]);
  Serial.print(F(" | DERATE_RAW="));
  Serial.print(rxBuf[5]);
  Serial.print(F(" | SOURCE=0x"));
  if (rxId < 0x1000) Serial.print('0');
  Serial.println(rxId, HEX);
}


// =====================================
// BMS 0x34A DECODER (PASSIVE)
// The community mapping identifies 0x34A as SoH. 0x030A has been observed
// with an identical payload. With dashboard SOC=82, byte 2 (0x52) is the
// SOC hypothesis; byte 3 (0x64) is the stronger SoH hypothesis. Both remain
// explicitly labelled hypotheses until a second known SOC reading confirms it.
// =====================================
void printSocCandidate()
{
  if ((rxId != BATTERY_SOC_CAN_ID && rxId != BATTERY_SOC_MIRROR_CAN_ID) || len < 4)
    return;

  bool changed = !hasSocSample;

  if (!changed)
  {
    for (byte i = 0; i < 4; i++)
    {
      if (rxBuf[i] != lastSocData[i])
      {
        changed = true;
        break;
      }
    }
  }

  unsigned long now = millis();

  if (!changed && (now - lastSocPrintMs < BATTERY_PRINT_INTERVAL_MS))
    return;

  for (byte i = 0; i < 4; i++)
    lastSocData[i] = rxBuf[i];

  hasSocSample = true;
  lastSocPrintMs = now;

  Serial.print(now);
  Serial.print(F(" ms | BMS_34A | SOC_HYPOTHESIS="));
  Serial.print(rxBuf[2]);
  Serial.print(F(" | SOH_HYPOTHESIS="));
  Serial.print(rxBuf[3]);
  Serial.print(F(" | RAW0="));
  Serial.print(rxBuf[0]);
  Serial.print(F(" | RAW1="));
  Serial.print(rxBuf[1]);
  Serial.print(F(" | SOURCE=0x"));
  if (rxId < 0x1000) Serial.print('0');
  Serial.println(rxId, HEX);
}


// =====================================
// PRINT CAN FRAME
// =====================================
void printCANFrame()
{
  // Timestamp
  Serial.print(millis());
  Serial.print(F(" ms"));

  Serial.print(F(" | ID: 0x"));

  if (rxId < 0x100)
    Serial.print("00");

  if (rxId < 0x1000)
    Serial.print("0");

  Serial.print(rxId, HEX);

  Serial.print(F(" | DLC: "));
  Serial.print(len);

  Serial.print(F(" | DATA: "));

  for (byte i = 0; i < len; i++)
  {
    if (rxBuf[i] < 0x10)
      Serial.print("0");

    Serial.print(rxBuf[i], HEX);

    if (i < len - 1)
      Serial.print(" ");
  }

  Serial.println();
}
