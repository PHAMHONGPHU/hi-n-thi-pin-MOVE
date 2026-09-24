#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// =====================================================
// BLE BMS
// =====================================================

BLEClient* pClient = NULL;
BLERemoteCharacteristic* pChar = NULL;

bool connected = false;

BLEAddress addr("8B:59:72:EA:D5:40");

BLEUUID serviceUUID(
    "0000FFE0-0000-1000-8000-00805F9B34FB"
);

BLEUUID charUUID(
    "0000FFE1-0000-1000-8000-00805F9B34FB"
);


// =====================================================
// PIN
// =====================================================

// DAC ESP32 GPIO25
#define DAC_PIN 25

// GPIO2 đọc trạng thái khóa
// HIGH = khóa ON
// LOW  = khóa OFF
#define POWER_SENSE_PIN 2


// =====================================================
// DAC CONFIG
// =====================================================

// DAC ESP32:
// 0   = 0V
// 255 = ~3.3V

// Khi khóa ON:
// SOC 0%   -> DAC 96
// SOC 100% -> DAC 109

#define DAC_MIN 96
#define DAC_MAX 109

// Khi khóa OFF
// Giữ DAC thấp
#define DAC_OFF 40


// =====================================================
// LOGIN COMMAND
// =====================================================

uint8_t loginCmd[] = {
    0xA5, 0x0B, 0x00, 0x58, 0x58,
    0x19, 0x0A, 0x1E,
    0x0E, 0x28, 0x0D
};


// =====================================================
// BIẾN
// =====================================================

// SOC hiện tại từ BMS
uint8_t currentSOC = 0;

// Trạng thái khóa hiện tại
// true  = ON
// false = OFF
bool displayPowerOn = false;


// =====================================================
// CHỐNG NHIỄU GPIO
// =====================================================

// Thời gian kiểm tra GPIO
unsigned long lastPowerCheck = 0;

#define POWER_CHECK_INTERVAL 50

// Trạng thái GPIO đọc lần trước
int lastRawPowerState = LOW;

// Trạng thái GPIO đã ổn định
int stablePowerState = LOW;


// =====================================================
// ĐỌC TRẠNG THÁI KHÓA
// =====================================================

bool isPowerOn()
{
    int state = digitalRead(POWER_SENSE_PIN);

    if (state == HIGH)
    {
        return true;
    }
    else
    {
        return false;
    }
}


// =====================================================
// XUẤT DAC THEO SOC
// =====================================================

void outputDAC(uint8_t soc)
{
    // Giới hạn SOC
    soc = constrain(soc, 0, 100);

    // Map:
    //
    // SOC 0%   -> DAC 96
    // SOC 100% -> DAC 109

    int dacValue = map(
        soc,
        0,
        100,
        DAC_MIN,
        DAC_MAX
    );

    dacWrite(DAC_PIN, dacValue);

    Serial.print("SOC = ");
    Serial.print(soc);
    Serial.print("%");

    Serial.print(" | DAC = ");
    Serial.print(dacValue);

    Serial.print(" | V = ");
    Serial.print(
        dacValue * 3.3 / 255.0,
        3
    );

    Serial.println("V");
}


// =====================================================
// DAC WAKEUP
// =====================================================

void displayWakeup()
{
    /*
       Khi khóa vừa ON:

       Đầu tiên đưa DAC lên 96
       để màn hình có thể khởi động.

       Sau đó mới đưa DAC theo SOC.
    */

    dacWrite(
        DAC_PIN,
        DAC_MIN
    );

    Serial.print("DISPLAY WAKEUP");
    Serial.print(" | DAC = ");
    Serial.print(DAC_MIN);

    Serial.print(" | V = ");
    Serial.print(
        DAC_MIN * 3.3 / 255.0,
        3
    );

    Serial.println("V");
}


// =====================================================
// DAC KHI KHÓA OFF
// =====================================================

void displayOff()
{
    /*
       Khi khóa OFF:

       DAC = 40

       Không để DAC = 0 vì theo yêu cầu
       màn hình sẽ không thể khởi động lại
       khi bật khóa.
    */

    dacWrite(
        DAC_PIN,
        DAC_OFF
    );

    Serial.print("DISPLAY OFF");
    Serial.print(" | DAC = ");
    Serial.print(DAC_OFF);

    Serial.print(" | V = ");
    Serial.print(
        DAC_OFF * 3.3 / 255.0,
        3
    );

    Serial.println("V");
}


// =====================================================
// BLE NOTIFY CALLBACK
// =====================================================

void notifyCallback(
    BLERemoteCharacteristic* c,
    uint8_t* data,
    size_t len,
    bool isNotify
)
{
    // Kiểm tra độ dài frame
    if (len < 12)
    {
        return;
    }

    // =================================================
    // BYTE 11 = SOC
    // =================================================

    uint8_t soc = data[11];

    soc = constrain(
        soc,
        0,
        100
    );

    // Lưu SOC
    currentSOC = soc;

    Serial.print("BMS SOC = ");
    Serial.print(currentSOC);
    Serial.println("%");


    // =================================================
    // CHỈ XUẤT SOC KHI KHÓA ON
    // =================================================

    if (displayPowerOn)
    {
        outputDAC(currentSOC);
    }
    else
    {
        displayOff();
    }
}


// =====================================================
// CONNECT BMS
// =====================================================

bool connectBMS()
{
    Serial.println();
    Serial.println("Connecting BMS...");

    // Tạo BLE client
    pClient = BLEDevice::createClient();

    // Kết nối BMS
    if (!pClient->connect(addr))
    {
        Serial.println("Connect fail");

        return false;
    }

    Serial.println("BMS Connected");


    // =================================================
    // TÌM SERVICE FFE0
    // =================================================

    BLERemoteService* service =
        pClient->getService(
            serviceUUID
        );

    if (service == nullptr)
    {
        Serial.println("No FFE0");

        pClient->disconnect();

        return false;
    }


    // =================================================
    // TÌM CHARACTERISTIC FFE1
    // =================================================

    pChar =
        service->getCharacteristic(
            charUUID
        );

    if (pChar == nullptr)
    {
        Serial.println("No FFE1");

        pClient->disconnect();

        return false;
    }


    // =================================================
    // ĐĂNG KÝ NOTIFY
    // =================================================

    pChar->registerForNotify(
        notifyCallback
    );

    delay(500);


    // =================================================
    // SEND LOGIN
    // =================================================

    Serial.println("Send Login");

    pChar->writeValue(
        loginCmd,
        sizeof(loginCmd),
        true
    );


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

    // =================================================
    // DAC
    // =================================================

    pinMode(
        DAC_PIN,
        OUTPUT
    );


    // =================================================
    // GPIO2 ĐỌC KHÓA
    // =================================================

    pinMode(
        POWER_SENSE_PIN,
        INPUT
    );


    // =================================================
    // ĐỌC TRẠNG THÁI KHÓA BAN ĐẦU
    // =================================================

    int initialState =
        digitalRead(
            POWER_SENSE_PIN
        );

    if (initialState == HIGH)
    {
        displayPowerOn = true;

        Serial.println(
            "INITIAL POWER = ON"
        );

        // Cho màn hình thức dậy
        displayWakeup();
    }
    else
    {
        displayPowerOn = false;

        Serial.println(
            "INITIAL POWER = OFF"
        );

        // Giữ DAC ở mức OFF
        displayOff();
    }


    // =================================================
    // SERIAL INFO
    // =================================================

    Serial.println();
    Serial.println("==============================");
    Serial.println(" ESP32 BMS SOC -> DAC");
    Serial.println("==============================");

    Serial.print("GPIO POWER = GPIO");
    Serial.println(
        POWER_SENSE_PIN
    );

    Serial.println(
        "HIGH = POWER ON"
    );

    Serial.println(
        "LOW  = POWER OFF"
    );

    Serial.print("DAC MIN = ");
    Serial.println(DAC_MIN);

    Serial.print("DAC MAX = ");
    Serial.println(DAC_MAX);

    Serial.print("DAC OFF = ");
    Serial.println(DAC_OFF);


    // =================================================
    // BLE INIT
    // =================================================

    BLEDevice::init("");

    connectBMS();

    delay(2000);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    // =================================================
    // 1. KIỂM TRA TRẠNG THÁI KHÓA
    // =================================================

    if (
        millis() - lastPowerCheck
        >= POWER_CHECK_INTERVAL
    )
    {
        lastPowerCheck = millis();


        // Đọc GPIO2
        int rawState =
            digitalRead(
                POWER_SENSE_PIN
            );


        // =================================================
        // KIỂM TRA TRẠNG THÁI THAY ĐỔI
        // =================================================

        if (rawState != lastRawPowerState)
        {
            lastRawPowerState =
                rawState;

            /*
               Không đổi trạng thái ngay lập tức.
               Chờ lần đọc tiếp theo để giảm nhiễu.
            */

            return;
        }


        // =================================================
        // XÁC NHẬN TRẠNG THÁI ỔN ĐỊNH
        // =================================================

        if (rawState != stablePowerState)
        {
            stablePowerState =
                rawState;


            // =================================================
            // OFF -> ON
            // =================================================

            if (
                stablePowerState
                == HIGH
            )
            {
                displayPowerOn = true;

                Serial.println();
                Serial.println(
                    ">>> KEY ON <<<"
                );


                // -----------------------------------------
                // BƯỚC 1:
                // Đưa DAC lên mức khởi động
                // -----------------------------------------

                displayWakeup();


                // -----------------------------------------
                // BƯỚC 2:
                // Chờ màn hình khởi động
                // -----------------------------------------

                delay(100);


                // -----------------------------------------
                // BƯỚC 3:
                // Xuất DAC theo SOC hiện tại
                // -----------------------------------------

                outputDAC(
                    currentSOC
                );
            }


            // =================================================
            // ON -> OFF
            // =================================================

            else
            {
                displayPowerOn = false;

                Serial.println();
                Serial.println(
                    ">>> KEY OFF <<<"
                );


                // Đưa DAC về mức OFF
                displayOff();
            }
        }
    }


    // =================================================
    // 2. KIỂM TRA BLE
    // =================================================

    if (
        pClient &&
        !pClient->isConnected()
    )
    {
        connected = false;

        Serial.println();
        Serial.println(
            "BMS Disconnected"
        );

        delay(2000);

        connectBMS();
    }


    delay(10);
}