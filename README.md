# cheepy_pi

A budget-friendly, fully opensource DIY mini Cyberdeck powered by Raspberry Pi Model B, featuring a custom BQ25895+TPS61236P UPS, 3 cheap ST7789 display and audio system.

<p align="center">
  <img src="Image/rear3dView.png" alt="Rear 3D View" width="45%" />
</p>

## Features
* **Processor:** Powered by Raspberry Pi Model B.
* **Power System:** Custom UPS utilizing BQ25895 + TPS61236P. It integrates a **CH220K** fast-charging sink controller at the input to negotiate up to a **12V input voltage** from Type-C PD charger, significantly boosting input power and enabling faster charging/stable high-load operation when plugged in.
* **Display System:** 
  * Triple (3x) low-cost ST7789 1.9" main screens.
  * An extra 2.25" ST7789 display dedicated to monitoring and displaying real-time battery status, independently managed by an onboard RP2040 microcontroller.
* **Input:** Integrated classic Blackberry Q20 keyboard, also driven and managed by the RP2040 to act as a custom USB HID device.

* **Audio:** Integrated audio system.
  
##  Current Status & Testing
* **Hardware Status:** The project is currently in the prototype stage. I am waiting for the PCBs and components to arrive for soldering, hardware debugging, and error verification. Updates will be posted as soon as testing is complete!
<p align="center">
  <img src="Image/testing.jpg" alt="Testing" width="45%" />
</p>

* **Software/Driver Notice:** 
  The current driver for the triple (3x) ST7789 display system **only supports the latest Raspberry Pi OS** and other Linux distributions utilizing the ́**Wayland** display server protocol. Traditional X11/Xorg environments are not supported.
  
* **Update 31/7/2026:**
  * I’ve tested the display system and it works great! I pushed the SPI clock speed up to 80 MHz, and it still runs smoothly, delivering a solid 60 FPS experience.
  * Still, I'm struggling with testing the keyboard system because of soldering tiny components by hand. I'm also still writing the firmware for it. I'll update as soon as I can!
  <p align="center">
  <img src="Image/display_system.jpg" alt="Testing" width="35%" />
  </p>
* **Update 2/8/2026
  * I wrote firmware for an RP2040 system to drive 4th ST7789V 2.25" display and it worked perfectly! I'm still planning to develop more interesting features, such as displaying GIFs, animations, and dashboards. To do this, I use a Python script to convert images to the exact aspect ratio and RGB565 color format. For now, the python code is still a bit stupid and single-purpose, so it needs more work. I'll update the RP2040 firmware and Python converter code once I perfected them (NAH, usable is just enough for upload to you haha!!!!).
  * The firmware for rp2040 system to control bbq20 keyboard worked, still it's  not stable. I'm struggling with trackpad i2c communication, so i made a simple version off mouse using the A W D S to move mouse (just a temporary workaround, I'll soon find out how the i2c trackpad work btw).
  <p align="center">
  <img src="Image/4thdisplay_system.jpg" alt="Testing" width="35%" />
  </p>
