#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Mouse.h>
#include "st7789_2.25.h"

// --- THÔNG SỐ TRACKPAD ---
#define PIN_TRACKPAD_RESET  16
#define PIN_TRACKPAD_SDA    18
#define PIN_TRACKPAD_MOTION 22
#define PIN_TRACKPAD_SCL    23

#define TRACKPAD_I2C_ADDR   0x3B
#define REG_PRODUCT_ID      0x00
#define REG_MOTION          0x02

// Định nghĩa màu RGB565
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_GREEN   0x07E0
#define COLOR_RED     0xF800
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF

TFTDriver tft;



void resetTrackpad() {
  pinMode(PIN_TRACKPAD_RESET, OUTPUT);
  digitalWrite(PIN_TRACKPAD_RESET, LOW);
  delay(20);
  digitalWrite(PIN_TRACKPAD_RESET, HIGH);
  delay(50);
}

bool checkTrackpadAlive() {
  Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
  if (Wire1.endTransmission() != 0) return false;

  Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
  Wire1.write(REG_PRODUCT_ID);
  Wire1.endTransmission(false);

  Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
  return Wire1.available() > 0;
}

void setup() {
  //tft.init();
  //tft.fillScreen(COLOR_BLACK);
  analogWrite(5, 16);

  resetTrackpad();

  pinMode(PIN_TRACKPAD_MOTION, INPUT_PULLUP);
  Wire1.setSDA(PIN_TRACKPAD_SDA);
  Wire1.setSCL(PIN_TRACKPAD_SCL);
  Wire1.begin();

  if (!checkTrackpadAlive()) {
    //tft.drawString(10, 30, "STATUS: ERROR!", COLOR_RED, COLOR_BLACK, 1);
    while (true) {
      analogWrite(5, 16); delay(500);
      analogWrite(5, 1);  delay(500);
    }
  }
  Mouse.begin();
}

void loop() {
  if (digitalRead(PIN_TRACKPAD_MOTION) == LOW) {
    
    // 1. Đọc Motion Status
    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(REG_MOTION);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    uint8_t status = Wire1.available() ? Wire1.read() : 0;

    // 2. Đọc Delta X
    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(0x03);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    int8_t deltaX = Wire1.available() ? (int8_t)Wire1.read() : 0;

    // 3. Đọc Delta Y
    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(0x04);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    int8_t deltaY = Wire1.available() ? (int8_t)Wire1.read() : 0;

    // Lọc giá trị rác -32
    if (deltaX == -32 && deltaY == -32) {
      deltaX = 0;
      deltaY = 0;
    }

if (deltaX != 0 || deltaY != 0) {
      // Đảo ngược trục X bằng cách thêm dấu trừ (-deltaX)
      Mouse.move(-deltaX, deltaY);
      // Cập nhật lên LCD (Giữ nguyên hoặc đổi dấu tùy bạn muốn hiển thị)
    }
  }
}  