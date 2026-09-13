//these hardware definition is for the 3.0 version board
//ver3git
//learn github

//control display pin
#define backlight_control_pin 5

//uart define for communicating to raspberry pi 5
#define uart_tx   8
#define uart_rx   9

//i2c define for charging circuit communication, i2c1
#define I2C_SDA_PIN_bq   6
#define I2C_SCL_PIN_bq   7
#define BQ25895_ADDR     0x6A

//i2c define for trackpad communication, i2c0
#define PIN_TRACKPAD_RESET  19
#define PIN_TRACKPAD_SDA    20
#define PIN_TRACKPAD_MOTION 18
#define PIN_TRACKPAD_SCL    21
#define TRACKPAD_I2C_ADDR   0x3B
#define REG_PRODUCT_ID      0x00
#define REG_MOTION          0x02

//for keyboard's light
#define RP_BLK 28
#define RP_PBLK 29

//for keyboard pin
const int colPins[7] = {10, 11, 12, 13, 14, 15, 16}; 
const int rowPins[7] = {27, 26, 25, 24, 23, 22, 17};