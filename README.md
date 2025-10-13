# ESP32S3-ili9341-lvgl-touch-ws2812-webserver-custfont

ESP32-S3 LVGL Touch & Web Demo
A complete ESP-IDF 5.x project that turns your ESP32-S3 board into a small, interactive touchscreen device — with a beautiful LVGL GUI, custom fonts, touch input, a WS2812 LED, and a built-in web server.
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
- Use LVGL’s online font converter for LVGL v9.x or off line converter script "lv_font_conv" for LVGL v8.x
- Select your .ttf file, choose size (e.g. 18 px or 24 px), and character range (0x20–0x7F for ASCII or full Unicode if needed).
- Export as C file (.c) and download it.
- LVGL v8.x cmd: lv_font_conv --font /System/Library/Fonts/Supplemental/Skia.ttf --size 24 --bpp 4 --format 
lvgl --no-compress --range 0x20-0x7F,0xC7,0xE7,0x11E,0x11F,0x130,0x131,0xD6,0xF6,0x15E,0x15F,0xDC,0xFC --output 
main/fonts/ink_free_12.c
- Add the font to your project (FONTS direcotry and include line
  
Copy the generated .c file (e.g. roboto_18.c) into your main/ folder.
Include it in your code:
