import sys
import os
import re
from PIL import Image

# Các định dạng ảnh được hỗ trợ
IMAGE_EXTENSIONS = ('.png', '.jpg', '.jpeg', '.bmp')

def sanitize_variable_name(name):
    """
    Chuyển đổi tên file thành tên biến C++ hợp lệ:
    - Loại bỏ phần mở rộng file.
    - Thay thế các ký tự không hợp lệ bằng dấu gạch dưới (_).
    - Thêm tiền tố 'img_' nếu tên bắt đầu bằng số.
    """
    base_name = os.path.splitext(name)[0]
    clean_name = re.sub(r'[^a-zA-Z0-9_]', '_', base_name)
    if clean_name and clean_name[0].isdigit():
        clean_name = f"img_{clean_name}"
    return clean_name or "img_unnamed"

def process_single_image(image_path, target_width=284, target_height=76):
    """Đọc ảnh, resize và trả về danh sách chuỗi hex RGB565."""
    img = Image.open(image_path).convert('RGB')
    img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
    
    width, height = img.size
    pixels = img.load()
    c_array = []
    
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            
            # RGB888 -> RGB565
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F
            
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            
            # Big-Endian (MSB First)
            msb = (rgb565 >> 8) & 0xFF
            lsb = rgb565 & 0xFF
            rgb565_big_endian = (msb << 8) | lsb
            
            c_array.append(f"0x{rgb565_big_endian:04X}")
            
    return c_array

def get_resolution_input(default_width=284, default_height=76):
    """Hàm hỏi người dùng nhập độ phân giải mong muốn."""
    res_input = input(f"Nhập độ phân giải WxH (ví dụ: 240x240) [Mặc định: {default_width}x{default_height}]: ").strip().lower()
    
    if not res_input:
        return default_width, default_height

    # Tìm các chuỗi số trong đầu vào (hỗ trợ cả dạng '284x76', '284 76', '284,76')
    numbers = re.findall(r'\d+', res_input)
    if len(numbers) >= 2:
        width = int(numbers[0])
        height = int(numbers[1])
        if width > 0 and height > 0:
            return width, height
            
    print(f"⚠️ Nhập không hợp lệ! Dùng độ phân giải mặc định ({default_width}x{default_height}).")
    return default_width, default_height

def convert_folder_to_c_array(folder_path, output_path="image_data.h", width=284, height=76):
    if not os.path.exists(folder_path):
        print(f"❌ Thư mục không tồn tại: {folder_path}")
        return

    # Lấy danh sách các file ảnh trong thư mục và sắp xếp theo tên
    image_files = [
        f for f in os.listdir(folder_path) 
        if f.lower().endswith(IMAGE_EXTENSIONS)
    ]
    image_files.sort()

    if not image_files:
        print(f"⚠️ Không tìm thấy file ảnh hợp lệ nào trong thư mục '{folder_path}'!")
        return

    print(f"\n🔍 Tìm thấy {len(image_files)} ảnh. Kích thước sẽ scale: {width}x{height} px. Đang xử lý...")

    array_names = []
    
    try:
        with open(output_path, "w", encoding="utf-8") as f:
            # Ghi phần Header Guard
            f.write("// File tự động tạo bởi Python Script\n")
            f.write(f"// Kích thước mỗi ảnh: {width}x{height} (RGB565 Big-Endian)\n")
            f.write("#ifndef IMAGE_DATA_H\n#define IMAGE_DATA_H\n\n")
            f.write("#include <Arduino.h>\n\n")
            f.write(f"#define IMAGE_WIDTH {width}\n")
            f.write(f"#define IMAGE_HEIGHT {height}\n")
            f.write(f"#define IMAGE_PIXEL_COUNT {width * height}\n")
            f.write(f"#define TOTAL_IMAGES {len(image_files)}\n\n")

            # Duyệt qua từng ảnh và tạo mảng C++
            for idx, file_name in enumerate(image_files):
                img_full_path = os.path.join(folder_path, file_name)
                var_name = sanitize_variable_name(file_name)
                
                # Đảm bảo tên biến không bị trùng lặp
                if var_name in array_names:
                    var_name = f"{var_name}_{idx}"
                array_names.append(var_name)

                print(f"  [{idx + 1}/{len(image_files)}] Đang xử lý: {file_name} -> {var_name}")
                
                c_array = process_single_image(img_full_path, target_width=width, target_height=height)

                f.write(f"// Image: {file_name}\n")
                f.write(f"const uint16_t {var_name}[{len(c_array)}] PROGMEM = {{\n")
                
                # Ghi 12 pixel trên mỗi dòng
                for i in range(0, len(c_array), 12):
                    line = ", ".join(c_array[i:i+12])
                    f.write(f"    {line},\n")
                f.write("};\n\n")

            # Tạo mảng chứa tất cả các con trỏ tới từng ảnh
            f.write("// Mảng chứa con trỏ tới tất cả các ảnh để duyệt vòng lặp for\n")
            f.write("const uint16_t* const all_images[TOTAL_IMAGES] PROGMEM = {\n")
            for name in array_names:
                f.write(f"    {name},\n")
            f.write("};\n\n")

            f.write("#endif // IMAGE_DATA_H\n")

        print("\n✅ Đã chuyển đổi thành công tất cả ảnh!")
        print(f"📁 File đầu ra: {os.path.abspath(output_path)}")
        print(f"🖼️ Tổng số ảnh đã ghi: {len(array_names)}")

    except Exception as e:
        print(f"❌ Có lỗi xảy ra: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        folder = sys.argv[1]
    else:
        folder = input("Nhập đường dẫn thư mục chứa ảnh của bạn: ").strip()
        folder = folder.strip("'\"")
        
    # Hỏi độ phân giải muốn scale
    width, height = get_resolution_input(default_width=284, default_height=76)
    
    convert_folder_to_c_array(folder, width=width, height=height)