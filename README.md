# ESP32S3-ili9341-lvgl-touch-ws2812-webserver-custfont

ESP32-S3 LVGL Touch & Web Demo
A complete ESP-IDF 5.x project that turns your ESP32-S3 board into a small, interactive touchscreen device — with a beautiful LVGL GUI, custom fonts, touch input, a WS2812 LED, and a built-in web server.
It’s a hands-on example of combining display, touch, network, and LED control — everything an embedded UI engineer loves.

Features
ILI9341 SPI display (up to 40 MHz)
LVGL v8.3+ GUI library integration
Custom font support (TTF → LVGL C arrays, UTF-8 ready)
XPT2046 resistive touch controller with IRQ-based detection
WS2812 (NeoPixel) LED control via RMT peripheral
Built-in HTTP server with REST-like endpoints (/, /coords, /led)
Wi-Fi connectivity with configurable SSID/PASS
Fully compatible with ESP-IDF v5.5+

Project Overview
When you flash the project:
The ESP32-S3 connects to your Wi-Fi network.
LVGL initializes and draws a small GUI on the TFT display, using your custom font.
Touch input is read from the XPT2046 controller via SPI interrupt.
The WS2812 LED turns on/off from both the screen and the HTTP interface.
You can open your ESP32’s IP address in a browser and control it remotely.
This project demonstrates how to mix UI, sensors, and networking cleanly under ESP-IDF.

Hardware Setup
Component	Interface	Description
ESP32-S3-DevKitC-1 (N16R8)	—	Main MCU board
ILI9341 TFT (2.4” – 2.8”)	SPI	For LVGL display output
XPT2046 Touch Controller	SPI + IRQ	Shares SPI bus or uses second host
WS2812 LED	RMT	Single LED or LED strip
Wi-Fi	2.4 GHz	Set SSID/PASS via codebehid

Custom Font Support
This demo also includes custom TTF font support for LVGL, allowing you to use your own typography instead of default built-in fonts.
Steps
Convert a TTF to an LVGL font C file
Use LVGL’s online font converter
Select your .ttf file, choose size (e.g. 18 px or 24 px), and character range (0x20–0x7F for ASCII or full Unicode if needed).
Export as C file (.c) and download it.
Add the font to your project
Copy the generated .c file (e.g. roboto_18.c) into your main/ folder.
Include it in your code:
