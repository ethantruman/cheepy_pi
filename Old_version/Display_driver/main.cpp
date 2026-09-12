#define TFT_SWRESET   0x01
#define TFT_SLPOUT    0x11
#define TFT_COLMOD    0x3A
#define TFT_MADCTL    0x36
#define TFT_CASET     0x2A
#define TFT_RASET     0x2B
#define TFT_RAMWR     0x2C
#define TFT_DISPON    0x29
#define TFT_DISPOFF   0x28
#define TFT_INVON     0x21
#define TFT_INVOFF    0x20

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h> // libgpiod v2 headers
#include <cstdio>
#include <memory>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>    // Cho std::chrono::milliseconds
#include <algorithm> // Cho std::swap

//đây là một dự án driver cho các đời rpi dùng màn hình spi st7789v, yêu cầu dùng libgpiod v2 và spidev để điều khiển màn hình. Yêu cầu tương thích với mọi đời rpi, chỉ cần thay đổi /dev/gpiochip4 thành /dev/gpiochip0 nếu dùng pi cũ.

class ST7789; // <--- Dòng này gọi là Forward Declaration

class ST7789 {
private:
    int spi_fd;
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *request;

    unsigned int dc_off, rst_off;

    void send_command(uint8_t cmd) {
        gpiod_line_request_set_value(request, dc_off, GPIOD_LINE_VALUE_INACTIVE); // DC low
        //usleep(10); // DC setup time (10µs)
        // thử bỏ usleep để tăng tốc độ, nếu gặp lỗi hiển thị thì hãy bật lại
        ssize_t ret = write(spi_fd, &cmd, 1);
        if (ret != 1) {
            std::cerr << "Error: Failed to send command 0x" << std::hex << (int)cmd << std::endl;
        }
    }

    void send_data(uint8_t data) {
        gpiod_line_request_set_value(request, dc_off, GPIOD_LINE_VALUE_ACTIVE); // DC high
        //usleep(10); // DC setup time (10µs)
        // thử bỏ usleep để tăng tốc độ, nếu gặp lỗi hiển thị thì hãy bật lại
        ssize_t ret = write(spi_fd, &data, 1);
        if (ret != 1) {         
            std::cerr << "Error: Failed to send data 0x" << std::hex << (int)data << std::endl;
        }
    }

    void send_command_data(const uint8_t* data, size_t len) {
        // Hàm này gửi command và chuỗi data ngắn
        send_command(data[0]); // Gửi byte đầu tiên làm command)
        gpiod_line_request_set_value(request, dc_off, GPIOD_LINE_VALUE_ACTIVE); // DC high cho data
        usleep(10);
        if (len > 1) {
             ssize_t ret = write(spi_fd, &data[1], len - 1);
            if (ret != (ssize_t)(len - 1)) {
                std::cerr << "Error: When send_command_data";
            }
        }
    }
public:
    void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        uint16_t x_end = x0 + x1 - 1;
        uint16_t y_end = y0 + y1 - 1;

        uint8_t col_cmd[] = {0x2A, (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
                             (uint8_t)(x_end >> 8), (uint8_t)(x_end & 0xFF)};
        send_command_data(col_cmd, 5);

        // ILI9341 set row address (0x2B): SP[15:0] EP[15:0] - inclusive
        uint8_t row_cmd[] = {0x2B, (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
                             (uint8_t)(y_end >> 8), (uint8_t)(y_end & 0xFF)};
        send_command_data(row_cmd, 5);
    }


    //Hàm init cho màn
    ST7789(const char* spi_dev, const char* chip_path, unsigned int dc, unsigned int rst)
        : spi_fd(-1), chip(nullptr), settings(nullptr), line_cfg(nullptr),
          req_cfg(nullptr), request(nullptr), dc_off(dc), rst_off(rst) {

        // 1. SPI Setup
        spi_fd = open(spi_dev, O_RDWR);
        if (spi_fd < 0) {
            std::cerr << "Error: Cannot open SPI device: " << spi_dev
                      << " (" << std::strerror(errno) << ")" << std::endl;
            return;
        }

        // Set SPI Mode 0 (CPOL=0, CPHA=0) - required by ILI9341
        uint8_t mode = SPI_MODE_0;
        if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
            std::cerr << "Error: Failed to set SPI mode (" << std::strerror(errno) << ")" << std::endl;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        // Set MSB-first (0) 
        uint8_t lsb = 0;
        if (ioctl(spi_fd, SPI_IOC_WR_LSB_FIRST, &lsb) < 0) {
            std::cerr << "Error: Failed to set MSB-first (" << std::strerror(errno) << ")" << std::endl;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        
        uint32_t speed = 80000000;
        if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
            std::cerr << "Error: Failed to set SPI speed (" << std::strerror(errno) << ")" << std::endl;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        // Set SPI bits per word (8-bit)
        uint8_t bits = 8;
        if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
            std::cerr << "Error: Failed to set bits per word (" << std::strerror(errno) << ")" << std::endl;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        // 2. libgpiod v2 Setup
        chip = gpiod_chip_open(chip_path);
        if (!chip) {
            std::cerr << "Error: Cannot open GPIO chip: " << chip_path << std::endl;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        settings = gpiod_line_settings_new();
        if (!settings) {
            std::cerr << "Error: Cannot allocate line settings" << std::endl;
            gpiod_chip_close(chip);
            chip = nullptr;
            close(spi_fd);
            spi_fd = -1;
            return;
        }
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

        line_cfg = gpiod_line_config_new();
        if (!line_cfg) {
            std::cerr << "Error: Cannot allocate line config" << std::endl;
            gpiod_line_settings_free(settings);
            settings = nullptr;
            gpiod_chip_close(chip);
            chip = nullptr;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        unsigned int offsets[] = {dc_off, rst_off};
        gpiod_line_config_add_line_settings(line_cfg, offsets, 2, settings);

        req_cfg = gpiod_request_config_new();
        if (!req_cfg) {
            std::cerr << "Error: Cannot allocate request config" << std::endl;
            gpiod_line_config_free(line_cfg);
            line_cfg = nullptr;
            gpiod_line_settings_free(settings);
            settings = nullptr;
            gpiod_chip_close(chip);
            chip = nullptr;
            close(spi_fd);
            spi_fd = -1;
            return;
        }
        gpiod_request_config_set_consumer(req_cfg, "ili9341_test");

        request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
        if (!request) {
            std::cerr << "Error: Cannot request GPIO lines" << std::endl;
            gpiod_request_config_free(req_cfg);
            req_cfg = nullptr;
            gpiod_line_config_free(line_cfg);
            line_cfg = nullptr;
            gpiod_line_settings_free(settings);
            settings = nullptr;
            gpiod_chip_close(chip);
            chip = nullptr;
            close(spi_fd);
            spi_fd = -1;
            return;
        }

        // Reset
        gpiod_line_request_set_value(request, rst_off, GPIOD_LINE_VALUE_INACTIVE);
        usleep(100000); // 100ms low
        gpiod_line_request_set_value(request, rst_off, GPIOD_LINE_VALUE_ACTIVE);
        usleep(150000); // 150ms high (wait for reset complete)

        // Init Sequence
        send_command(TFT_SWRESET); // Software Reset
        usleep(5000);

        send_command(TFT_SLPOUT);
        usleep(120000);

        send_command(TFT_COLMOD);
        send_data(0x55);

        send_command(TFT_MADCTL);
        send_data(0x60);  // Landscape: rotate 90°

        send_command(TFT_DISPON);
        usleep(100000);
        send_command(TFT_INVON);


    }

    void send_reset() {
        // Hardware reset sequence
        gpiod_line_request_set_value(request, rst_off, GPIOD_LINE_VALUE_INACTIVE);
        usleep(100000); // 100ms reset low
        gpiod_line_request_set_value(request, rst_off, GPIOD_LINE_VALUE_ACTIVE);
        usleep(150000); // 150ms reset high (wait for reset complete)
        std::cout << "Reset completed" << std::endl;
    }

    bool is_initialized() const {
        return spi_fd >= 0 && chip != nullptr && request != nullptr;
    }

// Helper function để gửi dữ liệu lớn theo chunks (tránh SPI buffer limit 4096 bytes)
    bool write_chunked(const uint8_t* data, size_t total_size) {
        const size_t CHUNK_SIZE = 4096;  // Linux SPI default buffer limit
        size_t bytes_written = 0;

        while (bytes_written < total_size) {
            size_t chunk_to_write = std::min(CHUNK_SIZE, total_size - bytes_written);
            ssize_t ret = write(spi_fd, data + bytes_written, chunk_to_write);

            if (ret < 0) {
                std::cerr << "Error: write() failed at offset " << bytes_written
                         << " (" << std::strerror(errno) << ")" << std::endl;
                return false;
            }

            if (ret == 0) {
                std::cerr << "Error: write() returned 0 at offset " << bytes_written << std::endl;
                return false;
            }

            bytes_written += ret;
        }

        return true;
    }
    //hàm gửi đúng 1 hình đã được rgb565 hóa.
    void display_buffer(const uint8_t* raw_data, size_t size) {
        if (!is_initialized()) {
            std::cerr << "Error: Display not initialized" << std::endl;
            return;
        }

        // Start Memory Write command (0x2C) - báo chip sẽ gửi pixel data
        send_command(0x2C); //đã low dc 

        // Set DC high and send pixel data in chunks
        gpiod_line_request_set_value(request, dc_off, GPIOD_LINE_VALUE_ACTIVE); // sau đó high dc 
        //usleep(10); thử bỏ usleep để tăng tốc độ, nếu gặp lỗi hiển thị thì hãy bật lại

        if (!write_chunked(raw_data, size)) {
            std::cerr << "Error: Failed to write display buffer (total size: " << size << " bytes)" << std::endl;
        }
        //flow: hạ low, gửi 0x2C , nâng high, gửi pixel
    }

        ~ST7789() {
        if (request) {
            gpiod_line_request_release(request);
            request = nullptr;
        }
        if (req_cfg) {
            gpiod_request_config_free(req_cfg);
            req_cfg = nullptr;
        }
        if (line_cfg) {
            gpiod_line_config_free(line_cfg);
            line_cfg = nullptr;
        }
        if (settings) {
            gpiod_line_settings_free(settings);
            settings = nullptr;
        }
        if (chip) {
            gpiod_chip_close(chip);
            chip = nullptr;
        }
        if (spi_fd >= 0) {
            close(spi_fd);
            spi_fd = -1;
        }
    }
};

    void scale_and_convert_fast3(const uint8_t* src, uint8_t* dst) {    //legacy alias
    const int DST_W = 320;
    const int DST_H = 340; // Tổng chiều cao 2 màn hình gộp lại
    const size_t TOTAL_BYTES = DST_W * DST_H * 2; // Mỗi pixel chiếm đúng 2 bytes

    // Sao chép vùng nhớ siêu tốc bằng hàm tiêu chuẩn của C++
    std::memcpy(dst, src, TOTAL_BYTES);
    }



void start_pipeline_mirror_triple_with_cpu_scale(ST7789 &lcd,ST7789 &lcd1) {
    // Kích thước ảnh nguồn từ wf-recorder (Ví dụ: 640x340 dạng BGR/RGB)
    const int W_SRC = 320, H_SRC = 340;
    const size_t BGR_SIZE_SRC = W_SRC * H_SRC * 2; // Kích thước 1 khung hình thô nhận từ stdin

    // Kích thước ảnh đích hiển thị lên 2 màn hình ST7789 (320x340)
    const int W_DST = 320, H_DST = 340;
    const size_t LCD_BUF_SIZE = W_DST * H_DST * 2; // Kích thước sau khi convert sang RGB565 (2 bytes/pixel)

    const size_t LCD_BUF_SIZE_REAL = W_DST * 170 * 2;//kích thước thật

    // TỐI ƯU TỐC ĐỘ ĐỌC LUỒNG PIPELINE
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    // KHỞI TẠO BỘ ĐỆM TRIPLE BUFFERING CHO ẢNH ĐÍCH (RGB565)
    std::vector<uint8_t> buffer_A(LCD_BUF_SIZE);
    std::vector<uint8_t> buffer_B(LCD_BUF_SIZE);
    std::vector<uint8_t> buffer_C(LCD_BUF_SIZE);

    uint8_t* ptr_worker  = buffer_A.data(); // Worker ghi ảnh đã convert vào đây
    uint8_t* ptr_ready   = buffer_B.data(); // Trạm trung chuyển chứa frame mới nhất
    uint8_t* ptr_display = buffer_C.data(); // Luồng chính độc chiếm để bơm SPI

    std::mutex mtx;
    bool has_new_frame = false;
    std::atomic<bool> running{true};

    // Khóa sẵn vị trí cửa sổ màn hình (offset y=35)
    lcd.set_window(0, 35, 320, 170);
    lcd1.set_window(0, 35, 320, 170);
    // --- LUỒNG WORKER: ĐỌC PIPELINE THÔ & TỰ COMPUTE BẰNG CPU ---
// --- LUỒNG WORKER: ĐỌC PIPELINE THÔ & TỰ COMPUTE BẰNG CPU ---
    std::thread worker([&]() {
        // Bộ đệm trung gian để hứng dữ liệu ảnh thô từ stdin
        std::vector<uint8_t> bgr_raw(BGR_SIZE_SRC);

        // =========================================================================
        // 🛠️ ĐOẠN CODE KIỂM THỬ: XẢ BỎ CÁC BYTE METADATA/RÁC KHI VỪA KHỞI ĐỘNG LUỒNG
        // =========================================================================
        // Bạn hãy tăng/giảm giá trị BYTES_TO_SKIP này để dò tìm điểm khít dòng.
        // Ví dụ thử nghiệm ban đầu: skip 8 bytes, 16 bytes, hoặc thậm chí 1024 bytes...
        const size_t BYTES_TO_SKIP = 24; 
        
        if (BYTES_TO_SKIP > 0) {
            std::vector<char> garbage_buffer(BYTES_TO_SKIP);
            std::cin.read(garbage_buffer.data(), BYTES_TO_SKIP);
            std::cout << "[DEBUG] Đã xả bỏ " << std::cin.gcount() 
                      << " bytes metadata ban đầu của luồng." << std::endl;
        }
        // =========================================================================

        // Bắt đầu vào vòng lặp xử lý khung hình chính thức
        while (running.load()) {
            if (!std::cin.good() || std::cin.eof()) {
                running.store(false);
                break;
            }

            // Đọc trọn vẹn 1 khung hình thô từ đường ống stdin
            std::cin.read(reinterpret_cast<char*>(bgr_raw.data()), BGR_SIZE_SRC);
            
            // Nếu đọc đủ số bytes của khung hình thô thì mới xử lý
            if (std::cin.gcount() == (ssize_t)BGR_SIZE_SRC) {
                
                // Sử dụng CPU xử lý sao chép
                scale_and_convert_fast3(bgr_raw.data(), ptr_worker);

                // Khóa mutex cực nhanh để đẩy sang trạm trung chuyển ptr_ready
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    std::swap(ptr_worker, ptr_ready); 
                    has_new_frame = true;
                }
            }
        }
    });

    // --- LUỒNG CHÍNH: HIỂN THỊ SPI LIÊN TỤC (Giữ nguyên logic non-blocking) ---
    while (running.load()) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (has_new_frame) {
                std::swap(ptr_display, ptr_ready);
                has_new_frame = false;
            }
        } 

        // Bơm dữ liệu liên tục ra màn hình qua bus SPI kịch trần tốc độ phần cứng

        uint8_t* ptr_display_half_bottom = ptr_display + LCD_BUF_SIZE_REAL;

        // 🚀 ĐỘT PHÁ SONG SONG: Tạo 2 luồng phụ chạy đồng thời kịch trần phần cứng
        // Luồng 1 gửi dữ liệu ra SPI0 cho màn trên
        std::thread t_top([&]() {
            lcd.display_buffer(ptr_display, LCD_BUF_SIZE_REAL);
        });

        // Luồng 2 gửi dữ liệu ra SPI1 cho màn dưới
        std::thread t_bottom([&]() {
            lcd1.display_buffer(ptr_display_half_bottom, LCD_BUF_SIZE_REAL);
        });

        // 🛑 Bắt buộc đợi cả 2 luồng phụ hoàn thành việc gửi qua SPI rồi mới bước sang chu kỳ mới
        t_top.join();
        t_bottom.join();

        // Nhường CPU 1 nhịp siêu nhỏ để hệ điều hành điều phối luồng ổn định
        std::this_thread::yield(); 
    }

    if (worker.joinable()) worker.join();
}

int main() {
    // 1. Khởi tạo màn hình (Giữ nguyên như cũ)
    // Lưu ý: Thay "/dev/gpiochip4" nếu dùng Pi 5, hoặc "/dev/gpiochip0" nếu Pi cũ
    ST7789 lcd("/dev/spidev0.0", "/dev/gpiochip0", 24, 25);
    ST7789 lcd1("/dev/spidev1.0", "/dev/gpiochip0", 18, 23);
    if (!lcd.is_initialized()||!lcd1.is_initialized()) {
        std::cerr << "Error: Failed to initialize LCD. Check connections and permissions." << std::endl;
        return 1;
    }

        try {
start_pipeline_mirror_triple_with_cpu_scale(lcd,lcd1);
    } catch (const std::exception& e) {
        std::cerr << "Có lỗi xảy ra: " << e.what() << std::endl;
        return 1;
    }

}

//g++ -O3 -std=c++17 main.cpp -o tft_driver -lgpiod -lpthread
