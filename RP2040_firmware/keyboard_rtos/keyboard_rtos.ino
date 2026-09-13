#include <Arduino.h>
#define __FREERTOS 1 
#include <FreeRTOS.h>

#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <timers.h>

#include <Keyboard.h>
#include <Mouse.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Wire.h>
#include "st7789_2.25.h"
#include "image_data.h"
#include"keyboard_MAT.h"
#include"HW_def_for_ver3.h"

// cần thêm cái switch sang gõ số ok
//thêm watchdog bq25895 ok
//thêm tab để câu hình bq25895

//mouse speed
#define MOUSE_SPEED 5

//phải có mutex bảo vệ mấy cái này
volatile float g_vbat = 0, g_vsys = 0, g_vbus = 0, g_ntcPct = 0;
volatile int g_ichg = 0, g_batPercent = 0;
volatile int g_iInLim = 0; 
volatile int g_iccLim = 0;
volatile uint8_t g_chgStat = 0; 
volatile uint8_t g_vbusStat = 0; 
volatile uint8_t g_faultStat = 0;

volatile bool isAlive = 0;

TaskHandle_t hTrackpadTask = NULL;
TaskHandle_t hKeypadTask   = NULL;
TaskHandle_t hPowerTask    = NULL;
TaskHandle_t hDisplayTask  = NULL;
TaskHandle_t hBackLightTask = NULL;

TFTDriver tft;

volatile uint8_t dashboard_page = 1;
volatile bool sym_detected = false;
volatile bool disp_ctrl = true;

void i2c_init(){
  Wire1.setSDA(I2C_SDA_PIN_bq);
  Wire1.setSCL(I2C_SCL_PIN_bq);
  Wire1.begin();

  Wire.setSDA(PIN_TRACKPAD_SDA);
  Wire.setSCL(PIN_TRACKPAD_SCL);
  Wire.begin();
}

uint8_t readBQRegister(uint8_t reg) {
  Wire1.beginTransmission(BQ25895_ADDR);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom((uint8_t)BQ25895_ADDR, (uint8_t)1);
  return Wire1.available() ? Wire1.read() : 0;
}

void writeBQRegister(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(BQ25895_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

void initBQ25895() {
  uint8_t reg02 = readBQRegister(0x02);
  reg02 |= 0xC0; 
  writeBQRegister(0x02, reg02);
}

bool check_bq25895_alive() {

  // 2. Gửi tín hiệu kiểm tra (ACK) đến địa chỉ BQ25895_ADDR (0x6A)
  Wire1.beginTransmission(BQ25895_ADDR);
  uint8_t error = Wire1.endTransmission();

  // Đơn vị phản hồi: error == 0 nghĩa là chip phản hồi thành công (ACK)
  if (error != 0) {
    return false; // Chip không phản hồi (chết hoặc hỏng đường truyền)
  }
  return true;
}

void resetBQ25895Watchdog() {
  uint8_t reg01 = readBQRegister(0x01);
  reg01 |= (1 << 6); // Set bit 6 (WATCHDOG_RESET) lên 1
  writeBQRegister(0x01, reg01);
}

// Khai báo handle cho Timer ở đầu file
TimerHandle_t xBacklightTimer = NULL;

// Callback hàm này sẽ TỰ ĐỘNG CHẠY khi HẾT 30 giây không bấm phím
void vBacklightTimerCallback(TimerHandle_t xTimer) {
    disp_ctrl = false;
    digitalWrite(backlight_control_pin, LOW); // Tắt đèn nền
}

void trackpadMotionISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Bắn tín hiệu Notification trực tiếp để đánh thức hTrackpadTask ngay lập tức
  if (hTrackpadTask != NULL) {
    vTaskNotifyGiveFromISR(hTrackpadTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void vTaskTrackpad(void *pvParameters) {
  for (;;) {
    // Task sẽ bị chặn (Block) hoàn toàn ở đây cho đến khi ISR gửi tín hiệu
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

      Wire.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire.write(REG_MOTION);
      Wire.endTransmission(false);
      Wire.requestFrom(TRACKPAD_I2C_ADDR, 1);
      if (Wire.available()) Wire.read();

      Wire.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire.write(0x03);
      Wire.endTransmission(false);
      Wire.requestFrom(TRACKPAD_I2C_ADDR, 1);
      int8_t deltaX = Wire.available() ? (int8_t)Wire.read() : 0;

      Wire.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire.write(0x04);
      Wire.endTransmission(false);
      Wire.requestFrom(TRACKPAD_I2C_ADDR, 1);
      int8_t deltaY = Wire.available() ? (int8_t)Wire.read() : 0;

      if (deltaX == -32 && deltaY == -32) { deltaX = 0; deltaY = 0; }

      bool isScrollPressed = (digitalRead(rowPins[2]) == LOW);
      if (deltaX != 0 || deltaY != 0) {
        if (isScrollPressed) {
          Mouse.move(0, 0, -deltaY / 4);
        } else {
          Mouse.move(-deltaX, deltaY);
        }
      }
    
  }
}

void vTaskPower(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {

      // Kiểm tra BQ Alive
      isAlive=check_bq25895_alive();

      if (isAlive) {
        resetBQ25895Watchdog();

        uint8_t reg00 = readBQRegister(0x00);
        g_iInLim = 100 + ((reg00 & 0x3F) * 50);

        uint8_t reg04 = readBQRegister(0x04);
        g_iccLim = (reg04 & 0x7F) * 64;

        uint8_t reg0E = readBQRegister(0x0E);
        g_vbat = 2.304 + ((reg0E & 0x7F) * 0.020);

        uint8_t reg0F = readBQRegister(0x0F);
        g_vsys = 2.304 + ((reg0F & 0x7F) * 0.020);

        uint8_t reg11 = readBQRegister(0x11);
        g_vbus = 2.600 + ((reg11 & 0x7F) * 0.100);

        uint8_t reg12 = readBQRegister(0x12);
        g_ichg = (reg12 & 0x7F) * 50;

        uint8_t reg10 = readBQRegister(0x10);
        g_ntcPct = 21.0 + ((reg10 & 0x7F) * 0.465);

        uint8_t reg0B = readBQRegister(0x0B);
        g_vbusStat = (reg0B >> 5) & 0x07;
        g_chgStat  = (reg0B >> 3) & 0x03;

        g_faultStat = readBQRegister(0x0C);

      }


    // Nhờ vTaskDelayUntil, Task sẽ chạy ĐÚNG 1000ms một lần chuẩn xác
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
}

void vTaskKeypad(void *pvParameters) {
  bool lastState[7][7] = {false};

  for (;;) {
    for (int c = 0; c < 7; c++) {
      digitalWrite(colPins[c], LOW);
      delayMicroseconds(5);

      for (int r = 0; r < 7; r++) {
        bool isPressed = (digitalRead(rowPins[r]) == LOW);

        if (isPressed && !lastState[r][c]) {

          // --- LOGIC ĐÈN NỀN MỚI ---
          // 1. Nếu màn hình đang tắt, bật lại ngay
          if (!disp_ctrl) {
            disp_ctrl = true;
            digitalWrite(backlight_control_pin, HIGH);
          }
          // 2. Restart/Reset lại bộ đếm 30s của Timer
          if (xBacklightTimer != NULL) {
            xTimerReset(xBacklightTimer, 0);
          }
          // --------------------------

          if (r == 2 && c == 1) {
            dashboard_page = (dashboard_page >= 2) ? 1 : dashboard_page + 1;
          }
          if (r == 5 && c == 4) sym_detected = !sym_detected;

          uint16_t keycode = pgm_read_word(sym_detected?(&(key_Mat[r][c])):(&(key_Mat_num[r][c])));
          if (keycode != 0xFFFF) {
            if (keycode == 0xE001) Mouse.press(MOUSE_LEFT);
            else if (keycode == 0xE002) Mouse.press(MOUSE_RIGHT);
            else Keyboard.press((uint8_t)keycode);
          }
          lastState[r][c] = true;
        } 
        else if (!isPressed && lastState[r][c]) {
          uint16_t keycode = pgm_read_word(sym_detected?(&(key_Mat[r][c])):(&(key_Mat_num[r][c])));
          if (keycode != 0xFFFF) {
            if (keycode == 0xE001) Mouse.release(MOUSE_LEFT);
            else if (keycode == 0xE002) Mouse.release(MOUSE_RIGHT);
            else Keyboard.release((uint8_t)keycode);
          }
          lastState[r][c] = false;
        }
      }
      digitalWrite(colPins[c], HIGH);
    }
    // Quét phím xong nghỉ 2ms để chống rung phím và nhường CPU
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void drawDashboardLayout1() {
  tft.fillScreen(BG_DARK);
  tft.fillRect(4, 6, 79, 64, CARD_BG);
  tft.fillRect(87, 6, 93, 64, CARD_BG);
  tft.drawString(93, 10, "POWER VOLT", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(93, 24, "VBAT: ", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(93, 38, "VBUS: ", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(93, 52, "VSYS: ", TEXT_GRAY, CARD_BG, 1);

  tft.fillRect(184, 6, 96, 64, CARD_BG);
  tft.drawString(190, 25, "ICHG:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(190, 38, "NTC :", TEXT_GRAY, CARD_BG, 1);

  tft.fillRect(190, 10, 84, 12, CARD_BG);
  tft.drawString(190, 52, "ILIM:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(190, 10, "BQ25895:", TEXT_GRAY, CARD_BG, 1);
  // thêm 1 cái để báo trạng thái phím chữ hay số.
}

void drawDashboardLayout2() {
  //nhiệm vụ của bạn
}

void vTaskDisplay(void *pvParameters) {
  uint8_t lastPage = 255; // Khởi tạo trang cũ khác biệt để vẽ layout lần đầu
  uint8_t currentFrame = 0;

  for (;;) {
    // 1. Chờ dữ liệu Pin tối đa 50ms (Đóng vai trò thay thế vTaskDelay cho toàn bộ Task)
    // Nếu có data mới -> Cập nhật pData. Nếu không có data -> Hết 50ms tự chạy tiếp code phía dưới

    // 2. Xử lý UI Trang 1
    if (dashboard_page == 1) {

      // Đổi trang: Chỉ vẽ lại khung nền layout khi vừa chuyển sang Page 1
      if (lastPage != 1) {
        lastPage = 1;
        drawDashboardLayout1();
      }

      // Cập nhật số liệu chữ lên TFT (Tần số quét 50ms rất mượt)
      char strBuf[16];

    snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vbat);
    tft.drawString(122, 24, strBuf, ACCENT_CYAN, CARD_BG, 1);

    snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vbus);
    tft.drawString(122, 38, strBuf, TEXT_WHITE, CARD_BG, 1);

    snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vsys);
    tft.drawString(122, 52, strBuf, TEXT_WHITE, CARD_BG, 1);

    snprintf(strBuf, sizeof(strBuf), "%dmA", g_ichg);
    tft.drawString(225, 25, strBuf, ACCENT_CYAN, CARD_BG, 1);

    snprintf(strBuf, sizeof(strBuf), "%.0f%%", g_ntcPct);
    tft.drawString(225, 38, strBuf, ACCENT_GREEN, CARD_BG, 1);

    // --- HIỂN THỊ ILIM THAY CHO THANH % PIN ---
    snprintf(strBuf, sizeof(strBuf), "%dmA ", g_iInLim); // Thêm khoảng trắng cuối để xóa ký tự dư
    tft.drawString(225, 52, strBuf, ACCENT_CYAN, CARD_BG, 1);

      if (isAlive) {
        tft.drawString(240, 10, "Alive", ACCENT_GREEN, CARD_BG, 1);
      } else {
        tft.drawString(240, 10, "Dead   ", ACCENT_RED, CARD_BG, 1);
      }

      // Xử lý Animation Ảnh dạng State Machine (KHÔNG DÙNG VÒNG LẶP FOR!)
      // Mỗi vòng lặp Task (~50ms) chỉ vẽ đúng 1 Frame ảnh rồi nhả CPU ngay
      if (TOTAL_IMAGES > 0) {
        const uint16_t* current_img = (const uint16_t*) pgm_read_ptr(&(all_images[currentFrame]));
        tft.pushImage(8, 6, IMAGE_WIDTH, IMAGE_HEIGHT, current_img);

        // Tăng frame cho lần lặp sau
        currentFrame = (currentFrame + 1) % TOTAL_IMAGES;
      }

    } 
    // 3. Xử lý UI các Trang khác (Trang 2, Trang 3...)
    else if(dashboard_page == 2) {
      // RESET lại trang để khi quay về Page 1 sẽ tự vẽ lại layout
      if (lastPage == 1) {
        lastPage = dashboard_page;
        currentFrame = 0;
      }
      
      // Viết code vẽ các Page khác ở đây (nhiệm vụ của bạn)

    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Keyboard.begin();
  Mouse.begin();

  pinMode(PIN_TRACKPAD_MOTION, INPUT_PULLUP);
  pinMode(backlight_control_pin, OUTPUT);      

  if(disp_ctrl){
    digitalWrite(backlight_control_pin, HIGH);
  }   // Xuất tín hiệu mức cao (3.3V)
  else{digitalWrite(backlight_control_pin, LOW);}

  // Pin Config
  for (int i = 0; i < 7; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for (int j = 0; j < 7; j++) {
    pinMode(colPins[j], OUTPUT);
    digitalWrite(colPins[j], HIGH);
  }
  pinMode(PIN_TRACKPAD_MOTION, INPUT_PULLUP);

  i2c_init();
  initBQ25895();

  attachInterrupt(digitalPinToInterrupt(PIN_TRACKPAD_MOTION), trackpadMotionISR, FALLING);

// --- TẠO SOFTWARE TIMER 30 GIÂY ---
  // pdFALSE = One-shot timer (chỉ chạy 1 lần khi kích hoạt)
  xBacklightTimer = xTimerCreate("BacklightTimer", 
                                 pdMS_TO_TICKS(30000), 
                                 pdFALSE, 
                                 (void *)0, 
                                 vBacklightTimerCallback);

  if (xBacklightTimer != NULL) {
    xTimerStart(xBacklightTimer, 0); // Kích hoạt timer ngay từ đầu
  }

// Tạo Task gắn cố định vào CORE 0 (Dùng (1 << 0))
xTaskCreateAffinitySet(vTaskTrackpad, "TrackpadTask", 2048, NULL, 4, (1 << 0), &hTrackpadTask);
xTaskCreateAffinitySet(vTaskKeypad,   "KeypadTask",   2048, NULL, 3, (1 << 0), &hKeypadTask);
xTaskCreateAffinitySet(vTaskPower,    "PowerTask",    2048, NULL, 1, (1 << 0), &hPowerTask);

}

void loop() {
  // Khi chạy RTOS, hàm loop() được bỏ trống
  vTaskDelete(NULL); 
}

void setup1() {
  tft.init();
  analogWrite(5, 16);
  drawDashboardLayout1();
  // Tạo Task gắn cố định vào CORE 1 (Dùng (1 << 1))
  xTaskCreateAffinitySet(vTaskDisplay,  "DisplayTask",  4096, NULL, 2, (1 << 1), &hDisplayTask);
}

void loop1() {
  // Khi chạy RTOS, hàm loop() được bỏ trống
  vTaskDelete(NULL); 
}

//scp "keyboard_rtos.ino.uf2" pi@192.168.137.87:/media/pi/RPI-RP2/
