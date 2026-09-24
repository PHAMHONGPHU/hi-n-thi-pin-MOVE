#include "driver/twai.h"

// =============================
// ESP32 CAN / TWAI
// =============================
#define CAN_TX GPIO_NUM_21
#define CAN_RX GPIO_NUM_22

// CAN ID của BMS BATT_ST1
#define BMS_SOC_CAN_ID 0x02F4

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 CAN - BMS SOC READER");
  Serial.println("================================");

  // Cấu hình CAN
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_LISTEN_ONLY);

  // 250 kbps theo BMS CAN Protocol V2.1
  twai_timing_config_t t_config =
      TWAI_TIMING_CONFIG_250KBITS();

  // Nhận tất cả frame
  twai_filter_config_t f_config =
      TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Cài đặt driver
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("ERROR: CAN driver install failed!");
    return;
  }

  // Start CAN
  if (twai_start() != ESP_OK) {
    Serial.println("ERROR: CAN start failed!");
    return;
  }

  Serial.println("CAN STARTED");
  Serial.println("Baudrate : 250 kbps");
  Serial.println("TX       : GPIO21");
  Serial.println("RX       : GPIO22");
  Serial.println("SOC ID   : 0x02F4");
  Serial.println();
}

void loop() {

  twai_message_t message;

  // Chờ frame CAN
  if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {

    // Chỉ xử lý Standard CAN ID 0x02F4
    if (!message.extd && message.identifier == BMS_SOC_CAN_ID) {

      // Kiểm tra frame có đủ 5 byte
      if (message.data_length_code >= 5) {

        // =============================
        // SOC nằm ở byte thứ 5
        // Start Bit = 32
        // Length = 8 bit
        // =============================
        uint8_t soc = message.data[4];

        Serial.println("--------------------------------");
        Serial.println("BMS BATT_ST1");

        Serial.print("CAN ID : 0x");
        Serial.println(message.identifier, HEX);

        Serial.print("DATA   : ");

        for (int i = 0; i < message.data_length_code; i++) {
          if (message.data[i] < 0x10)
            Serial.print("0");

          Serial.print(message.data[i], HEX);
          Serial.print(" ");
        }

        Serial.println();

        Serial.print("SOC    : ");
        Serial.print(soc);
        Serial.println(" %");

        Serial.println("--------------------------------");
        delay(1000);
      }
    }
  }
}