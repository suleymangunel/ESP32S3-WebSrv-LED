<div class="sag">
    <th><img alt="GitHub License" src="https://img.shields.io/github/license/suleymangunel/ePaperV2?label=License&style=plastic"></th>
    <th><img alt="Static Badge" src="https://img.shields.io/badge/Language-C-red?style=plastic"></th>
    <th><img alt="Static Badge" src="https://img.shields.io/badge/Platform-ESP32%2FS3-blue?style=plastic"></th>
    <th><img alt="Static Badge" src="https://img.shields.io/badge/Framework-ESP%E2%94%80IDF-white?style=plastic"></th>
    <th><img alt="Static Badge" src="https://img.shields.io/badge/OS-FreeRTOS-black?style=plastic"></th>
</div>

# ESP32-S3 demo: ILI9341 + LVGL + XPT2046 touch, WS2812 LED, and a minimal HTTP server (ESP-IDF 5.x).

## Description
>A complete ESP-IDF 5.x project that turns your ESP32-S3 board into a small, interactive touchscreen device — with a beautiful LVGL GUI, custom fonts, touch input, a WS2812 LED, and a built-in web server.
It’s a hands-on example of combining display, touch, network, and LED control — everything an embedded UI engineer loves.

Features:
- ILI9341 SPI display (up to 40 MHz)
- LVGL v8.3+ GUI engine
- Custom TTF font support (UTF-8 / Turkish glyphs supported)
- XPT2046 resistive touch controller (IRQ-based)
- WS2812 / NeoPixel LED via RMT peripheral
- HTTP server with endpoints /, /coords, /led
- Wi-Fi connectivity via ESP-IDF
- Fully compatible with ESP-IDF 5.5+


Overview:
When you flash the firmware
- The ESP32-S3 connects to your Wi-Fi network.
- LVGL initializes and displays a UI using your custom TTF font.
- Touch events are read from the XPT2046 controller and displayed.
- The WS2812 LED changes state when you touch or send HTTP requests.
- You can open the device IP (e.g. http://192.168.1.42/) and interact remotely.
It’s a small but complete example of embedded HMI + IoT design.


Hardware Setup:
- Component	Interface	Description
- ESP32-S3-DevKitC-1	—	Main MCU board
- ILI9341 TFT	SPI	LVGL display output
- XPT2046 Touch	SPI + IRQ	Touch input
- WS2812 LED	RMT	One or more LEDs
- Wi-Fi	2.4 GHz	SSID/PASS configurable (codebehind)
Pin assignments can be customized in main.c or via menuconfig.


Custom Font Support:
This demo also includes custom TTF font support for LVGL, allowing you to use your own typography instead of default built-in fonts.
Steps
- Convert a TTF to an LVGL font C file
- The key point here is that whether you’re using LVGL v8 or v9, you must not enable compression when converting the font — always use the --no-compress option.
- Use LVGL’s online font converter for LVGL v9.x or off line converter script "lv_font_conv" for LVGL v8.x
- Select your .ttf file, choose size (e.g. 18 px or 24 px), and character range (0x20–0x7F for ASCII or full Unicode if needed).
- Export as C file (.c) and download it.
- LVGL v8.x cmd:
  ```
  lv_font_conv --font /System/Library/Fonts/Supplemental/Skia.ttf --size 24 --bpp 4 --format 
  lvgl --no-compress --range 0x20-0x7F,0xC7,0xE7,0x11E,0x11F,0x130,0x131,0xD6,0xF6,0x15E,0x15F,0xDC,0xFC --output 
  main/fonts/ink_free_12.c
  ```
- Fix a known field for LVGL v8: remove that line
  ```
  .static_bitmap = 0,
  ```
- Copy the generated .c file (e.g. roboto_18.c) into your main/fonts folder.
- Create header file:
  
  ```
  #ifndef INK_FREE_12_H
  #define INK_FREE_12_H
  #ifdef __cplusplus
  extern "C" {
  #endif
  #include "lvgl.h"
  extern const lv_font_t ink_free_12;
  #ifdef __cplusplus
  } /*extern "C"*/
  #endif
  #endif /*INK_FREE_12_H*/
  ```
- run "idf.py menuconfig" and uncheck "Component config -> LVGL configuration -> uncheck this to use custom lv_conf.h"
- Copy components/lvgl/lv_conf_template.h to components/lvgl/lv_conf.h
- edit lv_conf.h:
  ```
  /* clang-format off */
  #if 0 /*Set it to "1" to enable content"*/
  ```
  change to
  ```
  #if 1 /*Set it to "1" to enable content"*/
  ```
- Update this line:
  ```
  #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(ink_free_12)
  ```
- Add the font to your project (FONTS direcotry and include line
- Include the font in your main source (in this project: main.c):
  ```
  #include "fonts/ink_free_12.h"
  ```
- Use the font in LVGL:
  ```
  lv_obj_t *title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "Hello, ESP32-S3!");
  lv_obj_set_style_text_font(title, &ink_free_12, 0);
  ```
  
License:
Licensed under the Apache License 2.0.
See LICENSE and NOTICE.
SPDX-License-Identifier: Apache-2.0  

Acknowledgments:
Espressif Systems for ESP-IDF
LVGL Kft. for LVGL and lv_font_conv
The open-source community for inspiration and guidance
