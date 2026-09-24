#include <SPI.h>
#include <mcp_can.h>

#define CAN_CS 10

MCP_CAN CAN(CAN_CS);

//========================
// Dữ liệu BMS
//========================
float packVoltage = 0;
float packCurrent = 0;
uint8_t soc = 0;

uint16_t maxCell = 0;
uint16_t minCell = 0;
uint8_t maxCellNo = 0;
uint8_t minCellNo = 0;

int maxTemp = 0;
int minTemp = 0;
int avgTemp = 0;

unsigned long lastPrint = 0;

//========================

void setup()
{
    Serial.begin(115200);

    while (CAN_OK != CAN.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ))
    {
        Serial.println("CAN Init Fail");
        delay(1000);
    }

    CAN.setMode(MCP_NORMAL);

    Serial.println("=================================");
    Serial.println("      JK BMS CAN READER");
    Serial.println("=================================");
}

void loop()
{
    unsigned long rxId;
    byte len;
    byte buf[8];

    if (CAN_MSGAVAIL == CAN.checkReceive())
    {
        CAN.readMsgBuf(&rxId, &len, buf);

        //---------------------------------
        // 0x02F4 Voltage Current SOC
        //---------------------------------
        if (rxId == 0x02F4)
        {
            uint16_t rawVolt =
                ((uint16_t)buf[1] << 8) | buf[0];

            packVoltage = rawVolt * 0.1;

            uint16_t rawCurrent =
                ((uint16_t)buf[3] << 8) | buf[2];

            packCurrent = (rawCurrent * 0.1) - 400.0;

            soc = buf[4];
        }

        //---------------------------------
        // 0x04F4 Cell Max Min
        //---------------------------------
        else if (rxId == 0x04F4)
        {
            maxCell =
                ((uint16_t)buf[1] << 8) | buf[0];

            maxCellNo = buf[2] + 1;

            minCell =
                ((uint16_t)buf[4] << 8) | buf[3];

            minCellNo = buf[5] + 1;
        }

        //---------------------------------
        // 0x05F4 Temperature
        //---------------------------------
        else if (rxId == 0x05F4)
        {
            maxTemp = buf[0] - 50;
            minTemp = buf[2] - 50;
            avgTemp = buf[4] - 50;
        }
    }

    //---------------------------------
    // In dữ liệu mỗi giây
    //---------------------------------
    if (millis() - lastPrint > 1000)
    {
        lastPrint = millis();

        Serial.println();
        Serial.println("=========== JK BMS ===========");

        Serial.print("Voltage : ");
        Serial.print(packVoltage, 1);
        Serial.println(" V");

        Serial.print("Current : ");
        Serial.print(packCurrent, 1);
        Serial.println(" A");

        Serial.print("SOC     : ");
        Serial.print(soc);
        Serial.println(" %");

        Serial.println();

        Serial.print("Max Cell: ");
        Serial.print(maxCell);
        Serial.print(" mV (Cell ");
        Serial.print(maxCellNo);
        Serial.println(")");

        Serial.print("Min Cell: ");
        Serial.print(minCell);
        Serial.print(" mV (Cell ");
        Serial.print(minCellNo);
        Serial.println(")");

        Serial.println();

        Serial.print("Max Temp: ");
        Serial.print(maxTemp);
        Serial.println(" C");

        Serial.print("Min Temp: ");
        Serial.print(minTemp);
        Serial.println(" C");

        Serial.print("Avg Temp: ");
        Serial.print(avgTemp);
        Serial.println(" C");

        Serial.println("================================");
    }
}