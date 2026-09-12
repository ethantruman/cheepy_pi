import os
from PIL import Image, ImageSequence


def split_gif_to_frames(gif_path, output_folder="frames"):
    """Tách tất cả các frame của file GIF và lưu thành các tệp ảnh PNG riêng lẻ.

    :param gif_path: Đường dẫn tới file GIF cần tách
    :param output_folder: Thư mục chứa các ảnh đầu ra
    """
    # Xóa ký tự ngoặc kép bao quanh (nếu người dùng kéo thả file vào terminal)
    gif_path = gif_path.strip('"\'')
    output_folder = output_folder.strip('"\'')

    # Kiểm tra xem file GIF có tồn tại hay không
    if not os.path.exists(gif_path):
        print(f"Lỗi: Không tìm thấy file tại đường dẫn '{gif_path}'!")
        return

    # Tạo thư mục đầu ra nếu chưa tồn tại[cite: 1]
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    try:
        # Mở file GIF[cite: 1]
        with Image.open(gif_path) as img:
            # Lấy tên file gốc (không bao gồm đuôi file) để đặt tên cho từng frame[cite: 1]
            base_name = os.path.splitext(os.path.basename(gif_path))[0]

            # Duyệt qua từng frame trong GIF[cite: 1]
            for i, frame in enumerate(ImageSequence.Iterator(img)):
                # Chuyển đổi frame sang định dạng RGBA để giữ lại độ trong suốt (nếu có)[cite: 1]
                frame_rgba = frame.convert("RGBA")

                # Đặt tên file đầu ra, ví dụ: my_gif_frame_001.png[cite: 1]
                frame_filename = f"{base_name}_frame_{i+1:03d}.png"
                output_path = os.path.join(output_folder, frame_filename)

                # Lưu ảnh[cite: 1]
                frame_rgba.save(output_path, "PNG")

            print(
                f"\n✨ Đã tách thành công {img.n_frames} frames vào thư mục '{output_folder}'!"
            )
    except Exception as e:
        print(f"Có lỗi xảy ra khi xử lý file: {e}")


# --- Hướng dẫn sử dụng ---[cite: 1]
if __name__ == "__main__":
    print("=== CHƯƠNG TRÌNH TÁCH FRAME TỪ FILE GIF ===\n")
    
    # Hỏi đường dẫn tới file GIF
    gif_file = input("Nhập đường dẫn tới file GIF (hoặc kéo thả file vào đây): ")
    
    # Hỏi tên thư mục đầu ra
    output_dir = input("Nhập tên folder bạn muốn tạo để chứa ảnh: ")
    
    # Nếu người dùng bấm Enter mà không nhập tên folder, mặc định dùng "output_frames"
    if not output_dir.strip():
        output_dir = "output_frames"

    split_gif_to_frames(gif_file, output_dir)