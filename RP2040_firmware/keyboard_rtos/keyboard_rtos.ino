#include <Arduino.h>
#define __FREERTOS 1 
#include <FreeRTOS.h>

#include <task.h>
#include <semphr.h>
#include <queue.h>

#include <Keyboard.h>
#include <Mouse.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Wire.h>
#include "st7789_2.25.h"
#include "image_data.h"

//mouse speed
#define MOUSE_SPEED 5

//this is not beginer friendly, so this just for legacy 
/*
//hardware pin for each version is not alike, if you are using ver3.0, add 
#ifdef USING_ver3
    #include"HW_def_for_ver3.h"
#else
    #include"HW_def_for_ver2.h"
#endif
*/

//for everyone :)
#include"keyboard_MAT.h"
#include"HW_def_for_ver2.h"

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

SemaphoreHandle_t i2cMutex;

TFTDriver tft;

volatile uint8_t dashboard_page = 1;
volatile bool sym_detected = false;
volatile bool disp_ctrl = true;

void selectI2CPins(uint8_t sda, uint8_t scl) {
  Wire1.end();
  Wire1.setSDA(sda);
  Wire1.setSCL(scl);
  Wire1.begin();
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
  // 1. Chuyển bus Wire1 sang các chân I2C của BQ25895
  selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);

  // 2. Gửi tín hiệu kiểm tra (ACK) đến địa chỉ BQ25895_ADDR (0x6A)
  Wire1.beginTransmission(BQ25895_ADDR);
  uint8_t error = Wire1.endTransmission();

  // Đơn vị phản hồi: error == 0 nghĩa là chip phản hồi thành công (ACK)
  if (error != 0) {
    return false; // Chip không phản hồi (chết hoặc hỏng đường truyền)
  }
  return true;
}

void trackpadMotionISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Bắn tín hiệu Notification trực tiếp để đánh thức hTrackpadTask ngay lập tức
  if (hTrackpadTask != NULL) {
    vTaskNotifyGiveFromISR(hTrackpadTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void backlightBtnISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Gửi tín hiệu đánh thức vTaskBackLightControl
  if (hBackLightTask != NULL){
    vTaskNotifyGiveFromISR(hBackLightTask, &xHigherPriorityTaskWoken);
    // Chuyển ngữ cảnh ngay nếu Task backlight có độ ưu tiên cao
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void vTaskTrackpad(void *pvParameters) {
  for (;;) {
    // Task sẽ bị chặn (Block) hoàn toàn ở đây cho đến khi ISR gửi tín hiệu
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Lấy quyền sử dụng I2C Bus
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      selectI2CPins(PIN_TRACKPAD_SDA, PIN_TRACKPAD_SCL);

      Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire1.write(REG_MOTION);
      Wire1.endTransmission(false);
      Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
      if (Wire1.available()) Wire1.read();

      Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire1.write(0x03);
      Wire1.endTransmission(false);
      Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
      int8_t deltaX = Wire1.available() ? (int8_t)Wire1.read() : 0;

      Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
      Wire1.write(0x04);
      Wire1.endTransmission(false);
      Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
      int8_t deltaY = Wire1.available() ? (int8_t)Wire1.read() : 0;

      xSemaphoreGive(i2cMutex); // Nhả bus I2C

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
}

void vTaskPower(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);

      // Kiểm tra BQ Alive
      isAlive=check_bq25895_alive();

      if (isAlive) {
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
      xSemaphoreGive(i2cMutex);

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
          if (r == 2 && c == 1) {
            dashboard_page = (dashboard_page >= 2) ? 1 : dashboard_page + 1;
          }
          if (r == 5 && c == 4) sym_detected = !sym_detected;

          uint16_t keycode = pgm_read_word(&(key_Mat[r][c]));
          if (keycode != 0xFFFF) {
            if (keycode == 0xE001) Mouse.press(MOUSE_LEFT);
            else if (keycode == 0xE002) Mouse.press(MOUSE_RIGHT);
            else Keyboard.press((uint8_t)keycode);
          }
          lastState[r][c] = true;
        } 
        else if (!isPressed && lastState[r][c]) {
          uint16_t keycode = pgm_read_word(&(key_Mat[r][c]));
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
    else {
      // RESET lại trang để khi quay về Page 1 sẽ tự vẽ lại layout
      if (lastPage == 1) {
        lastPage = dashboard_page;
        currentFrame = 0;
      }
      
      // Viết code vẽ các Page khác ở đây...
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void vTaskBackLightControl(void *pvParameters) {
  for (;;) {
    // Task DỪNG LẠI VÀ NGỦ HOÀN TOÀN ở đây cho đến khi có ngắt bấm nút
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Khi ngón tay bấm nút, Task tỉnh dậy ở dòng này:
    disp_ctrl = !disp_ctrl;                   // Đảo trạng thái hiển thị
    digitalWrite(bl_ctrl_pin, disp_ctrl ? HIGH : LOW);  // Điều khiển GPIO 6

    // Chống rung phím (Debounce):
    // Nhường CPU 200ms để bỏ qua các xung nhiễu cơ học khi bấm nút
    vTaskDelay(pdMS_TO_TICKS(200));

    // Xóa cờ ngắt thừa (nếu có) tích tụ trong lúc delay chống rung
    xTaskNotifyStateClear(NULL);
  }
}

void setup() {
  Keyboard.begin();
  Mouse.begin();

  pinMode(PIN_TRACKPAD_MOTION, INPUT_PULLUP);

  pinMode(Disp_ctrl_button, INPUT);
  pinMode(bl_ctrl_pin, OUTPUT);      // Cấu hình chân GPIO 6 là đầu ra

  if(disp_ctrl){
    digitalWrite(bl_ctrl_pin, HIGH);
  }   // Xuất tín hiệu mức cao (3.3V)
  else{digitalWrite(bl_ctrl_pin, LOW);}

  // Pin Config
  for (int i = 0; i < 7; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for (int j = 0; j < 7; j++) {
    pinMode(colPins[j], OUTPUT);
    digitalWrite(colPins[j], HIGH);
  }
  pinMode(PIN_TRACKPAD_MOTION, INPUT_PULLUP);

  i2cMutex = xSemaphoreCreateMutex();

  selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);
  initBQ25895();

  attachInterrupt(digitalPinToInterrupt(PIN_TRACKPAD_MOTION), trackpadMotionISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(Disp_ctrl_button), backlightBtnISR, FALLING);

// Tạo Task gắn cố định vào CORE 0 (Dùng (1 << 0))
xTaskCreateAffinitySet(vTaskTrackpad, "TrackpadTask", 2048, NULL, 4, (1 << 0), &hTrackpadTask);
xTaskCreateAffinitySet(vTaskKeypad,   "KeypadTask",   2048, NULL, 3, (1 << 0), &hKeypadTask);
xTaskCreateAffinitySet(vTaskPower,    "PowerTask",    2048, NULL, 1, (1 << 0), &hPowerTask);
xTaskCreateAffinitySet(vTaskBackLightControl, "BackLightTask", 1024, NULL, 1, (1 << 0), &hBackLightTask);

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
















/*

#include <Keyboard.h>
#include <Mouse.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <SPI.h>
#include "st7789_2.25.h"
#include "image_data.h"
#include <Wire.h>

#define MOUSE_SPEED 5
// lưu ý đây là màn hình có độ phân giải là 284x76
// =========================================================================
// PHÍM VÀ CẤU HÌNH PIN
// =========================================================================
// về kiến trúc: 2 lõi, lõi 1 đảm nhiệm quét phím liên tục+đọc i2c bq để cập nhật biến pin, 
// đọc i2c trackpad mỗi loop, 1 lõi chỉ đảm nhận việc vẽ các thông tin thu được lên màn hình

//////////////////////////////----for keyboard----/////////////////////////////////
#define KC_A        (uint16_t)'a'
#define KC_B        (uint16_t)'b'
#define KC_C        (uint16_t)'c'
#define KC_D        (uint16_t)'d'
#define KC_E        (uint16_t)'e'
#define KC_F        (uint16_t)'f'
#define KC_G        (uint16_t)'g'
#define KC_H        (uint16_t)'h'
#define KC_I        (uint16_t)'i'
#define KC_J        (uint16_t)'j'
#define KC_K        (uint16_t)'k'
#define KC_L        (uint16_t)'l'
#define KC_M        (uint16_t)'m'
#define KC_N        (uint16_t)'n'
#define KC_O        (uint16_t)'o'
#define KC_P        (uint16_t)'p'
#define KC_Q        (uint16_t)'q'
#define KC_R        (uint16_t)'r'
#define KC_S        (uint16_t)'s'
#define KC_T        (uint16_t)'t'
#define KC_U        (uint16_t)'u'
#define KC_V        (uint16_t)'v'
#define KC_W        (uint16_t)'w'
#define KC_X        (uint16_t)'x'
#define KC_Y        (uint16_t)'y'
#define KC_Z        (uint16_t)'z'
#define KC_SPC      (uint16_t)' '
#define KC_ENT      (uint16_t)KEY_RETURN
#define KC_BSPC     (uint16_t)KEY_BACKSPACE
#define KC_TAB      (uint16_t)KEY_TAB
#define KC_CAPS     (uint16_t)KEY_CAPS_LOCK
#define KC_LCTRL    (uint16_t)KEY_LEFT_CTRL
#define KC_LSHFT    (uint16_t)KEY_LEFT_SHIFT
#define KC_LALT     (uint16_t)KEY_LEFT_ALT
#define KC_LCLICK   (uint16_t)0xE001
#define KC_RCLICK   (uint16_t)0xE002
#define KC_LGUI     (uint16_t)KEY_LEFT_GUI
#define KC_NO       (uint16_t)0xFFFF
#define KC_1        (uint16_t)'1'
#define KC_2        (uint16_t)'2'
#define KC_3        (uint16_t)'3'
#define KC_4        (uint16_t)'4'
#define KC_5        (uint16_t)'5'
#define KC_6        (uint16_t)'6'
#define KC_7        (uint16_t)'7'
#define KC_8        (uint16_t)'8'
#define KC_9        (uint16_t)'9'
#define KC_0        (uint16_t)'0'

#define KC_SLSH     (uint16_t)'/'
#define KC_COLN     (uint16_t)':'
#define KC_SCLN     (uint16_t)';'
#define KC_QUOT     (uint16_t)'\''
#define KC_DQUO     (uint16_t)'\"'
#define KC_QUES     (uint16_t)'?'
#define KC_EXLM     (uint16_t)'!'
#define KC_COMM     (uint16_t)','
#define KC_DOT      (uint16_t)'.'
#define KC_ASTR     (uint16_t)'*'
#define KC_HASH     (uint16_t)'#'
//////////////////////////////----for keyboard----/////////////////////////////////

//////////////////////////////////----for BQ25895----//////////////////////////////////
#define I2C_SDA_PIN_bq   14
#define I2C_SCL_PIN_bq   15
#define BQ25895_ADDR     0x6A
//////////////////////////////////----for BQ25895----//////////////////////////////////

//////////////////////////////////----for trackpad----//////////////////////////////////
#define PIN_TRACKPAD_RESET  16
#define PIN_TRACKPAD_SDA    18
#define PIN_TRACKPAD_MOTION 22
#define PIN_TRACKPAD_SCL    23

#define TRACKPAD_I2C_ADDR   0x3B
#define REG_PRODUCT_ID      0x00
#define REG_MOTION          0x02
//////////////////////////////////----for trackpad----//////////////////////////////////

//////////////////////////////////----for color----//////////////////////////////////
#define BG_DARK       0x0842
#define CARD_BG       0x18C5
#define TEXT_WHITE    0xFFFF
#define TEXT_GRAY     0x8410
#define ACCENT_CYAN   0x07FF
#define ACCENT_GREEN  0x07E0
#define ACCENT_RED    0xF800
#define ACCENT_YELLOW 0xFDE0
#define HUD_BG        0x0000 // Nền đen tuyền tạo tương phản cao
#define HUD_GRID      0x2104 // Xám tối cho vạch phân cách
#define HUD_CYAN      0x07FF // Băng lam (Primary Accent)
#define HUD_GREEN     0x3E00 // Xanh neon
#define HUD_ORANGE    0xFD20 // Cam cảnh báo
#define HUD_GRAY      0x6B55 // Xám chữ phụ
#define COLOR_CYBER_PINK   0xF81F  // Magenta / Neon Pink
#define COLOR_CYBER_CYAN   0x07FF  // Bright Cyan
#define COLOR_CYBER_YELLOW 0xFFE0  // Bright Yellow
#define COLOR_CYBER_GREEN  0x07E0  // Neon Green
#define COLOR_CYBER_PURPLE 0x981F  // Purple
#define COLOR_DARK_GRID    0x18E3  // Dark Slate Gray cho các vạch ẩn
#define COLOR_WHITE    0xFFFF
#define COLOR_RED 0x07E0
//////////////////////////////////----for color----//////////////////////////////////

#define Disp_ctrl_button 7

//////////////////////////////----display object oriented----//////////////////////////////
TFTDriver tft;
//////////////////////////////----display object oriented----//////////////////////////////

const uint16_t Mat[7][7] PROGMEM = {
  { KC_LCLICK,  KC_W,     KC_G,     KC_S,     KC_L,     KC_H,     KC_NO  },
  { KC_NO,      KC_Q,     KC_R,     KC_E,     KC_O,     KC_U,     KC_NO  },
  { KC_NO,      KC_NO,    KC_F,     KC_CAPS,  KC_K,     KC_J,     KC_NO  },
  { KC_NO,      KC_SPC,   KC_C,     KC_Z,     KC_M,     KC_N,     KC_NO  },
  { KC_LGUI,    KC_LCTRL, KC_T,     KC_D,     KC_I,     KC_Y,     KC_NO  },
  { KC_RCLICK,  KC_LALT,  KC_V,     KC_X,     KC_LSHFT, KC_B,     KC_NO  },
  { KC_NO,      KC_A,     KC_NO,    KC_P,     KC_BSPC,  KC_ENT,   KC_NO  }
};

//number, shift, ctrl, fn, tab,... matrix
const uint16_t Mat_num[7][7] PROGMEM = {
  { KC_LCLICK,  KC_1,       KC_SLSH,    KC_4,     KC_L,     KC_COLN,  KC_NO  },
  { KC_NO,      KC_HASH,    KC_3,       KC_2,     KC_O,     KC_U,     KC_NO  },
  { KC_NO,      KC_NO,      KC_6,       KC_CAPS,  KC_K,     KC_SCLN,  KC_NO  },
  { KC_NO,      KC_SPC,     KC_9,       KC_7,     KC_DOT,   KC_COMM,  KC_NO  },
  { KC_LGUI,    KC_LCTRL,   KC_T,       KC_5,     KC_I,     KC_Y,     KC_NO  },
  { KC_RCLICK,  KC_LALT,    KC_QUES,    KC_8,     KC_LSHFT, KC_EXLM,  KC_NO  },
  { KC_NO,      KC_ASTR,    KC_NO,      KC_P,     KC_BSPC,  KC_ENT,   KC_NO  }
};

const int rowPins[7] = {27, 26, 25, 24, 21, 20, 19}; 
const int colPins[7] = {8, 9, 10, 11, 12, 13, 17};
bool lastState[7][7] = {false};
bool is_bq25895_alive=false;

uint8_t dashboard_page=1; //current dashboard
uint8_t max_dashboard_page=2; //maximun number of dashboard panel
bool disp_ctrl = true;

static unsigned long lastDispBtnTime = 0;
static bool lastDispBtnState = HIGH;
const unsigned long DEBOUNCE_DELAY = 200;

bool sym_detected = false;

volatile float g_vbat = 0, g_vsys = 0, g_vbus = 0, g_ntcPct = 0;
volatile int g_ichg = 0, g_batPercent = 0;
volatile int g_iInLim = 0; 
volatile int g_iccLim = 0;
volatile uint8_t g_chgStat = 0; 
volatile uint8_t g_vbusStat = 0; 
volatile uint8_t g_faultStat = 0;

// I2C selector
void selectI2CPins(uint8_t sda, uint8_t scl) {
  Wire1.end();
  Wire1.setSDA(sda);
  Wire1.setSCL(scl);
  Wire1.begin();
}

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

// Hàm kiểm tra IC sạc BQ25895 có phản hồi trên bus I2C hay không
bool check_bq25895_alive() {
  // 1. Chuyển bus Wire1 sang các chân I2C của BQ25895
  //selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);

  // 2. Gửi tín hiệu kiểm tra (ACK) đến địa chỉ BQ25895_ADDR (0x6A)
  Wire1.beginTransmission(BQ25895_ADDR);
  uint8_t error = Wire1.endTransmission();

  // Đơn vị phản hồi: error == 0 nghĩa là chip phản hồi thành công (ACK)
  if (error != 0) {
    return false; // Chip không phản hồi (chết hoặc hỏng đường truyền)
  }
  return true;
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

// =========================================================================
// SETUP CORE 0 (XỬ LÝ PHÍM & I2C)
// =========================================================================
void setup() {
  pinMode(Disp_ctrl_button, INPUT);
  pinMode(6, OUTPUT);      // Cấu hình chân GPIO 6 là đầu ra
  if(disp_ctrl){digitalWrite(6, HIGH);}   // Xuất tín hiệu mức cao (3.3V)
  else{digitalWrite(6, LOW);}
  Keyboard.begin();
  Mouse.begin();
  for (int i = 0; i < 7; i++) pinMode(rowPins[i], INPUT_PULLUP);
  for (int j = 0; j < 7; j++) {
    pinMode(colPins[j], OUTPUT);
    digitalWrite(colPins[j], HIGH);
  }



  // RESET VÀ KHỞI TẠO TRACKPAD ĐÚNG 1 LẦN TRONG SETUP
  selectI2CPins(PIN_TRACKPAD_SDA, PIN_TRACKPAD_SCL);
  resetTrackpad();

  if (!checkTrackpadAlive()) {
    while (true) {
      analogWrite(5, 16); delay(300);
      analogWrite(5, 1);  delay(300);
    }
  }

  // KHỞI TẠO BQ25895 ĐÚNG 1 LẦN
  selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);
  initBQ25895();

}

unsigned long lastBatteryCheck = 0;

// =========================================================================
// LOOP CORE 0
// =========================================================================

// Hàm bóc tách bit của REG0C và trả về chuỗi ngắn gọn (tối đa 7 ký tự)
const char* getBQFaultText(uint8_t fault) {
  if (fault == 0) return "OK     ";

  // 1. Lỗi Watchdog Timer (Bit 7)
  if (fault & 0x80) return "WD TMR ";

  // 2. Lỗi Boost / OTG (Bit 6)
  if (fault & 0x40) return "BST FLT";

  // 3. Lỗi Pin quá áp BVOVP (Bit 3)
  if (fault & 0x08) return "BAT OVP";

  // 4. Lỗi Quá trình Sạc (Bit 5:4)
  uint8_t chgFault = (fault >> 4) & 0x03;
  if (chgFault == 1) return "VBUS Err"; // Nguồn vào quá áp/yếu
  if (chgFault == 2) return "IC HOT! "; // IC sạc quá nhiệt (>145°C)
  if (chgFault == 3) return "TMR EXP "; // Sạc quá thời gian an toàn

  // 5. Lỗi Nhiệt độ Pin qua điện trở NTC (Bit 2:0)
  uint8_t ntcFault = fault & 0x07;
  switch (ntcFault) {
    case 1: return "BAT COLD"; // Pin quá lạnh khi sạc
    case 2: return "BAT HOT "; // Pin quá nóng khi sạc
    case 3: return "BST COLD"; // Pin quá lạnh khi phát OTG
    case 4: return "BST HOT "; // Pin quá nóng khi phát OTG
    case 5: return "BAT WARM"; // Pin hơi ấm (Cảnh báo)
    case 6: return "BAT COOL"; // Pin hơi mát (Cảnh báo)
  }

  return "UNKNOWN";
}

void loop() {
bool currentDispBtnState = digitalRead(Disp_ctrl_button);

  // Kiểm tra cạnh xuống: Nút chuyển từ HIGH (nhả) -> LOW (nhấn) và đã qua thời gian debounce
  if (lastDispBtnState == HIGH && currentDispBtnState == LOW && (millis() - lastDispBtnTime > DEBOUNCE_DELAY)) {
    disp_ctrl = !disp_ctrl;         // Đảo trạng thái hiển thị
    lastDispBtnTime = millis();     // Cập nhật mốc thời gian nhấn gần nhất
  }
  lastDispBtnState = currentDispBtnState; // Lưu lại trạng thái để so sánh cho vòng lap sau

  // Điều khiển chân GPIO 6 dựa trên disp_ctrl
  digitalWrite(6, disp_ctrl ? HIGH : LOW);
  
  // 1. ĐỌC TRACKPAD
  if (digitalRead(PIN_TRACKPAD_MOTION) == LOW) {
    digitalWrite(colPins[0], LOW);
    selectI2CPins(PIN_TRACKPAD_SDA, PIN_TRACKPAD_SCL);

    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(REG_MOTION);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    uint8_t status = Wire1.available() ? Wire1.read() : 0;

    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(0x03);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    int8_t deltaX = Wire1.available() ? (int8_t)Wire1.read() : 0;

    Wire1.beginTransmission(TRACKPAD_I2C_ADDR);
    Wire1.write(0x04);
    Wire1.endTransmission(false);
    Wire1.requestFrom(TRACKPAD_I2C_ADDR, 1);
    int8_t deltaY = Wire1.available() ? (int8_t)Wire1.read() : 0;

    if (deltaX == -32 && deltaY == -32) { deltaX = 0; deltaY = 0; }
    
    bool isScrollPressed = (digitalRead(rowPins[2]) == LOW);

      if (deltaX != 0 || deltaY != 0) {
          if(isScrollPressed){
                int8_t scrollSpeed = -deltaY/4;
                Mouse.move(0, 0, scrollSpeed);
              }
                else{Mouse.move(-deltaX, deltaY);}
              }
  digitalWrite(colPins[0], HIGH);
}

  // 2. QUÉT MA TRẬN PHÍM
  for (int c = 0; c < 7; c++) {
    digitalWrite(colPins[c], LOW);
    delayMicroseconds(5); 

    for (int r = 0; r < 7; r++) {
      bool isPressed = (digitalRead(rowPins[r]) == LOW);

      if (isPressed && !lastState[r][c]) {
        if(r==2&&c==1){if(dashboard_page>=max_dashboard_page){dashboard_page=1;}
          else{dashboard_page++;}
        }

        if(r==5&&c==4){sym_detected=!sym_detected;}
        uint16_t keycode;
                if(sym_detected==false){keycode = pgm_read_word(&(Mat[r][c]));}
                else{keycode = pgm_read_word(&(Mat_num[r][c]));}
        if (keycode != KC_NO) {
          if (keycode == KC_LCLICK) Mouse.press(MOUSE_LEFT);
          else if (keycode == KC_RCLICK) Mouse.press(MOUSE_RIGHT);
          else Keyboard.press((uint8_t)keycode);
        }
        lastState[r][c] = true;
      } 
      else if (!isPressed && lastState[r][c]) {
        //if(r==2&&c==1){dashboard_page=1;}
        uint16_t keycode;
                if(sym_detected==false){keycode = pgm_read_word(&(Mat[r][c]));}
                else{keycode = pgm_read_word(&(Mat_num[r][c]));}
        if (keycode != KC_NO) {
          if (keycode == KC_LCLICK) Mouse.release(MOUSE_LEFT);
          else if (keycode == KC_RCLICK) Mouse.release(MOUSE_RIGHT);
          else Keyboard.release((uint8_t)keycode);
        }
        lastState[r][c] = false;
      }
    }
    digitalWrite(colPins[c], HIGH);
  }

  // 3. ĐỌC THÔNG SỐ BQ25895 MỖI 1 GIÂY (KHÔNG LÀM CHẬM MÁY)
  if (millis() - lastBatteryCheck > 1000) {
    lastBatteryCheck = millis();
    selectI2CPins(I2C_SDA_PIN_bq, I2C_SCL_PIN_bq);

    is_bq25895_alive=check_bq25895_alive();

// REG00: Input Current Limit (ILIM)
    uint8_t reg00 = readBQRegister(0x00);
    g_iInLim = 100 + ((reg00 & 0x3F) * 50);

    // REG04: Fast Charge Current Limit (ICHG Cài đặt)
    uint8_t reg04 = readBQRegister(0x04);
    g_iccLim = (reg04 & 0x7F) * 64;

    // REG0E, 0F, 11: Điện áp
    uint8_t reg0E = readBQRegister(0x0E);
    g_vbat = 2.304 + ((reg0E & 0x7F) * 0.020);

    uint8_t reg0F = readBQRegister(0x0F);
    g_vsys = 2.304 + ((reg0F & 0x7F) * 0.020);

    uint8_t reg11 = readBQRegister(0x11);
    g_vbus = 2.600 + ((reg11 & 0x7F) * 0.100);

    // REG12: Dòng sạc đo thực tế
    uint8_t reg12 = readBQRegister(0x12);
    g_ichg = (reg12 & 0x7F) * 50;

    // REG10: NTC TS Percent
    uint8_t reg10 = readBQRegister(0x10);
    g_ntcPct = 21.0 + ((reg10 & 0x7F) * 0.465);

    // REG0B: Status Register (VBUS & Charge Status)
    uint8_t reg0B = readBQRegister(0x0B);
    g_vbusStat = (reg0B >> 5) & 0x07;
    g_chgStat  = (reg0B >> 3) & 0x03;

    // REG0C: Fault Register
    g_faultStat = readBQRegister(0x0C);

    g_batPercent = (int)((g_vbat - 3.3) / (4.2 - 3.3) * 100.0);
    g_batPercent = constrain(g_batPercent, 0, 100);


  }

  delay(2);
}

// =========================================================================
// SETUP & LOOP CORE 1 (CHUYÊN VẼ MÀN HÌNH TFT)
// =========================================================================

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
}


void drawDashboardLayout2() {
  tft.fillScreen(BG_DARK);
  
  // -----------------------------------------------------------------------
  // Card 1: BẢNG ĐIỆN ÁP (Tọa độ X: 4 -> 92 | Rộng 88px, Cao 68px)
  // -----------------------------------------------------------------------
  tft.fillRect(4, 4, 88, 68, CARD_BG);
  tft.drawString(8, 8,   "VOLTAGE", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(8, 22,  "VBAT:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(8, 35,  "VBUS:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(8, 48,  "VSYS:", TEXT_GRAY, CARD_BG, 1);

  // -----------------------------------------------------------------------
  // Card 2: DÒNG ĐIỆN & NHIỆT ĐỘ (Tọa độ X: 96 -> 188 | Rộng 92px, Cao 68px)
  // -----------------------------------------------------------------------
  tft.fillRect(96, 4, 92, 68, CARD_BG);
  tft.drawString(100, 8,  "CURRENT/NTC", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(100, 22, "ICHG:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(100, 35, "ILIM:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(100, 48, "NTC :", TEXT_GRAY, CARD_BG, 1);

  // -----------------------------------------------------------------------
  // Card 3: CHẨN ĐOÁN BQ25895 (Tọa độ X: 192 -> 280 | Rộng 88px, Cao 68px)
  // -----------------------------------------------------------------------
  tft.fillRect(192, 4, 88, 68, CARD_BG);
  tft.drawString(196, 8,  "DIAGNOSTIC", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(196, 22, "STAT:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(196, 35, "VBUS:", TEXT_GRAY, CARD_BG, 1);
  tft.drawString(196, 48, "FLT :", TEXT_GRAY, CARD_BG, 1);


}

uint8_t dashboard_page_latest=1;
void setup1() {
  Serial.begin(115200);
  tft.init();
  analogWrite(5, 16);

  drawDashboardLayout1();
}

unsigned long lastTftUpdate = 0;

void loop1() {
  // Update số liệu trên màn hình mỗi 500ms
if(dashboard_page==1){
  if(dashboard_page_latest!=1){
    dashboard_page_latest=1;
    drawDashboardLayout1();
  }
  if (millis() - lastTftUpdate > 500) {
    lastTftUpdate = millis();

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

    if(is_bq25895_alive){
    tft.drawString(240, 10, "Alive", ACCENT_GREEN, CARD_BG, 1); }
    
    else{tft.drawString(240, 10, "Dead   ", ACCENT_RED, CARD_BG, 1); }
    }
  

  // Vẽ ảnh GIF/Animation
  for (int i = 0; i < TOTAL_IMAGES; i++) {
    const uint16_t* current_img = (const uint16_t*) pgm_read_ptr(&(all_images[i]));
    tft.pushImage(8, 6, IMAGE_WIDTH, IMAGE_HEIGHT, current_img);
    delay(80); 
    }

  }

  else if (dashboard_page == 2) {

      // ===================================================================
      // CẬP NHẬT DỮ LIỆU PAGE 2 (ĐA CÂN CHỈNH CHO MÀN HÌNH 284x76)
      // ===================================================================
   if(dashboard_page_latest!=2){
    dashboard_page_latest=2;
    drawDashboardLayout2();
    }   

    if (millis() - lastTftUpdate > 500) {
    lastTftUpdate = millis();
      // --- CARD 1: VOLTAGE ---
     
      char strBuf[16];
      snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vbat);
      tft.drawString(44, 22, strBuf, ACCENT_CYAN, CARD_BG, 1);

      snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vbus);
      tft.drawString(44, 35, strBuf, TEXT_WHITE, CARD_BG, 1);

      snprintf(strBuf, sizeof(strBuf), "%.2fV", g_vsys);
      tft.drawString(44, 48, strBuf, TEXT_WHITE, CARD_BG, 1);

      // --- CARD 2: CURRENT / NTC ---
      snprintf(strBuf, sizeof(strBuf), "%dmA ", g_ichg);
      tft.drawString(136, 22, strBuf, ACCENT_CYAN, CARD_BG, 1);

      if (g_iInLim >= 3250) {
        tft.drawString(136, 35, "MAX  ", ACCENT_CYAN, CARD_BG, 1);
      } else {
        snprintf(strBuf, sizeof(strBuf), "%dmA ", g_iInLim);
        tft.drawString(136, 35, strBuf, ACCENT_CYAN, CARD_BG, 1);
      }

      snprintf(strBuf, sizeof(strBuf), "%.0f%% ", g_ntcPct);
      tft.drawString(136, 48, strBuf, ACCENT_GREEN, CARD_BG, 1);

      // --- CARD 3: DIAGNOSTICS ---
      // Trạng thái sạc (STAT)
      if (!is_bq25895_alive) {
        tft.drawString(232, 22, "OFFLine", ACCENT_RED, CARD_BG, 1);
      } else {
        switch (g_chgStat) {
          case 0: tft.drawString(232, 22, "DISCHG ", TEXT_GRAY, CARD_BG, 1); break;
          case 1: tft.drawString(232, 22, "PRE-C  ", ACCENT_YELLOW, CARD_BG, 1); break;
          case 2: tft.drawString(232, 22, "FAST   ", ACCENT_CYAN, CARD_BG, 1); break;
          case 3: tft.drawString(232, 22, "FULL   ", ACCENT_GREEN, CARD_BG, 1); break;
        }
      }

      // Loại nguồn cắm vào (VBUS)
      switch (g_vbusStat) {
        case 0: tft.drawString(232, 35, "NO IN  ", TEXT_GRAY, CARD_BG, 1); break;
        case 1: tft.drawString(232, 35, "USB PC ", TEXT_WHITE, CARD_BG, 1); break;
        case 2: tft.drawString(232, 35, "USB CDP", TEXT_WHITE, CARD_BG, 1); break;
        case 3: tft.drawString(232, 35, "ADAPT  ", ACCENT_CYAN, CARD_BG, 1); break;
        case 7: tft.drawString(232, 35, "OTG OUT", ACCENT_YELLOW, CARD_BG, 1); break;
        default: tft.drawString(232, 35, "UNKNOW ", TEXT_GRAY, CARD_BG, 1); break;
      }

      // Báo lỗi IC (FAULT)
      if (g_faultStat == 0) {
        tft.drawString(232, 48, "OK     ", ACCENT_GREEN, CARD_BG, 1);
      } else {
        // Kiểm tra xem lỗi là Cảnh báo nhẹ (Warm/Cool) hay Lỗi nguy hiểm
        uint8_t ntcFault = g_faultStat & 0x07;
        uint16_t textColor = ACCENT_RED; // Mặc định báo lỗi nguy hiểm màu đỏ

        // Nếu chỉ là cảnh báo nhiệt độ nhẹ (Warm/Cool) thì hiện màu Vàng
        if ((g_faultStat & 0xF8) == 0 && (ntcFault == 5 || ntcFault == 6)) {
          textColor = ACCENT_YELLOW;
        }

        // Gọi hàm dịch mã và in lên màn hình
        tft.drawString(232, 48, getBQFaultText(g_faultStat), textColor, CARD_BG, 1);
      }
    } 
  }   

}
  */