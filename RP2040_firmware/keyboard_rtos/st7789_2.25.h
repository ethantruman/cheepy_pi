#include <Arduino.h>
#include <SPI.h>

#define TFT_SWRESET   0x01
#define TFT_SLPOUT    0x11
#define TFT_COLMOD    0x3A
#define TFT_MADCTL    0x36
#define TFT_CASET     0x2A
#define TFT_RASET     0x2B
#define TFT_RAMWR     0x2C
#define TFT_DISPON    0x29
#define TFT_INVON     0x21

#define DISPLAY_WIDTH  284
#define DISPLAY_HEIGHT 76

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

// điều chỉnh offset nếu hình bị tràn hoặc hiển thị xéo/lệch
// ST7789 thường dùng offset 0, 0 hoặc 0, 52 / 40, 53 tùy nhà sản xuất panel
#define TFT_X_OFFSET  18
#define TFT_Y_OFFSET  82 



static const uint8_t font5x7[][5] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, // 0x20 ' '
    0x00, 0x00, 0x5F, 0x00, 0x00, // 0x21 '!'
    0x00, 0x07, 0x00, 0x07, 0x00, // 0x22 '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14, // 0x23 '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // 0x24 '$'
    0x23, 0x13, 0x08, 0x64, 0x62, // 0x25 '%'
    0x36, 0x49, 0x55, 0x22, 0x50, // 0x26 '&'
    0x00, 0x05, 0x03, 0x00, 0x00, // 0x27 '''
    0x00, 0x1C, 0x22, 0x41, 0x00, // 0x28 '('
    0x00, 0x41, 0x22, 0x1C, 0x00, // 0x29 ')'
    0x14, 0x08, 0x3E, 0x08, 0x14, // 0x2A '*'
    0x08, 0x08, 0x3E, 0x08, 0x08, // 0x2B '+'
    0x00, 0x50, 0x30, 0x00, 0x00, // 0x2C ','
    0x08, 0x08, 0x08, 0x08, 0x08, // 0x2D '-'
    0x00, 0x60, 0x60, 0x00, 0x00, // 0x2E '.'
    0x20, 0x10, 0x08, 0x04, 0x02, // 0x2F '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0x30 '0'
    0x00, 0x42, 0x7F, 0x40, 0x00, // 0x31 '1'
    0x42, 0x61, 0x51, 0x49, 0x46, // 0x32 '2'
    0x21, 0x41, 0x45, 0x4B, 0x31, // 0x33 '3'
    0x18, 0x14, 0x12, 0x7F, 0x10, // 0x34 '4'
    0x27, 0x45, 0x45, 0x45, 0x39, // 0x35 '5'
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 0x36 '6'
    0x01, 0x71, 0x09, 0x05, 0x03, // 0x37 '7'
    0x36, 0x49, 0x49, 0x49, 0x36, // 0x38 '8'
    0x06, 0x49, 0x49, 0x29, 0x1E, // 0x39 '9'
    0x00, 0x36, 0x36, 0x00, 0x00, // 0x3A ':'
    0x00, 0x56, 0x36, 0x00, 0x00, // 0x3B ';'
    0x08, 0x14, 0x22, 0x41, 0x00, // 0x3C '<'
    0x14, 0x14, 0x14, 0x14, 0x14, // 0x3D '='
    0x00, 0x41, 0x22, 0x14, 0x08, // 0x3E '>'
    0x02, 0x01, 0x51, 0x09, 0x06, // 0x3F '?'
    0x32, 0x49, 0x79, 0x41, 0x3E, // 0x40 '@'
    0x7E, 0x11, 0x11, 0x11, 0x7E, // 0x41 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36, // 0x42 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22, // 0x43 'C'
    0x7F, 0x41, 0x41, 0x22, 0x1C, // 0x44 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41, // 0x45 'E'
    0x7F, 0x09, 0x09, 0x09, 0x01, // 0x46 'F'
    0x3E, 0x41, 0x49, 0x49, 0x7A, // 0x47 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F, // 0x48 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00, // 0x49 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01, // 0x4A 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41, // 0x4B 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40, // 0x4C 'L'
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // 0x4D 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F, // 0x4E 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E, // 0x4F 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06, // 0x50 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E, // 0x51 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46, // 0x52 'R'
    0x46, 0x49, 0x49, 0x49, 0x31, // 0x53 'S'
    0x01, 0x01, 0x7F, 0x01, 0x01, // 0x54 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F, // 0x55 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F, // 0x56 'V'
    0x7F, 0x20, 0x18, 0x20, 0x7F, // 0x57 'W'
    0x63, 0x14, 0x08, 0x14, 0x63, // 0x58 'X'
    0x03, 0x04, 0x78, 0x04, 0x03, // 0x59 'Y'
    0x61, 0x51, 0x49, 0x45, 0x43, // 0x5A 'Z'
    0x00, 0x7F, 0x41, 0x41, 0x00, // 0x5B '['
    0x02, 0x04, 0x08, 0x10, 0x20, // 0x5C '\'
    0x00, 0x41, 0x41, 0x7F, 0x00, // 0x5D ']'
    0x04, 0x02, 0x01, 0x02, 0x04, // 0x5E '^'
    0x40, 0x40, 0x40, 0x40, 0x40, // 0x5F '_'
    0x00, 0x01, 0x02, 0x04, 0x00, // 0x60 '`'
    0x20, 0x54, 0x54, 0x54, 0x78, // 0x61 'a'
    0x7F, 0x48, 0x44, 0x44, 0x38, // 0x62 'b'
    0x38, 0x44, 0x44, 0x44, 0x20, // 0x63 'c'
    0x38, 0x44, 0x44, 0x48, 0x7F, // 0x64 'd'
    0x38, 0x54, 0x54, 0x54, 0x18, // 0x65 'e'
    0x08, 0x7E, 0x09, 0x01, 0x02, // 0x66 'f'
    0x0C, 0x52, 0x52, 0x52, 0x3E, // 0x67 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78, // 0x68 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00, // 0x69 'i'
    0x20, 0x40, 0x44, 0x3D, 0x00, // 0x6A 'j'
    0x7F, 0x10, 0x28, 0x44, 0x00, // 0x6B 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00, // 0x6C 'l'
    0x7C, 0x04, 0x18, 0x04, 0x78, // 0x6D 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78, // 0x6E 'n'
    0x38, 0x44, 0x44, 0x44, 0x38, // 0x6F 'o'
    0x7C, 0x14, 0x14, 0x14, 0x08, // 0x70 'p'
    0x08, 0x14, 0x14, 0x18, 0x7C, // 0x71 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08, // 0x72 'r'
    0x48, 0x54, 0x54, 0x54, 0x20, // 0x73 's'
    0x04, 0x3F, 0x44, 0x40, 0x20, // 0x74 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C, // 0x75 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C, // 0x76 'v'
    0x3C, 0x40, 0x30, 0x40, 0x3C, // 0x77 'w'
    0x44, 0x28, 0x10, 0x28, 0x44, // 0x78 'x'
    0x0C, 0x50, 0x50, 0x50, 0x3C, // 0x79 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44, // 0x7A 'z'
    0x00, 0x08, 0x36, 0x41, 0x00, // 0x7B '{'
    0x00, 0x00, 0x7F, 0x00, 0x00, // 0x7C '|'
    0x00, 0x41, 0x36, 0x08, 0x00, // 0x7D '}'
    0x10, 0x08, 0x08, 0x10, 0x08, // 0x7E '~'
    0x00, 0x00, 0x00, 0x00, 0x00  // 0x7F DEL
};

class TFTDriver {
private:
    static constexpr uint8_t TFT_DC  = 0;
    static constexpr uint8_t TFT_RST = 4;
    static constexpr uint8_t TFT_CS  = 1;
    static constexpr uint8_t TFT_SCK = 2;
    static constexpr uint8_t TFT_SDA = 3;

    // Tần số SPI: 27MHz hoặc 40MHz là chuẩn cho ST7789
    SPISettings spiSettings = SPISettings(16000000, MSBFIRST, SPI_MODE0);

    void spi_write_cmd(uint8_t cmd) {
        digitalWrite(TFT_DC, LOW);   // DC = LOW: Lệnh
        digitalWrite(TFT_CS, LOW);   
        
        SPI.beginTransaction(spiSettings);
        SPI.transfer(cmd); 
        SPI.endTransaction();
        
        digitalWrite(TFT_CS, HIGH);  
    }

    void spi_write_byte(uint8_t data) {
        digitalWrite(TFT_DC, HIGH);  // DC = HIGH: Dữ liệu
        digitalWrite(TFT_CS, LOW);
        
        SPI.beginTransaction(spiSettings);
        SPI.transfer(data); 
        SPI.endTransaction();
        
        digitalWrite(TFT_CS, HIGH);
    }

public:
    TFTDriver(){}

    void init() {
        pinMode(TFT_DC, OUTPUT);
        pinMode(TFT_RST, OUTPUT);
        pinMode(TFT_CS, OUTPUT);
        
        digitalWrite(TFT_CS, HIGH);
        digitalWrite(TFT_DC, HIGH);

        // Khai báo chân SPI cho RP2040 Pico
        SPI.setTX(TFT_SDA);  // MOSI
        SPI.setSCK(TFT_SCK); // SCK
        SPI.begin();

        // Hardware Reset
        digitalWrite(TFT_RST, HIGH);
        delay(50);
        digitalWrite(TFT_RST, LOW);
        delay(50);
        digitalWrite(TFT_RST, HIGH);
        delay(120);

        // Chuỗi khởi tạo cho ST7789
        spi_write_cmd(TFT_SWRESET);
        delay(150);
        
        spi_write_cmd(TFT_SLPOUT);
        delay(120);
        
        spi_write_cmd(TFT_COLMOD);
        spi_write_byte(0x55);  // Format 16-bit color (RGB565)
        delay(10);
        
        spi_write_cmd(TFT_MADCTL);
        spi_write_byte(0x60);  // Xoay màn hình Landscape
        delay(10);
        
        //spi_write_cmd(TFT_INVON); // Bật Color Inversion (hầu hết màn TFT hiện đại cần dòng này)
        delay(10);

        spi_write_cmd(TFT_DISPON);
        delay(120);
    }

    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        // Cộng thêm Offset màn hình
        x0 += TFT_X_OFFSET;
        x1 += TFT_X_OFFSET;
        y0 += TFT_Y_OFFSET;
        y1 += TFT_Y_OFFSET;

        // COLUMN ADDRESS
        spi_write_cmd(TFT_CASET);
        digitalWrite(TFT_DC, HIGH);
        digitalWrite(TFT_CS, LOW);
        SPI.beginTransaction(spiSettings);
        SPI.transfer(x0 >> 8);     
        SPI.transfer(x0 & 0xFF);    
        SPI.transfer(x1 >> 8);     
        SPI.transfer(x1 & 0xFF);    
        SPI.endTransaction();
        digitalWrite(TFT_CS, HIGH);

        // ROW ADDRESS
        spi_write_cmd(TFT_RASET);
        digitalWrite(TFT_DC, HIGH);
        digitalWrite(TFT_CS, LOW);
        SPI.beginTransaction(spiSettings);
        SPI.transfer(y0 >> 8);     
        SPI.transfer(y0 & 0xFF);    
        SPI.transfer(y1 >> 8);     
        SPI.transfer(y1 & 0xFF);    
        SPI.endTransaction();
        digitalWrite(TFT_CS, HIGH);

        // RAM WRITE CMD
        spi_write_cmd(TFT_RAMWR);
    }

    void pushImage(int x, int y, int w, int h, const uint16_t *data, bool preswapped = false) {
        int x1 = x + w - 1;
        int y1 = y + h - 1;
        setAddrWindow(x, y, x1, y1); 
        
        digitalWrite(TFT_DC, HIGH); 
        digitalWrite(TFT_CS, LOW);
        
        SPI.beginTransaction(spiSettings);
        
        uint32_t pixel_count = (uint32_t)w * h;
        static uint8_t buffer[1024]; 
        
        for (uint32_t i = 0; i < pixel_count; i += 512) {
            uint32_t chunk_size = (pixel_count - i > 512) ? 512 : (pixel_count - i);
            
            if (preswapped) { 
                SPI.transfer((uint8_t*)&data[i], chunk_size * 2);
            } else {
                for (uint32_t j = 0; j < chunk_size; j++) {
                    uint16_t pixel = data[i + j];
                    buffer[j * 2]     = pixel >> 8;   
                    buffer[j * 2 + 1] = pixel & 0xFF; 
                }
                SPI.transfer(buffer, chunk_size * 2);
            }
        }
        
        SPI.endTransaction();
        digitalWrite(TFT_CS, HIGH);
    }

    // Hàm xóa màn hình dùng để Test kiểm tra nhanh
    void fillScreen(uint16_t color) {
        setAddrWindow(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
        
        digitalWrite(TFT_DC, HIGH);
        digitalWrite(TFT_CS, LOW);
        SPI.beginTransaction(spiSettings);

        uint8_t msb = color >> 8;
        uint8_t lsb = color & 0xFF;

        for (uint32_t i = 0; i < (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
            SPI.transfer(msb);
            SPI.transfer(lsb);
        }

        SPI.endTransaction();
        digitalWrite(TFT_CS, HIGH);
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
        
        setAddrWindow(x, y, x, y);
        digitalWrite(TFT_DC, HIGH);
        digitalWrite(TFT_CS, LOW);
        
        SPI.beginTransaction(spiSettings);
        SPI.transfer(color >> 8);
        SPI.transfer(color & 0xFF);
        SPI.endTransaction();
        
        digitalWrite(TFT_CS, HIGH);
    }
    
    // 2. Vẽ hình chữ nhật đặc (Dùng để tô nền hoặc vẽ ký tự phóng to nhanh hơn)
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    setAddrWindow(x, y, x + w - 1, y + h - 1);

    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    SPI.beginTransaction(spiSettings);

    uint8_t msb = color >> 8;
    uint8_t lsb = color & 0xFF;
    uint32_t total_pixels = (uint32_t)w * h;

    for (uint32_t i = 0; i < total_pixels; i++) {
        SPI.transfer(msb);
        SPI.transfer(lsb);
    }

    SPI.endTransaction();
    digitalWrite(TFT_CS, HIGH);
}

// 3. Vẽ 1 ký tự ASCII (Có hỗ trợ phóng to 'size' và làm nền trong suốt)
// Nếu bg_color == color -> Nền trong suốt
void drawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color, uint8_t size = 1) {
    if (c < 32 || c > 126) c = '?'; // Xử lý các ký tự nằm ngoài bảng ASCII hỗ trợ

    uint8_t c_idx = c - 32;

    for (int8_t i = 0; i < 5; i++) { // Quét 5 cột pixel của font
        uint8_t line = pgm_read_byte(&font5x7[c_idx][i]);

        for (int8_t j = 0; j < 8; j++) { // Quét 8 bit theo chiều dọc
            if (line & 0x01) { // Pixel bật
                if (size == 1) {
                    drawPixel(x + i, y + j, color);
                } else {
                    fillRect(x + i * size, y + j * size, size, size, color);
                }
            } else if (bg_color != color) { // Pixel tắt (Xử lý màu nền)
                if (size == 1) {
                    drawPixel(x + i, y + j, bg_color);
                } else {
                    fillRect(x + i * size, y + j * size, size, size, bg_color);
                }
            }
            line >>= 1;
        }
    }

    // Xử lý khoảng trống (1 cột) giữa các chữ cái để chữ không bị dính vào nhau
    if (bg_color != color) {
        if (size == 1) {
            for (int8_t j = 0; j < 8; j++) drawPixel(x + 5, y + j, bg_color);
        } else {
            fillRect(x + 5 * size, y, size, 8 * size, bg_color);
        }
    }
}

// 4. In chuỗi văn bản (Tự động xuống dòng khi gặp '\n' hoặc khi chạm mép màn hình)
void drawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg_color, uint8_t size = 1) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*str) {
        if (*str == '\n') { // Gặp ký tự xuống dòng
            cursor_x = x;
            cursor_y += 8 * size;
        } else {
            // Tự động xuống dòng nếu ký tự tiếp theo vượt quá chiều rộng màn hình
            if ((cursor_x + 6 * size) > DISPLAY_WIDTH) {
                cursor_x = x;
                cursor_y += 8 * size;
            }

            drawChar(cursor_x, cursor_y, *str, color, bg_color, size);
            cursor_x += 6 * size; // Mỗi ký tự rộng 5px + 1px khoảng cách
        }
        str++;
    }
}

    // Hàm in chữ kiểu máy đánh chữ / Hacker loading
void drawHackerText(int16_t x, int16_t y, const char *str, uint8_t size = 1, uint16_t delayMs = 40) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            cursor_y += 8 * size;
        } else {
            // In từng ký tự một (Nền đen 0x0000)
            drawChar(cursor_x, cursor_y, *str, 0x07E0, 0x0000, size);
            cursor_x += 6 * size;
            
            // Tạo độ trễ nhỏ sau mỗi ký tự
            delay(delayMs); 
        }
        str++;
    }
}

};