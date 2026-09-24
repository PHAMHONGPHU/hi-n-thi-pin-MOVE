#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <mcp_can.h>

#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);
Adafruit_MCP4725 dac;

unsigned long canId;
byte len;
byte buf[8];

void setup()
{
    Serial.begin(115200);

    Wire.begin();

    dac.begin(0x60);

    while(CAN.begin(MCP_ANY,CAN_250KBPS,MCP_8MHZ)!=CAN_OK)
    {
        Serial.println("CAN init fail");
        delay(1000);
    }

    CAN.setMode(MCP_NORMAL);

    Serial.println("CAN OK");
}

void loop()
{
    if(CAN_MSGAVAIL==CAN.checkReceive())
    {
        CAN.readMsgBuf(&canId,&len,buf);

        if((canId & 0x1FFFFFFF)==0x181150C1)
        {
            uint8_t soc=buf[4];

            Serial.print("SOC=");
            Serial.print(soc);
            Serial.println("%");

            // map 0-100% -> DAC 0-4095
            uint16_t dacValue= map(soc,0,100,1420,1730);
            dac.setVoltage(dacValue, false);
            Serial.print("DAC=");
            Serial.println(dacValue);
        }
    }
}