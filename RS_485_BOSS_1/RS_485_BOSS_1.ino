#include <SoftwareSerial.h>

#define RS485_RX 2
#define RS485_TX 3
#define RS485_EN 4

SoftwareSerial rs485(RS485_RX, RS485_TX);

uint8_t buffer[256];
uint16_t indexBuf = 0;

void printFrame(uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);

    rs485.begin(9600);

    pinMode(RS485_EN, OUTPUT);
    digitalWrite(RS485_EN, LOW);      // Receive

    Serial.println("===== RS485 Monitor =====");
}

void loop()
{
    while (rs485.available())
    {
        uint8_t c = rs485.read();

        buffer[indexBuf++] = c;

        if (indexBuf >= sizeof(buffer))
            indexBuf = 0;

        //---------------------------------
        // C5 Request
        //---------------------------------
        if (indexBuf >= 8)
        {
            if (buffer[indexBuf-8] == 0xC5 &&
                buffer[indexBuf-7] == 0x5C &&
                buffer[indexBuf-1] == 0x0D)
            {
                Serial.print("[VCU->BMS] ");

                printFrame(&buffer[indexBuf-8],8);
            }
        }

        //---------------------------------
        // B6 Response
        //---------------------------------
        if (c == 0xB6)
        {
            delay(8);

            uint8_t frame[128];

            int len = 0;

            frame[len++] = 0xB6;

            while(rs485.available())
            {
                frame[len++] = rs485.read();

                if(len>=120)
                    break;
            }

            Serial.print("[BMS->VCU] ");

            printFrame(frame,len);
        }

        //---------------------------------
        // Lingbo 5B
        //---------------------------------
        if(c==0x5B)
        {
            delay(8);

            uint8_t frame[128];

            int len=0;

            frame[len++]=0x5B;

            while(rs485.available())
            {
                frame[len++]=rs485.read();

                if(len>=120)
                    break;
            }

            Serial.print("[LINGBO] ");

            printFrame(frame,len);
        }
    }
}