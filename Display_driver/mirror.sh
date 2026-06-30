#!/bin/bash

echo "--- Đang cấu hình độ phân giải đầu ra ---"
wlr-randr --output NOOP-1 --custom-mode 640x680

#wlr-randr --output NOOP-1 --transform 360
wlr-randr --output NOOP-1 --transform normal
echo "Nhấn Ctrl+C để dừng lại."
#wf-recorder --muxer=v4l2 --codec=rawvideo --pixel-format=bgr24 -f /dev/video1
#wf-recorder --muxer=v4l2 --codec=rawvideo --pixel-format=bgr24 -F scale=640:680 -f /dev/video1
#wf-recorder --muxer=rawvideo --codec=rawvideo --pixel-format=bgr24 -f /dev/stdout | ./tft_driver
#wf-recorder -y --muxer=rawvideo --codec=rawvideo --pixel-format=bgr24 -f /dev/stdout  -F scale=320:340 | ./tft_driver
#wf-recorder -y --muxer=rawvideo --codec=rawvideo --pixel-format=bgr24 -f /dev/stdout | ./tft_driver
wf-recorder -y --muxer=rawvideo --codec=rawvideo --pixel-format=rgb565be -F scale=320:340 -f /dev/stdout | ./tft_driver
#wf-recorder -y --muxer=image2pipe --codec=rawvideo --pixel-format=rgb565be -F "scale=320:340" -f /dev/stdout | ./tft_driver
#wf-recorder -y --muxer=image2pipe --codec=rawvideo --pixel-format=rgb565be -F "scale=320:340" -f /dev/stdout 2>/dev/null | ./tft_driver
