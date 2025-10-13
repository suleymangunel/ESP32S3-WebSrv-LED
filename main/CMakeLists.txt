idf_component_register(SRCS "main.c" "led_strip_encoder.c" "fonts/ink_free_12.c"
	INCLUDE_DIRS "." "fonts"
	REQUIRES freertos esp_timer esp_common log heap newlib esp_lvgl_port esp_lcd_ili9341 esp_lcd_touch nvs_flash esp_wifi esp_http_server)

# components/dependencies.lock (opsiyonel - managed components için)
# Bu dosya otomatik oluşturulur, ancak manuel olarak belirtmek için:
# esp_lvgl_port: "*"