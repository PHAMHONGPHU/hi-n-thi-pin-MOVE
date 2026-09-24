#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== SERVO =====
Servo barrier;

// ===== PIN MAP =====
#define IR_IN   PA1
#define IR_OUT  PA2

#define IR_S1   PA3
#define IR_S2   PA4
#define IR_S3   PA5

#define SERVO_PIN PB7

// ===== SERVO ANGLE =====
#define SERVO_CLOSE  0
#define SERVO_OPEN   90

int freeSlots = 0;

void setup() {
  // IR sensors
  pinMode(IR_IN, INPUT_PULLUP);
  pinMode(IR_OUT, INPUT_PULLUP);

  pinMode(IR_S1, INPUT_PULLUP);
  pinMode(IR_S2, INPUT_PULLUP);
  pinMode(IR_S3, INPUT_PULLUP);

  // Servo
  barrier.attach(SERVO_PIN);
  barrier.write(SERVO_CLOSE);

  // I2C (PB9 SDA, PB8 SCL)
  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  delay(1500);
  lcd.clear();
}

void loop() {
  bool s1 = digitalRead(IR_S1);
  bool s2 = digitalRead(IR_S2);
  bool s3 = digitalRead(IR_S3);
  freeSlots = 0;
  if (s1 == HIGH) freeSlots++;
  if (s2 == HIGH) freeSlots++;
  if (s3 == HIGH) freeSlots++;

  // ===== LCD LINE 1 =====
  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print(s1 ? "E " : "O ");
  lcd.print("S2:");
  lcd.print(s2 ? "E " : "O ");
  lcd.print("S3:");
  lcd.print(s3 ? "E " : "O ");

  // ===== LCD LINE 2 =====
  lcd.setCursor(0, 1);
  if (freeSlots == 0) {
    lcd.print("FULL            ");
  } else {
    lcd.print("Free slots: ");
    lcd.print(freeSlots);
    lcd.print("   ");
  }

  // ===== XE VAO =====
  if (digitalRead(IR_IN) == LOW) {
    if (freeSlots > 0) {
      openBarrier();
    } else {
      lcd.setCursor(0, 1);
      lcd.print("FULL - NO ENTRY ");
    }
    delay(1000); // chống đếm lặp
  }

  // ===== XE RA =====
  if (digitalRead(IR_OUT) == LOW) {
    openBarrier();
    delay(1000);
  }

  delay(150);
}

// ===== OPEN BARRIER =====
void openBarrier() {
  barrier.write(SERVO_OPEN);
  delay(2000);   // thời gian xe đi qua
  barrier.write(SERVO_CLOSE);
}
