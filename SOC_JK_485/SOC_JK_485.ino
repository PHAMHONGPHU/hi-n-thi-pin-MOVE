#include <Arduino.h>

// ===============================
// ESP32 + MAX485
// ===============================
#define RS485_RX      16   // RO -> GPIO16
#define RS485_TX      17    // DI <- GPIO17
#define RS485_DE_RE    4    // DE + RE

#define BMS_BAUD    2400
#define MAX_FRAME   128

HardwareSerial BMS(2);

uint8_t frame[MAX_FRAME];
uint16_t indexFrame = 0;
uint16_t expectedLength = 0;

enum State {
  WAIT_A5,
  WAIT_5A,
  READ_LENGTH,
  READ_DATA
};

State state = WAIT_A5;


// ===============================
// RESET
// ===============================
void resetParser()
{
  indexFrame = 0;
  expectedLength = 0;
  state = WAIT_A5;
}


// ===============================
// XỬ LÝ FRAME
// ===============================
void processFrame()
{
  // Chỉ lấy frame:
  // A5 5A 5D 82 10 00 ...

  if (indexFrame < 14)
    return;

  if (frame[0] != 0xA5 ||
      frame[1] != 0x5A ||
      frame[2] != 0x5D ||
      frame[3] != 0x82 ||
      frame[4] != 0x10 ||
      frame[5] != 0x00)
  {
    return;
  }

  // SOC = DATA offset 6
  // DATA bắt đầu từ frame[6]
  // => SOC nằm tại frame[12] và frame[13]

  uint16_t soc =
      ((uint16_t)frame[12] << 8) |
      frame[13];

  // Chỉ in giá trị hợp lệ
  if (soc <= 100)
  {
    Serial.print("SOC: ");
    Serial.print(soc);
    Serial.println("%");
  }
}


// ===============================
// NHẬN TỪNG BYTE
// ===============================
void processByte(uint8_t b)
{
  switch (state)
  {
    case WAIT_A5:

      if (b == 0xA5)
      {
        frame[0] = b;
        indexFrame = 1;
        state = WAIT_5A;
      }

      break;


    case WAIT_5A:

      if (b == 0x5A)
      {
        frame[1] = b;
        indexFrame = 2;
        state = READ_LENGTH;
      }
      else if (b == 0xA5)
      {
        frame[0] = b;
        indexFrame = 1;
      }
      else
      {
        resetParser();
      }

      break;


    case READ_LENGTH:

      frame[2] = b;
      indexFrame = 3;

      // Tổng frame = length + 3
      expectedLength = (uint16_t)b + 3;

      if (expectedLength < 6 ||
          expectedLength > MAX_FRAME)
      {
        resetParser();
      }
      else
      {
        state = READ_DATA;
      }

      break;


    case READ_DATA:

      if (indexFrame < MAX_FRAME)
      {
        frame[indexFrame++] = b;
      }
      else
      {
        resetParser();
        break;
      }

      if (indexFrame >= expectedLength)
      {
        processFrame();
        resetParser();
      }

      break;
  }
}


// ===============================
// SETUP
// ===============================
void setup()
{
  Serial.begin(115200);

  // MAX485 receive mode
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);

  // JK BMS: 2400 8N1
  BMS.begin(
    BMS_BAUD,
    SERIAL_8N1,
    RS485_RX,
    RS485_TX
  );
}


// ===============================
// LOOP
// ===============================
void loop()
{
  while (BMS.available())
  {
    processByte(BMS.read());
  }
}