#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac;

int soc = 0;
int direction = 1;   // 1=tăng, -1=giảm

unsigned long lastTime = 0;

void setup()
{
    Serial.begin(115200);

    Wire.begin();

    // đổi 0x61 nếu scanner ra địa chỉ khác
    dac.begin(0x60);

    Serial.println("SOC Up/Down Simulator");
}

void loop()
{
    if(millis()-lastTime>=1000)
    {
        lastTime=millis();

        soc += direction;

        // chạm đỉnh thì đổi chiều
        if(soc >= 100)
        {
            soc = 100;
            direction = -1;
        }

        // chạm đáy thì đổi chiều
        if(soc <= 0)
        {
            soc = 0;
            direction = 1;
        }

        // 0-100% -> DAC 0-4095
        uint16_t dacValue =
        map(
            soc,
            0,
            100,
            1420,
            1735
        );
        dac.setVoltage(dacValue,false);
        float voltage =
        dacValue * 5.0 / 4095.0;

        Serial.print("SOC=");
        Serial.print(soc);

        Serial.print("% DAC=");
        Serial.print(dacValue);
        Serial.print(" V=");
        Serial.println(voltage,3);
    }
}