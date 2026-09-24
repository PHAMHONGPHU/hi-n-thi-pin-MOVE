#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// =====================================================
// P LED
// =====================================================
#define KEY_PIN     4
#define BRAKE_PIN   26
#define P_LED       5
#define P_BUTTON_PIN 27 
bool oldPButton = HIGH;
bool pState = false;
bool oldKey = false;

// =====================================================
// DAC
// =====================================================
#define DAC_PIN 25

// =====================================================
// BLE
// =====================================================
BLEClient* pClient = NULL;
BLERemoteCharacteristic* pChar = NULL;

bool connected = false;

//BLEAddress addr("01:26:45:58:B0:46");
BLEAddress addr("CD:6C:99:80:62:18");
BLEUUID serviceUUID("0000FFE0-0000-1000-8000-00805F9B34FB");
BLEUUID charUUID("0000FFE1-0000-1000-8000-00805F9B34FB");

uint8_t loginCmd[] = {
  0xA5,0x0B,0x00,0x58,0x58,
  0x19,0x0A,0x1E,
  0x0E,0x28,0x0D
};

// =====================================================
// NHẬN DATA TỪ BMS
// =====================================================
void notifyCallback(
  BLERemoteCharacteristic* c,
  uint8_t* data,
  size_t len,
  bool isNotify)
{
  if(len < 12) return;

  uint8_t soc = data[11];

  Serial.print("SOC = ");
  Serial.print(soc);
  Serial.println("%");

  // map SOC -> DAC
  soc = constrain(soc, 0, 100);

  int dacValue = map(soc, 0, 100, 160, 182);

  dacWrite(DAC_PIN, dacValue);

  Serial.print("DAC = ");
  Serial.print(dacValue);

  Serial.print("   Voltage = ");
  Serial.println(dacValue * 3.3 / 255.0, 2);
}

// =====================================================
// KẾT NỐI BMS
// =====================================================
bool connectBMS()
{
  Serial.println("Connecting BMS...");

  pClient = BLEDevice::createClient();

  if(!pClient->connect(addr))
  {
    Serial.println("Connect Fail");
    return false;
  }

  Serial.println("Connected");

  BLERemoteService* service =
    pClient->getService(serviceUUID);

  if(service == nullptr)
  {
    Serial.println("No FFE0");

    pClient->disconnect();

    return false;
  }

  pChar = service->getCharacteristic(charUUID);

  if(pChar == nullptr)
  {
    Serial.println("No FFE1");

    pClient->disconnect();

    return false;
  }

  pChar->registerForNotify(notifyCallback);

  delay(500);

  Serial.println("Send Login");

  pChar->writeValue(loginCmd, sizeof(loginCmd), true);

  connected = true;

  Serial.println("BMS Ready");

  return true;
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  // ================= P LED =================
  pinMode(KEY_PIN, INPUT_PULLDOWN);
  pinMode(BRAKE_PIN, INPUT_PULLDOWN);

  pinMode(P_LED, OUTPUT);
  pinMode(P_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(P_LED, HIGH);

  // ================= DAC =================
  pinMode(DAC_PIN, OUTPUT);

  // mặc định tắt màn hình
  dacWrite(DAC_PIN, 0);

  // ================= BLE =================
  BLEDevice::init("");

  connectBMS();
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  // =================================================
  // ĐỌC INPUT
  // =================================================
  
  bool keyState   = digitalRead(KEY_PIN);
  bool brakeState = digitalRead(BRAKE_PIN);
  bool pButtonState = digitalRead(P_BUTTON_PIN);
  // =================================================
  // VỪA BẬT KHÓA
  // =================================================
  if(keyState == HIGH && oldKey == LOW)
  {
    pState = true;

    // bật LED P
    digitalWrite(P_LED, LOW);

    // bật màn hình trước
   // dacWrite(DAC_PIN, 170);

    Serial.println("KEY ON -> P ON");
  }

  // =================================================
  // ĐANG BẬT KHÓA
  // =================================================
if(keyState == HIGH)
{
    // bóp phanh -> thoát P
    if(brakeState == HIGH && pState)
    {
        pState = false;

        digitalWrite(P_LED, HIGH);

        Serial.println("BRAKE -> P OFF");
    }

    // nhấn nút P (cạnh xuống HIGH -> LOW)
    if(oldPButton == HIGH && pButtonState == LOW)
    {
        pState = !pState;

        digitalWrite(P_LED, pState ? LOW : HIGH);

        Serial.println(
            pState ?
            "BUTTON -> P ON" :
            "BUTTON -> P OFF"
        );
    }
}

  // =================================================
  // TẮT KHÓA
  // =================================================
  if(keyState == LOW)
  {
    pState = false;

    // tắt LED P
    digitalWrite(P_LED, HIGH);

    // tắt màn hình
    dacWrite(DAC_PIN, 0);

    Serial.println("KEY OFF -> DAC OFF");
  }

  oldKey = keyState;

  // =================================================
  // KIỂM TRA BLE
  // =================================================
  if(pClient && !pClient->isConnected())
  {
    connected = false;

    Serial.println("BMS Disconnected");

    delay(2000);

    connectBMS();
  }

  delay(10);
  oldPButton = pButtonState;
}