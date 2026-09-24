#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BLEScan.h>

// ============================================================
// JK BMS
// ============================================================

#define JK_MAC       "28:D4:1E:6B:11:B7"
#define SERVICE_UUID "FFE0"
#define CHAR_UUID    "FFE1"

#define FRAME_SIZE   300
#define SOC_INDEX    173

uint8_t rxBuffer[FRAME_SIZE];
uint16_t rxIndex = 0;

bool receiving = false;
bool connected = false;
bool doConnect = false;

BLEClient* client = nullptr;
BLERemoteCharacteristic* chr = nullptr;
BLEAdvertisedDevice* target = nullptr;


// ============================================================
// CRC
// ============================================================

uint8_t calcCRC(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++)
        crc += data[i];

    return crc;
}


// ============================================================
// GỬI LỆNH JK
// ============================================================

void sendCommand(uint8_t cmdType)
{
    uint8_t cmd[20] =
    {
        0xAA, 0x55, 0x90, 0xEB,
        cmdType, 0x00,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0
    };

    cmd[19] = calcCRC(cmd, 19);

    chr->writeValue(cmd, 20, false);
}


// ============================================================
// NHẬN DATA
// ============================================================

void notifyCallback(
    BLERemoteCharacteristic* c,
    uint8_t* data,
    size_t length,
    bool notify)
{
    if (!receiving)
    {
        if (length < 4 ||
            data[0] != 0x55 ||
            data[1] != 0xAA ||
            data[2] != 0xEB ||
            data[3] != 0x90)
        {
            return;
        }

        rxIndex = 0;
        receiving = true;
    }

    for (size_t i = 0; i < length; i++)
    {
        if (rxIndex < FRAME_SIZE)
            rxBuffer[rxIndex++] = data[i];
    }

    if (rxIndex >= FRAME_SIZE)
    {
        // Chỉ lấy frame 0x02
        if (rxBuffer[4] == 0x02)
        {
            uint8_t soc = rxBuffer[SOC_INDEX];

            if (soc <= 100)
            {
                Serial.print("SOC = ");
                Serial.print(soc);
                Serial.println("%");
            }
        }

        rxIndex = 0;
        receiving = false;
    }
}


// ============================================================
// SCAN
// ============================================================

class ScanCallback : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice device)
    {
        String mac =
            device.getAddress().toString().c_str();

        if (mac.equalsIgnoreCase(JK_MAC))
        {
            target = new BLEAdvertisedDevice(device);
            doConnect = true;

            BLEDevice::getScan()->stop();
        }
    }
};


// ============================================================
// KẾT NỐI JK
// ============================================================

bool connectJK()
{
    client = BLEDevice::createClient();

    if (!client->connect(target))
        return false;

    BLERemoteService* service =
        client->getService(BLEUUID(SERVICE_UUID));

    if (!service)
        return false;

    chr =
        service->getCharacteristic(
            BLEUUID(CHAR_UUID)
        );

    if (!chr)
        return false;

    if (!chr->canNotify())
        return false;

    chr->registerForNotify(notifyCallback);

    connected = true;

    Serial.println("JK CONNECTED");

    delay(500);

    // Quan trọng: JK cần 0x97 -> 0x96
    sendCommand(0x97);

    delay(500);

    sendCommand(0x96);

    return true;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    BLEDevice::init("ESP32-JK");

    BLEScan* scan = BLEDevice::getScan();

    scan->setAdvertisedDeviceCallbacks(
        new ScanCallback()
    );

    scan->setActiveScan(true);

    scan->setInterval(100);
    scan->setWindow(80);

    Serial.println("Scanning JK...");

    scan->start(5, false);
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    if (doConnect && !connected)
    {
        doConnect = false;

        if (!connectJK())
        {
            Serial.println("CONNECT FAILED");
        }
    }

    // Request lại mỗi 1 giây
    static unsigned long lastRequest = 0;

    if (connected &&
        millis() - lastRequest >= 1000)
    {
        lastRequest = millis();

        sendCommand(0x97);

        delay(500);

        sendCommand(0x96);
    }

    delay(10);
}