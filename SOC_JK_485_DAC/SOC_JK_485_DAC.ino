#include <Arduino.h>

// =====================================================
// ESP32 + MAX485 + JK BMS
// =====================================================

// MAX485 RO  -> ESP32 GPIO16
// MAX485 DI  <- ESP32 GPIO17
// MAX485 DE + RE -> ESP32 GPIO4

#define RS485_RX       16
#define RS485_TX       17
#define RS485_DE_RE     4

#define BMS_BAUD      2400

// ESP32 DAC
#define DAC_PIN         25

// DAC range
#define DAC_MIN          203
#define DAC_MAX        241


// =====================================================
// UART2
// =====================================================

HardwareSerial BMS(2);


// =====================================================
// FRAME BUFFER
// =====================================================

#define MAX_FRAME     128

uint8_t frame[MAX_FRAME];

uint16_t indexFrame = 0;
uint16_t expectedLength = 0;


// =====================================================
// RX STATE
// =====================================================

enum State
{
  WAIT_A5,
  WAIT_5A,
  READ_LENGTH,
  READ_DATA
};

State state = WAIT_A5;


// =====================================================
// SOC
// =====================================================

int lastSOC = -1;


// =====================================================
// RESET PARSER
// =====================================================

void resetParser()
{
  indexFrame = 0;
  expectedLength = 0;
  state = WAIT_A5;
}


// =====================================================
// OUTPUT DAC
// =====================================================

void outputDAC(uint8_t soc)
{
  // Giới hạn SOC
  soc = constrain(soc, 0, 100);

  // SOC 0~100%
  // chuyển thành DAC 0~255
  uint8_t dacValue = map(
    soc,
    0,
    100,
    DAC_MIN,
    DAC_MAX
  );

  // Xuất DAC GPIO25
  dacWrite(DAC_PIN, dacValue);
}


// =====================================================
// PROCESS JK BMS FRAME
// =====================================================

void processFrame()
{
  // ---------------------------------------------------
  // Frame phải có tối thiểu 14 byte
  // ---------------------------------------------------

  if (indexFrame < 14)
    return;


  // ---------------------------------------------------
  // Chỉ lấy MAIN FRAME
  //
  // A5 5A 5D 82 10 00
  // ---------------------------------------------------

  if (
    frame[0] != 0xA5 ||
    frame[1] != 0x5A ||
    frame[2] != 0x5D ||
    frame[3] != 0x82 ||
    frame[4] != 0x10 ||
    frame[5] != 0x00
  )
  {
    return;
  }


  // ---------------------------------------------------
  // ĐỌC SOC
  //
  // DATA bắt đầu từ frame[6]
  //
  // SOC nằm tại:
  //
  // frame[12] = High byte
  // frame[13] = Low byte
  // ---------------------------------------------------

  uint16_t soc =
    ((uint16_t)frame[12] << 8) |
    frame[13];


  // ---------------------------------------------------
  // Kiểm tra SOC
  // ---------------------------------------------------

  if (soc > 100)
    return;


  // ---------------------------------------------------
  // Chỉ xử lý khi SOC thay đổi
  // ---------------------------------------------------

  if (soc != lastSOC)
  {
    lastSOC = soc;


    // -----------------------------------------------
    // Serial
    // -----------------------------------------------

    Serial.print("SOC: ");
    Serial.print(soc);
    Serial.println("%");


    // -----------------------------------------------
    // DAC GPIO25
    // -----------------------------------------------

    outputDAC((uint8_t)soc);
  }
}


// =====================================================
// PROCESS ONE BYTE
// =====================================================

void processByte(uint8_t b)
{
  switch (state)
  {

    // =================================================
    // WAIT A5
    // =================================================

    case WAIT_A5:

      if (b == 0xA5)
      {
        frame[0] = b;

        indexFrame = 1;

        state = WAIT_5A;
      }

      break;


    // =================================================
    // WAIT 5A
    // =================================================

    case WAIT_5A:

      if (b == 0x5A)
      {
        frame[1] = b;

        indexFrame = 2;

        state = READ_LENGTH;
      }

      else if (b == 0xA5)
      {
        // A5 mới → tiếp tục chờ 5A

        frame[0] = b;

        indexFrame = 1;
      }

      else
      {
        resetParser();
      }

      break;


    // =================================================
    // READ LENGTH
    // =================================================

    case READ_LENGTH:

      frame[2] = b;

      indexFrame = 3;


      // -----------------------------------------------
      // Byte thứ 3 là LENGTH
      //
      // Tổng frame = LENGTH + 3
      // -----------------------------------------------

      expectedLength = (uint16_t)b + 3;


      // -----------------------------------------------
      // Bảo vệ buffer
      // -----------------------------------------------

      if (
        expectedLength < 6 ||
        expectedLength > MAX_FRAME
      )
      {
        resetParser();
      }
      else
      {
        state = READ_DATA;
      }

      break;


    // =================================================
    // READ DATA
    // =================================================

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


      // -----------------------------------------------
      // Đã nhận đủ frame
      // -----------------------------------------------

      if (indexFrame >= expectedLength)
      {
        processFrame();

        resetParser();
      }

      break;
  }
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  // ---------------------------------------------------
  // USB Serial
  // ---------------------------------------------------

  Serial.begin(115200);

  delay(300);


  // ---------------------------------------------------
  // MAX485
  // ---------------------------------------------------

  pinMode(RS485_DE_RE, OUTPUT);

  // LOW:
  // DE = 0 → không truyền
  // RE = 0 → cho phép nhận

  digitalWrite(RS485_DE_RE, LOW);


  // ---------------------------------------------------
  // JK BMS UART
  // ---------------------------------------------------

  BMS.begin(
    BMS_BAUD,
    SERIAL_8N1,
    RS485_RX,
    RS485_TX
  );


  // ---------------------------------------------------
  // DAC GPIO25
  // ---------------------------------------------------

  dacWrite(DAC_PIN, 0);


  // ---------------------------------------------------
  // Startup
  // ---------------------------------------------------

  Serial.println();
  Serial.println("==============================");
  Serial.println(" JK BMS SOC -> ESP32 DAC");
  Serial.println("==============================");

  Serial.println("RS485 RX : GPIO16");
  Serial.println("RS485 TX : GPIO17");
  Serial.println("DE/RE    : GPIO4");
  Serial.println("DAC OUT  : GPIO25");
  Serial.println("UART     : 2400 8N1");

  Serial.println();
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  while (BMS.available())
  {
    uint8_t b = BMS.read();

    processByte(b);
  }
}