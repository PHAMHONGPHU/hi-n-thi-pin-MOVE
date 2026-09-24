#include "driver/twai.h"

#define CAN_TX GPIO_NUM_21
#define CAN_RX GPIO_NUM_22

#define DAC_PIN 25

void setup()
{
    Serial.begin(115200);

    // Cấu hình CAN
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX,
        CAN_RX,
        TWAI_MODE_NORMAL);

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
    {
        Serial.println("CAN Driver Install Failed");
        while (1);
    }

    if (twai_start() != ESP_OK)
    {
        Serial.println("CAN Start Failed");
        while (1);
    } 
     dacWrite(DAC_PIN,28);
    Serial.println("--------------------------------");
    Serial.println(" DEL BMS Reader");
    Serial.println("--------------------------------");
}

void loop()
{
   twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK)
    {
        if (message.extd == 1)
        {
            if (message.identifier == 0x181150C1)
            {
                uint16_t voltage =
                    message.data[0] |
                    (message.data[1] << 8);
                int16_t current =
                    message.data[2] |
                    (message.data[3] << 8);
                uint8_t soc =
                    message.data[4];
                uint8_t soh =
                    message.data[5];
                uint16_t capacity =
                    message.data[6] |
                    (message.data[7] << 8);
                Serial.println("---------------------");

                Serial.print("Voltage : ");
                Serial.print(voltage * 0.1);
                Serial.println(" V");

                Serial.print("Current : ");
                Serial.print(current * 0.1);
                Serial.println(" A");

                Serial.print("SOC : ");
                Serial.print(soc);
                Serial.println(" %");

                Serial.print("SOH : ");
                Serial.print(soh);
                Serial.println(" %");

                Serial.print("Capacity : ");
                Serial.println(capacity);

                // Xuất DAC
                uint8_t dac = map(soc,0,100,163,188);

                dacWrite(DAC_PIN,dac);
            }
        }
    }
}