
#define DAC_PIN 25   // GPIO25 = DAC1

uint8_t socFake = 0;
int direction = 1;

void outputGauge(uint8_t soc)
{
    // Giới hạn SOC từ 0 -> 100%
    soc = constrain(soc, 0, 100);

    // Map SOC sang giá trị DAC
    // DAC ESP32: 0 -> 255
    // Ở đây map từ 100 -> 140
    uint8_t dacValue = map(soc, 0, 100, 163,188);

    // Xuất DAC
    dacWrite(DAC_PIN, dacValue);

    // Tính điện áp xấp xỉ
    float voltage = (dacValue / 255.0f) * 3.3f;

    // Debug
    Serial.print("SOC: ");
    Serial.print(soc);
    Serial.print("%");

    Serial.print(" | DAC: ");
    Serial.print(dacValue);

    Serial.print(" | Voltage: ");
    Serial.print(voltage, 3);
    Serial.println(" V");
}

void setup()
{
    Serial.begin(115200);

    pinMode(DAC_PIN, OUTPUT);

    // Reset DAC
    dacWrite(DAC_PIN, 0);

    Serial.println("ESP32 DAC Fake SOC Test");
}

void loop()
{
    outputGauge(socFake);

    // Tăng / giảm SOC giả lập
    socFake += direction * 2;

    // Đảo chiều khi đạt ngưỡng
    if (socFake >= 100)
    {
        socFake = 100;
        direction = -1;
    }

    if (socFake <= 0)
    {
        socFake = 0;
        direction = 1;
    }

    delay(2000);
}
