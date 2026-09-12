//control display pin
#define Disp_ctrl_button 7
#define bl_ctrl_pin 6

//i2c define for charging circuit communication
#define I2C_SDA_PIN_bq   14
#define I2C_SCL_PIN_bq   15
#define BQ25895_ADDR     0x6A

//i2c define for trackpad communication
#define PIN_TRACKPAD_RESET  16
#define PIN_TRACKPAD_SDA    18
#define PIN_TRACKPAD_MOTION 22
#define PIN_TRACKPAD_SCL    23
#define TRACKPAD_I2C_ADDR   0x3B
#define REG_PRODUCT_ID      0x00
#define REG_MOTION          0x02

const int rowPins[7] = {27, 26, 25, 24, 21, 20, 19}; 
const int colPins[7] = {8, 9, 10, 11, 12, 13, 17};