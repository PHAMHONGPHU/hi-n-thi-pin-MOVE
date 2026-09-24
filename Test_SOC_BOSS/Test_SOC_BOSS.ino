#include <SoftwareSerial.h>

#define RS485_RX 2
#define RS485_TX 3
#define RS485_EN 4

SoftwareSerial rs485(RS485_RX, RS485_TX);

const uint8_t request[8] = {
  0xC5,0x5C,0x5A,0xAA,
  0x01,0x97,0x96,0x0D
};

uint8_t rx[8];
uint8_t pos = 0;

void sendBMS()
{
  uint8_t tx[17];

  tx[0] = 0xB6;
  tx[1] = 0x6B;
  tx[2] = 0xAA;
  tx[3] = 0x5A;

  tx[4] = 0x0A;      // Length

  // -------- DATA (10 byte) --------
  tx[5]  = 0x76;     // Voltage = 59.0V
  tx[6]  = 0x46;     // SOC =85%
  tx[7]  = 0x1E;     // Temp=30℃
  tx[8]  = 0x00;     // Current

  tx[9]  = 0x00;     // Charge Count H
  tx[10] = 0x00;     // Charge Count L

  tx[11] = 0x00;     // Discharge Count H
  tx[12] = 0x00;     // Discharge Count L

  tx[13] = 0x00;     // Fault
  tx[14] = 0x00;     // Status

  // XOR
  uint8_t cs = tx[4];
  for(int i=5;i<=14;i++)
      cs ^= tx[i];

  tx[15] = cs;
  tx[16] = 0x0D;

  digitalWrite(RS485_EN,HIGH);
  delayMicroseconds(80);

  rs485.write(tx,sizeof(tx));
  rs485.flush();

  delayMicroseconds(150);
  digitalWrite(RS485_EN,LOW);
}

void setup()
{
  pinMode(RS485_EN,OUTPUT);
  digitalWrite(RS485_EN,LOW);

  rs485.begin(9600);
}

void loop()
{
  while(rs485.available())
  {
    uint8_t b = rs485.read();

    if(b == request[pos])
    {
      pos++;

      if(pos == 8)
      {
        sendBMS();
        pos = 0;
      }
    }
    else
    {
      pos = (b == request[0]) ? 1 : 0;
    }
  }
}