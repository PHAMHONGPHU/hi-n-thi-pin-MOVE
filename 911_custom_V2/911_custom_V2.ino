
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// =====================================================
// PIN CONFIG
// =====================================================
#define KEY_PIN       4
#define BRAKE_PIN     26
#define P_LED         5
#define P_BUTTON_PIN  27
#define DAC_PIN       25

// =====================================================
// P STATE
// =====================================================
bool pState = false;

bool oldKeyState = false;
bool oldPButtonState = HIGH;

unsigned long lastButtonTime = 0;
const uint32_t debounceMs = 50;

// =====================================================
// BLE
// =====================================================
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pChar = nullptr;

bool connected = false;

BLEAddress addr("01:26:45:58:B0:46");

BLEUUID serviceUUID("0000FFE0-0000-1000-8000-00805F9B34FB");
BLEUUID charUUID("0000FFE1-0000-1000-8000-00805F9B34FB");

uint8_t loginCmd[] =
{
    0xA5,0x0B,0x00,0x58,0x58,
    0x19,0x0A,0x1E,
    0x0E,0x28,0x0D
};

// =====================================================
// UPDATE P LED
// =====================================================
void updatePIndicator()
{
    digitalWrite(P_LED, pState ? LOW : HIGH);
}

// =====================================================
// DAC OUTPUT FROM SOC
// =====================================================
void updateBatteryDisplay(uint8_t soc)
{ 
    
    soc = constrain(soc, 0, 100);

    int dacValue = map(soc, 0, 100, 160, 182);
    if(digitalRead(4)){
       dacWrite(DAC_PIN, dacValue);
    } else
    {
        dacWrite(DAC_PIN, 0);
    }

    Serial.print("SOC = ");
    Serial.print(soc);
    Serial.print("%   DAC = ");
    Serial.print(dacValue);
    Serial.print("   Voltage = ");
    Serial.println(dacValue * 3.3f / 255.0f, 2);
}

// =====================================================
// BLE NOTIFY CALLBACK
// =====================================================
void notifyCallback(
    BLERemoteCharacteristic* c,
    uint8_t* data,
    size_t len,
    bool isNotify)
{
    if(len < 12)
        return;

    uint8_t soc = data[11];

    updateBatteryDisplay(soc);
}

// =====================================================
// CONNECT BMS
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

    pChar->writeValue(
        loginCmd,
        sizeof(loginCmd),
        true);

    connected = true;

    Serial.println("BMS Ready");

    return true;
}

// =====================================================
// HANDLE KEY
// =====================================================
void handleKey()
{
    bool keyState = digitalRead(KEY_PIN);

    // KEY ON EDGE
    if(keyState && !oldKeyState)
    {
        pState = true;

        updatePIndicator();

        Serial.println("KEY ON -> P ON");
    } 

    // KEY OFF EDGE
    if(!keyState && oldKeyState)
    {
        pState = false;

        updatePIndicator();

        dacWrite(DAC_PIN, 0);

        Serial.println("KEY OFF");
    }

    oldKeyState = keyState;
}

// =====================================================
// HANDLE P BUTTON
// =====================================================
void handlePButton()
{
    if(!digitalRead(KEY_PIN))
        return;

    bool buttonState = digitalRead(P_BUTTON_PIN);

    if(oldPButtonState == HIGH &&
       buttonState == LOW &&
       millis() - lastButtonTime > debounceMs)
    {
        lastButtonTime = millis();

        pState = !pState;

        updatePIndicator();

        Serial.print("P BUTTON -> ");
        Serial.println(pState ? "P ON" : "P OFF");
    }

    oldPButtonState = buttonState;
}

// =====================================================
// BLE CHECK
// =====================================================
void handleBLE()
{
    if(pClient && !pClient->isConnected())
    {
        connected = false;

        Serial.println("BMS Disconnected");

        delay(2000);

        connectBMS();
    }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
    Serial.begin(115200);

    // INPUT
    pinMode(KEY_PIN, INPUT_PULLDOWN);
    pinMode(BRAKE_PIN, INPUT_PULLDOWN);

    pinMode(P_BUTTON_PIN, INPUT_PULLUP);

    // OUTPUT
    pinMode(P_LED, OUTPUT);
    pinMode(DAC_PIN, OUTPUT);

    digitalWrite(P_LED, HIGH);
    dacWrite(DAC_PIN, 0);

    // BLE
    BLEDevice::init("");

    connectBMS();
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
    handleKey();

    handlePButton();

    handleBLE();

    delay(5);
}
