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

// ============================================================
// FRAME
// ============================================================

#define FRAME_SIZE 300

uint8_t rxBuffer[FRAME_SIZE];

uint16_t rxIndex = 0;

bool receiving = false;


// ============================================================
// BLE
// ============================================================

BLEClient* client = nullptr;

BLERemoteCharacteristic* chr = nullptr;

BLEAdvertisedDevice* target = nullptr;

bool connected = false;

bool doConnect = false;


// ============================================================
// CRC
// ============================================================

uint8_t calcCRC(uint8_t* data, uint8_t len)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++)
    {
        crc += data[i];
    }

    return crc;
}


// ============================================================
// GỬI COMMAND JK
// ============================================================

void sendCommand(uint8_t command)
{
    if (!connected || chr == nullptr)
        return;

    uint8_t cmd[20] =
    {
        0xAA,
        0x55,
        0x90,
        0xEB,

        command,
        0x00,

        0x00,
        0x00,
        0x00,
        0x00,

        0x00,
        0x00,
        0x00,
        0x00,

        0x00,
        0x00,
        0x00,
        0x00,

        0x00
    };

    // CRC byte cuối
    cmd[19] = calcCRC(cmd, 19);

    Serial.print("TX: ");

    for (int i = 0; i < 20; i++)
    {
        if (cmd[i] < 0x10)
            Serial.print("0");

        Serial.print(cmd[i], HEX);
        Serial.print(" ");
    }

    Serial.println();

    chr->writeValue(
        cmd,
        20,
        false
    );
}


// ============================================================
// REQUEST JK
// ============================================================

void requestJK()
{
    Serial.println();
    Serial.println("==============================");
    Serial.println("REQUEST DEVICE INFO 0x97");
    Serial.println("==============================");

    // --------------------------------------------------------
    // 0x97
    // --------------------------------------------------------

    sendCommand(0x97);

    delay(500);

    // --------------------------------------------------------
    // 0x96
    // --------------------------------------------------------

    Serial.println();
    Serial.println("==============================");
    Serial.println("REQUEST CELL INFO 0x96");
    Serial.println("==============================");

    sendCommand(0x96);
}


// ============================================================
// IN FRAME
// ============================================================

void printFrame()
{
    Serial.println();
    Serial.println();
    Serial.println("========================================");
    Serial.println("             FRAME COMPLETE");
    Serial.println("========================================");

    Serial.print("Length: ");
    Serial.println(rxIndex);

    Serial.print("Type: 0x");

    if (rxBuffer[4] < 0x10)
        Serial.print("0");

    Serial.println(rxBuffer[4], HEX);


    // --------------------------------------------------------
    // PRINT FRAME
    // --------------------------------------------------------

    for (int i = 0; i < FRAME_SIZE; i++)
    {
        if (i % 10 == 0)
        {
            Serial.println();

            Serial.print("[");
            Serial.print(i);
            Serial.print("] ");
        }

        if (rxBuffer[i] < 0x10)
            Serial.print("0");

        Serial.print(
            rxBuffer[i],
            HEX
        );

        Serial.print(" ");
    }

    Serial.println();


    // --------------------------------------------------------
    // FRAME 0x02
    // --------------------------------------------------------

    if (rxBuffer[4] == 0x02)
    {
        Serial.println();
        Serial.println("----------------------------------------");
        Serial.println("          FRAME 0x02 FOUND");
        Serial.println("----------------------------------------");

        Serial.print("byte[170] = 0x");

        if (rxBuffer[170] < 0x10)
            Serial.print("0");

        Serial.print(
            rxBuffer[170],
            HEX
        );

        Serial.print("  DEC = ");

        Serial.println(
            rxBuffer[170]
        );


        Serial.print("byte[171] = 0x");

        if (rxBuffer[171] < 0x10)
            Serial.print("0");

        Serial.print(
            rxBuffer[171],
            HEX
        );

        Serial.print("  DEC = ");

        Serial.println(
            rxBuffer[171]
        );


        Serial.print("byte[172] = 0x");

        if (rxBuffer[172] < 0x10)
            Serial.print("0");

        Serial.print(
            rxBuffer[172],
            HEX
        );

        Serial.print("  DEC = ");

        Serial.println(
            rxBuffer[172]
        );


        // ----------------------------------------------------
        // SOC
        // ----------------------------------------------------

        Serial.print("byte[173] = 0x");

        if (rxBuffer[173] < 0x10)
            Serial.print("0");

        Serial.print(
            rxBuffer[173],
            HEX
        );

        Serial.print("  DEC = ");

        Serial.println(
            rxBuffer[173]
        );


        Serial.println();

        Serial.print("******** SOC = ");

        Serial.print(
            rxBuffer[173]
        );

        Serial.println("% ********");

        Serial.println(
            "----------------------------------------"
        );
    }
}


// ============================================================
// BLE NOTIFY
// ============================================================

void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify)
{
    if (length == 0)
        return;


    // ========================================================
    // TÌM HEADER
    // ========================================================

    if (!receiving)
    {
        if (length >= 4 &&
            data[0] == 0x55 &&
            data[1] == 0xAA &&
            data[2] == 0xEB &&
            data[3] == 0x90)
        {
            rxIndex = 0;

            receiving = true;
        }
        else
        {
            return;
        }
    }


    // ========================================================
    // GHÉP DATA
    // ========================================================

    for (size_t i = 0; i < length; i++)
    {
        if (rxIndex < FRAME_SIZE)
        {
            rxBuffer[rxIndex] =
                data[i];

            rxIndex++;
        }
    }


    // ========================================================
    // ĐỦ FRAME
    // ========================================================

    if (rxIndex >= FRAME_SIZE)
    {
        printFrame();

        rxIndex = 0;

        receiving = false;
    }
}


// ============================================================
// SCAN CALLBACK
// ============================================================

class ScanCallback :
    public BLEAdvertisedDeviceCallbacks
{
    void onResult(
        BLEAdvertisedDevice device)
    {
        String mac =
            device.getAddress()
                  .toString()
                  .c_str();

        if (mac.equalsIgnoreCase(JK_MAC))
        {
            Serial.println();
            Serial.println(
                ">>> JK FOUND"
            );

            target =
                new BLEAdvertisedDevice(
                    device
                );

            doConnect = true;

            BLEDevice::getScan()->stop();
        }
    }
};


// ============================================================
// CONNECT JK
// ============================================================

bool connectJK()
{
    if (target == nullptr)
        return false;


    Serial.println();
    Serial.println(
        "Connecting JK..."
    );


    // --------------------------------------------------------
    // CREATE CLIENT
    // --------------------------------------------------------

    client =
        BLEDevice::createClient();


    // --------------------------------------------------------
    // CONNECT
    // --------------------------------------------------------

    if (!client->connect(target))
    {
        Serial.println(
            "CONNECT FAILED"
        );

        return false;
    }


    Serial.println(
        "JK CONNECTED"
    );


    // --------------------------------------------------------
    // SERVICE
    // --------------------------------------------------------

    BLERemoteService* service =
        client->getService(
            BLEUUID(SERVICE_UUID)
        );

    if (service == nullptr)
    {
        Serial.println(
            "FFE0 NOT FOUND"
        );

        client->disconnect();

        return false;
    }


    Serial.println(
        "FFE0 FOUND"
    );


    // --------------------------------------------------------
    // CHARACTERISTIC
    // --------------------------------------------------------

    chr =
        service->getCharacteristic(
            BLEUUID(CHAR_UUID)
        );

    if (chr == nullptr)
    {
        Serial.println(
            "FFE1 NOT FOUND"
        );

        client->disconnect();

        return false;
    }


    Serial.println(
        "FFE1 FOUND"
    );


    // --------------------------------------------------------
    // NOTIFY
    // --------------------------------------------------------

    if (!chr->canNotify())
    {
        Serial.println(
            "NOTIFY NOT SUPPORTED"
        );

        client->disconnect();

        return false;
    }


    chr->registerForNotify(
        notifyCallback
    );


    connected = true;


    Serial.println(
        "BLE READY"
    );


    // --------------------------------------------------------
    // REQUEST
    // --------------------------------------------------------

    delay(500);

    requestJK();


    return true;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        " ESP32 JK BMS SOC TEST"
    );

    Serial.println(
        "================================"
    );

    Serial.print(
        "MAC: "
    );

    Serial.println(
        JK_MAC
    );


    // ========================================================
    // BLE DEFAULT ESP32
    // ========================================================

    BLEDevice::init(
        "ESP32-JK"
    );


    // ========================================================
    // SCAN
    // ========================================================

    BLEScan* scan =
        BLEDevice::getScan();


    scan->setAdvertisedDeviceCallbacks(
        new ScanCallback()
    );


    scan->setActiveScan(
        true
    );


    scan->setInterval(
        100
    );


    scan->setWindow(
        80
    );


    Serial.println();
    Serial.println(
        "Scanning JK..."
    );


    scan->start(
        5,
        false
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // ========================================================
    // CONNECT
    // ========================================================

    if (doConnect &&
        !connected)
    {
        doConnect = false;

        if (!connectJK())
        {
            Serial.println(
                "Connect error"
            );
        }
    }


    // ========================================================
    // KIỂM TRA CONNECTION
    // ========================================================

    if (connected &&
        client != nullptr)
    {
        if (!client->isConnected())
        {
            Serial.println();
            Serial.println(
                "JK DISCONNECTED"
            );

            connected = false;

            chr = nullptr;

            rxIndex = 0;

            receiving = false;
        }
    }


    delay(10);
}