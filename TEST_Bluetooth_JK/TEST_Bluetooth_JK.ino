
/*
   ============================================================
   ESP32 + JK BMS BLE TEST
   Library:
       ESP32 BLE Arduino
       #include <BLEDevice.h>

   Chức năng:
       1. Scan JK BMS
       2. Tìm MAC
       3. Connect
       4. Tìm Service FFE0
       5. Tìm Characteristic FFE1
       6. Subscribe Notify
       7. Gửi command JK
       8. Nhận và in frame HEX
       9. Tự reconnect

   Serial Monitor:
       115200 baud
   ============================================================
*/

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <BLEScan.h>

// ============================================================
// JK BMS CONFIG
// ============================================================

// MAC JK BMS của bạn
#define JK_MAC "28:D4:1E:6B:11:B7"

// JK BLE UUID
#define JK_SERVICE_UUID        "FFE0"
#define JK_CHARACTERISTIC_UUID "FFE1"

// ============================================================
// GLOBAL
// ============================================================

BLEClient* pClient = nullptr;

BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

BLEAdvertisedDevice* targetDevice = nullptr;

BLEScan* pBLEScan = nullptr;

bool doConnect = false;
bool connected = false;

unsigned long lastScan = 0;

#define SCAN_TIME 5

// ============================================================
// PRINT HEX
// ============================================================

void printHex(const uint8_t* data, size_t length)
{
    Serial.print("RX [");
    Serial.print(length);
    Serial.print("]: ");

    for (size_t i = 0; i < length; i++)
    {
        if (data[i] < 0x10)
            Serial.print("0");

        Serial.print(data[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}

// ============================================================
// PRINT MAC
// ============================================================

void printMAC(BLEAddress address)
{
    Serial.println(address.toString().c_str());
}

// ============================================================
// CRC JK
// ============================================================
//
// JK command CRC:
// Sum toàn bộ byte trước CRC
// lấy 8 bit thấp
//
// ============================================================

uint8_t jkCRC(const uint8_t* data, uint16_t length)
{
    uint8_t crc = 0;

    for (uint16_t i = 0; i < length; i++)
    {
        crc += data[i];
    }

    return crc;
}

// ============================================================
// SEND JK COMMAND
// ============================================================
//
// Theo code gốc của bạn:
//
// AA 55 90 EB
// address
// length
// value 4 byte little endian
// ...
// CRC
//
// Tổng frame = 20 byte
//
// ============================================================

void writeRegister(
    uint8_t address,
    uint32_t value,
    uint8_t length)
{
    if (!connected || pRemoteCharacteristic == nullptr)
    {
        Serial.println("Cannot send command - JK not connected");
        return;
    }

    uint8_t frame[20] = {0};

    // Header
    frame[0] = 0xAA;
    frame[1] = 0x55;
    frame[2] = 0x90;
    frame[3] = 0xEB;

    // Address
    frame[4] = address;

    // Length
    frame[5] = length;

    // Value - Little Endian
    frame[6] = (value >> 0) & 0xFF;
    frame[7] = (value >> 8) & 0xFF;
    frame[8] = (value >> 16) & 0xFF;
    frame[9] = (value >> 24) & 0xFF;

    // Các byte còn lại = 0
    for (int i = 10; i < 19; i++)
    {
        frame[i] = 0x00;
    }

    // CRC
    frame[19] = jkCRC(frame, 19);

    // ============================================
    // DEBUG
    // ============================================

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("TX JK COMMAND");

    Serial.print("Address : 0x");
    if (address < 0x10)
        Serial.print("0");

    Serial.println(address, HEX);

    Serial.print("Length  : ");
    Serial.println(length);

    Serial.print("Value   : 0x");
    Serial.println(value, HEX);

    Serial.print("Frame   : ");

    for (int i = 0; i < 20; i++)
    {
        if (frame[i] < 0x10)
            Serial.print("0");

        Serial.print(frame[i], HEX);
        Serial.print(" ");
    }

    Serial.println();

    Serial.println("--------------------------------");

    // ============================================
    // WRITE
    // ============================================

    pRemoteCharacteristic->writeValue(
        frame,
        sizeof(frame),
        false
    );
}

// ============================================================
// REQUEST DEVICE INFO
// ============================================================

void requestDeviceInfo()
{
    Serial.println();
    Serial.println("Requesting JK DEVICE INFO...");

    writeRegister(
        0x97,
        0x00000000,
        0x00
    );
}

// ============================================================
// REQUEST CELL INFO
// ============================================================

void requestCellInfo()
{
    Serial.println();
    Serial.println("Requesting JK CELL INFO...");

    writeRegister(
        0x96,
        0x00000000,
        0x00
    );
}

// ============================================================
// NOTIFY CALLBACK
// ============================================================

static void notifyCallback(
    BLERemoteCharacteristic* pCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify)
{
    Serial.println();
    Serial.println("========================================");

    Serial.println("JK NOTIFICATION RECEIVED");

    Serial.print("Length = ");
    Serial.println(length);

    // In HEX
    printHex(pData, length);

    // ============================================
    // KIỂM TRA HEADER JK
    // ============================================

    if (length >= 4)
    {
        if (pData[0] == 0x55 &&
            pData[1] == 0xAA &&
            pData[2] == 0xEB &&
            pData[3] == 0x90)
        {
            Serial.println();
            Serial.println("******** JK FRAME OK ********");

            // Byte 4 = frame type
            if (length > 4)
            {
                Serial.print("Frame Type = 0x");

                if (pData[4] < 0x10)
                    Serial.print("0");

                Serial.println(pData[4], HEX);

                switch (pData[4])
                {
                    case 0x01:
                        Serial.println("Type: BMS SETTINGS");
                        break;

                    case 0x02:
                        Serial.println("Type: CELL DATA");
                        break;

                    case 0x03:
                        Serial.println("Type: DEVICE INFO");
                        break;

                    default:
                        Serial.println("Type: UNKNOWN");
                        break;
                }
            }

            Serial.println("*******************************");
        }
    }

    Serial.println("========================================");
}

// ============================================================
// CLIENT CALLBACK
// ============================================================

class MyClientCallbacks : public BLEClientCallbacks
{
    void onConnect(BLEClient* pclient)
    {
        Serial.println();
        Serial.println("========================================");
        Serial.println("        JK BMS CONNECTED");
        Serial.println("========================================");

        connected = true;
    }

    void onDisconnect(BLEClient* pclient)
    {
        Serial.println();
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("        JK BMS DISCONNECTED");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

        connected = false;

        pRemoteCharacteristic = nullptr;
    }
};

// ============================================================
// SCAN CALLBACK
// ============================================================

class MyAdvertisedDeviceCallbacks
    : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        String mac =
            advertisedDevice.getAddress().toString().c_str();

        Serial.print("BLE: ");
        Serial.print(mac);

        if (advertisedDevice.haveName())
        {
            Serial.print(" | Name: ");
            Serial.print(
                advertisedDevice.getName().c_str()
            );
        }

        Serial.print(" | RSSI: ");
        Serial.println(
            advertisedDevice.getRSSI()
        );

        // ============================================
        // SO SÁNH MAC
        // ============================================

        if (mac.equalsIgnoreCase(JK_MAC))
        {
            Serial.println();
            Serial.println("########################################");
            Serial.println("          FOUND JK BMS");
            Serial.println("########################################");

            Serial.print("MAC: ");
            Serial.println(mac);

            // Copy device
            targetDevice =
                new BLEAdvertisedDevice(
                    advertisedDevice
                );

            doConnect = true;

            BLEDevice::getScan()->stop();

            Serial.println("Scan stopped.");
            Serial.println();
        }
    }
};

// ============================================================
// CONNECT TO JK
// ============================================================

bool connectToJK()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("Connecting to JK BMS...");
    Serial.println("========================================");

    if (targetDevice == nullptr)
    {
        Serial.println("Target device is NULL");

        return false;
    }

    // ============================================
    // CREATE CLIENT
    // ============================================

    pClient = BLEDevice::createClient();

    if (pClient == nullptr)
    {
        Serial.println("Failed to create BLE client");

        return false;
    }

    pClient->setClientCallbacks(
        new MyClientCallbacks()
    );

    // ============================================
    // CONNECT
    // ============================================

    Serial.println("Connecting...");

    if (!pClient->connect(targetDevice))
    {
        Serial.println();
        Serial.println("!!! JK CONNECTION FAILED !!!");

        return false;
    }

    Serial.println();
    Serial.println("JK BLE CONNECTED!");

    // ============================================
    // GET SERVICE FFE0
    // ============================================

    Serial.println();
    Serial.println("Searching service FFE0...");

    BLERemoteService* pRemoteService =
        pClient->getService(
            BLEUUID(JK_SERVICE_UUID)
        );

    if (pRemoteService == nullptr)
    {
        Serial.println("!!! FFE0 SERVICE NOT FOUND !!!");

        pClient->disconnect();

        return false;
    }

    Serial.println("FFE0 SERVICE FOUND!");

    // ============================================
    // GET CHARACTERISTIC FFE1
    // ============================================

    Serial.println();
    Serial.println("Searching characteristic FFE1...");

    pRemoteCharacteristic =
        pRemoteService->getCharacteristic(
            BLEUUID(JK_CHARACTERISTIC_UUID)
        );

    if (pRemoteCharacteristic == nullptr)
    {
        Serial.println(
            "!!! FFE1 CHARACTERISTIC NOT FOUND !!!"
        );

        pClient->disconnect();

        return false;
    }

    Serial.println("FFE1 CHARACTERISTIC FOUND!");

    // ============================================
    // PROPERTY
    // ============================================

    Serial.print("Can Read  : ");
    Serial.println(
        pRemoteCharacteristic->canRead()
    );

    Serial.print("Can Write : ");
    Serial.println(
        pRemoteCharacteristic->canWrite()
    );

    Serial.print("Can Notify: ");
    Serial.println(
        pRemoteCharacteristic->canNotify()
    );

    // ============================================
    // REGISTER NOTIFY
    // ============================================

    if (pRemoteCharacteristic->canNotify())
    {
        Serial.println();
        Serial.println("Registering NOTIFY...");

        pRemoteCharacteristic->registerForNotify(
            notifyCallback
        );

        Serial.println("NOTIFY REGISTERED!");
    }
    else
    {
        Serial.println();
        Serial.println(
            "WARNING: FFE1 does not support NOTIFY"
        );
    }

    connected = true;

    // ============================================
    // CONNECT SUCCESS
    // ============================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("       JK BMS READY");
    Serial.println("========================================");

    delay(1000);

    // ============================================
    // REQUEST DEVICE INFO
    // ============================================

    requestDeviceInfo();

    delay(1000);

    // ============================================
    // REQUEST CELL INFO
    // ============================================

    requestCellInfo();

    return true;
}

// ============================================================
// START SCAN
// ============================================================

void startScan()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("          BLE SCANNING...");
    Serial.println("========================================");

    found:
    
    pBLEScan->clearResults();

    pBLEScan->start(
        SCAN_TIME,
        false
    );

    pBLEScan->clearResults();

    Serial.println();
    Serial.println("Scan finished.");
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println();
    Serial.println("########################################");
    Serial.println("#                                      #");
    Serial.println("#      ESP32 + JK BMS BLE TEST         #");
    Serial.println("#                                      #");
    Serial.println("########################################");

    Serial.println();

    Serial.print("Target JK MAC: ");
    Serial.println(JK_MAC);

    Serial.print("Service UUID: ");
    Serial.println(JK_SERVICE_UUID);

    Serial.print("Characteristic UUID: ");
    Serial.println(JK_CHARACTERISTIC_UUID);

    // ============================================
    // INIT BLE
    // ============================================

    Serial.println();
    Serial.println("Initializing BLE...");

    BLEDevice::init(
        "ESP32-JK-BMS"
    );

    Serial.println("BLE initialized.");

    // ============================================
    // SCAN
    // ============================================

    pBLEScan =
        BLEDevice::getScan();

    pBLEScan->setAdvertisedDeviceCallbacks(
        new MyAdvertisedDeviceCallbacks()
    );

    pBLEScan->setActiveScan(true);

    pBLEScan->setInterval(100);

    pBLEScan->setWindow(80);

    Serial.println("BLE scan configured.");

    delay(500);

    // ============================================
    // FIRST SCAN
    // ============================================

    startScan();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ============================================
    // CHƯA CONNECT
    // ============================================

    if (!connected)
    {
        if (doConnect)
        {
            doConnect = false;

            if (connectToJK())
            {
                Serial.println();
                Serial.println(
                    ">>> CONNECTION SUCCESS <<<"
                );
            }
            else
            {
                Serial.println();
                Serial.println(
                    ">>> CONNECTION FAILED <<<"
                );

                connected = false;

                delay(2000);
            }
        }
        else
        {
            // Scan lại mỗi 10 giây
            if (millis() - lastScan > 10000)
            {
                lastScan = millis();

                startScan();
            }
        }
    }

    // ============================================
    // KIỂM TRA CONNECTION
    // ============================================

    if (connected)
    {
        if (pClient == nullptr ||
            !pClient->isConnected())
        {
            Serial.println();
            Serial.println(
                "Connection lost!"
            );

            connected = false;

            pRemoteCharacteristic = nullptr;

            delay(1000);
        }
    }

    delay(20);
}

