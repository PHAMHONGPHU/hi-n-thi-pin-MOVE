#include <SoftwareSerial.h>

#define RS485_RX 2
#define RS485_TX 3
#define RS485_EN 4

SoftwareSerial rs485(RS485_RX, RS485_TX);

unsigned long lastByteTime = 0;

void setup()
{
    Serial.begin(115200);

    pinMode(RS485_EN, OUTPUT);

    // Chế độ nhận
    digitalWrite(RS485_EN, LOW);

    rs485.begin(9600);

    Serial.println("RS485 Monitor Ready");
}

void loop()
{
    while (rs485.available())
    {
        byte c = rs485.read();

        if (millis() - lastByteTime > 20)
        {
            Serial.println();
            Serial.print("RX: ");
        }

        if (c < 0x10)
            Serial.print("0");

        Serial.print(c, HEX);
        Serial.print(" ");

        lastByteTime = millis();
    }
}