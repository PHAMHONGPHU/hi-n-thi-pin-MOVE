#include <SPI.h>
#include <mcp_can.h>
#include <SoftwareSerial.h>

#define CAN_CS      10

#define RS485_RX    2
#define RS485_TX    3
#define RS485_EN    4

MCP_CAN CAN(CAN_CS);
SoftwareSerial rs485(RS485_RX, RS485_TX);

//==================================================
// JK BMS
//==================================================

volatile uint8_t SOC = 0;

//==================================================
// Lingbo Request
//==================================================

const uint8_t request[8] =
{
    0xC5,0x5C,0x5A,0xAA,
    0x01,0x97,0x96,0x0D
};

uint8_t pos = 0;

//==================================================
// CRC XOR
//==================================================

uint8_t CalcCRC(uint8_t *buf)
{
    uint8_t crc = buf[4];

    for(uint8_t i=5;i<=14;i++)
        crc ^= buf[i];

    return crc;
}

//==================================================
// CAN
//==================================================

void InitCAN()
{
    while(CAN.begin(MCP_ANY,CAN_250KBPS,MCP_8MHZ)!=CAN_OK)
    {
        Serial.println("CAN Init Fail");
        delay(1000);
    }

    CAN.setMode(MCP_NORMAL);

    Serial.println("CAN Ready");
}

void ReadCAN()
{
    unsigned long id;
    byte len;
    byte buf[8];

    if(CAN.checkReceive()!=CAN_MSGAVAIL)
        return;

    CAN.readMsgBuf(&id,&len,buf);

    //------------------------------------------------
    // JK BMS Battery Status
    //------------------------------------------------

    if(id==0x02F4)
    {
        SOC = buf[4];

        if(SOC>100)
            SOC=100;
    }
}

//==================================================
// RS485
//==================================================

void SendBMS()
{
    uint8_t tx[17];

    tx[0]=0xB6;
    tx[1]=0x6B;
    tx[2]=0xAA;
    tx[3]=0x5A;

    tx[4]=0x0A;

    //-------------------------
    // Fixed Data
    //-------------------------

    tx[5]=0x76;      // Voltage giả

    tx[6]=SOC;       // <-- SOC từ JK

    tx[7]=0x1E;      // Temp 30°C

    tx[8]=0x00;      // Current

    tx[9]=0x00;
    tx[10]=0x00;

    tx[11]=0x00;
    tx[12]=0x00;

    tx[13]=0x00;     // Fault

    tx[14]=0x00;     // Status

    tx[15]=CalcCRC(tx);

    tx[16]=0x0D;

    digitalWrite(RS485_EN,HIGH);

    rs485.write(tx,sizeof(tx));

    rs485.flush();

    digitalWrite(RS485_EN,LOW);

    //------------------------------------------------
    // Debug
    //------------------------------------------------

    Serial.print("TX SOC = ");
    Serial.println(SOC);
}

void CheckRequest()
{
    while(rs485.available())
    {
        uint8_t b = rs485.read();

        if(b==request[pos])
        {
            pos++;

            if(pos==8)
            {
                Serial.println("VCU Request");

                SendBMS();

                pos=0;
            }
        }
        else
        {
            if(b==request[0])
                pos=1;
            else
                pos=0;
        }
    }
}

//==================================================
// Setup
//==================================================

void setup()
{
    Serial.begin(115200);

    pinMode(RS485_EN,OUTPUT);

    digitalWrite(RS485_EN,LOW);

    rs485.begin(9600);

    InitCAN();

    Serial.println();
    Serial.println("==================================");
    Serial.println(" JK CAN -> Lingbo RS485 Bridge");
    Serial.println("==================================");
}

//==================================================
// Loop
//==================================================

void loop()
{
    ReadCAN();

    CheckRequest();
}