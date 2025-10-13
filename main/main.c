// main.c – ESP32-S3 + ILI9341 (SPI) + LVGL + WiFi Web Server
// Coordinate display and LED control via buttons on the web page

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "fonts/ink_free_12.h"

// WiFi & HTTP server
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

// WS2812 RGB LED
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "esp_random.h"

#define TAG "ILI9341_DEMO"

// WiFi settings
#define WIFI_SSID "GUNEL"
#define WIFI_PASS "51@Sema84"

// LCD pin definitions
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10
#define PIN_NUM_DC    8
#define PIN_NUM_RST   9
#define PIN_NUM_BCKL  7

// Touch pin definitions
#define TP_CS   5
#define TP_IRQ  14

// WS2812 RGB LED pin definitions
#define LED_GPIO 48  // There may be an 8 on your ESP32 board, check it
#define RMT_RES_HZ (10 * 1000 * 1000)  // 10 MHz
#define LED_COUNT 1  // Single WS2812

#define LCD_HRES 240 //320: For horizontal display (CH01)
#define LCD_VRES 320 //240: For horizontal display (CH01)

#define XPT2046_CMD_Y 0x90

static esp_lcd_touch_handle_t tp = NULL;
static volatile uint16_t g_touch_x = 0, g_touch_y = 0;
static volatile bool g_touch_pressed = false;
static esp_lcd_panel_io_handle_t tp_io = NULL;
static QueueHandle_t gpio_evt_queue = NULL;
static lv_obj_t *coord_label = NULL;
static httpd_handle_t server = NULL;

static lv_obj_t *label = NULL;  // WiFi state label
static char ip_address_str[32] = "IP: Waiting...";  // IP address string

// WS2812 RMT handle
static rmt_channel_handle_t rmt_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// WS2812 LED control
static void set_led_state(bool on) {
    uint8_t grb[3 * LED_COUNT] = {0};
    
    if (on) {
        // RED: GRB -> (G=0, R=50, B=0)
        grb[0] = 0;   // Green
        grb[1] = 50;  // Red
        grb[2] = 0;   // Blue
    } else {
        // OFF: (0,0,0)
        grb[0] = grb[1] = grb[2] = 0;
    }
    
    rmt_transmit_config_t tx_conf = { .loop_count = 0 };
    rmt_transmit(rmt_chan, led_encoder, grb, sizeof(grb), &tx_conf);
    ESP_LOGI(TAG, "LED %s", on ? "ON" : "OFF");
}

// Web page HTML (JavaScript mouse/touch events)
static const char* html_page = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<title>Touch Coordinate</title>"
"<style>"
"body{font-family:Arial;text-align:center;margin-top:50px;background:#f0f0f0}"
"h1{color:#333}"
".coord{font-size:48px;color:#0066cc;margin:30px;min-height:60px}"
"button{font-size:24px;padding:20px 40px;background:#4CAF50;color:white;"
"border:none;border-radius:8px;cursor:pointer;margin:20px;user-select:none;}"
"button:hover{background:#45a049}"
"button:active{background:#3d8b40}"
"button.pressed{background:#d32f2f;}"
".status{font-size:18px;color:#666;margin-top:20px}"
"</style>"
"</head><body>"
"<h1>ESP32-S3 Touch Coordinates</h1>"
"<button id='btn' "
"onmousedown='btnPress()' onmouseup='btnRelease()' onmouseleave='btnRelease()' "
"ontouchstart='btnPress()' ontouchend='btnRelease()' ontouchcancel='btnRelease()'>"
"Press and Hold – LED On"
"</button>"
"<div class='coord' id='coordX'>X: --</div>"
"<div class='coord' id='coordY'>Y: --</div>"
"<div class='status' id='status'>Press button...</div>"
"<script>"
"let isPressed = false;"
"function btnPress(){"
"  if(isPressed) return;"
"  isPressed = true;"
"  document.getElementById('btn').classList.add('pressed');"
"  document.getElementById('status').innerText='LED is ON...';"
"  fetch('/led?state=on');"
"  fetch('/coords')"
"  .then(r=>r.json())"
"  .then(data=>{"
"    document.getElementById('coordX').innerText='X: '+data.x;"
"    document.getElementById('coordY').innerText='Y: '+data.y;"
"  });"
"}"
"function btnRelease(){"
"  if(!isPressed) return;"
"  isPressed = false;"
"  document.getElementById('btn').classList.remove('pressed');"
"  document.getElementById('status').innerText='LED söndü';"
"  fetch('/led?state=off');"
"}"
"</script>"
"</body></html>";

// Main page handler
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

// Coordinate JSON endpoint
static esp_err_t coords_handler(httpd_req_t *req) {
    char response[256];
    const char* status = g_touch_pressed ? "Touch Detected" : "No Touch";
    
    snprintf(response, sizeof(response), 
             "{\"x\":%d,\"y\":%d,\"status\":\"%s\"}", 
             (int)g_touch_x, (int)g_touch_y, status);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// LED control endpoint
static esp_err_t led_handler(httpd_req_t *req) {
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char state_param[8];
        if (httpd_query_key_value(query, "state", state_param, sizeof(state_param)) == ESP_OK) {
            if (strcmp(state_param, "on") == 0) {
                set_led_state(true);
                ESP_LOGI(TAG, "LED turned on");
            } else if (strcmp(state_param, "off") == 0) {
                set_led_state(false);
                ESP_LOGI(TAG, "LED turned off");
            }
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP Adress: " IPSTR, IP2STR(&event->ip_info.ip));

        // Store IP address in string
        snprintf(ip_address_str, sizeof(ip_address_str), 
                 "IP: " IPSTR, IP2STR(&event->ip_info.ip));

                 // Update LVGL label
        if (label != NULL) {
            lvgl_port_lock(0);
            lv_label_set_text(label, ip_address_str);
            lvgl_port_unlock();
        }
        
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        
        if (httpd_start(&server, &config) == ESP_OK) {
            httpd_uri_t root = {
                .uri = "/",
                .method = HTTP_GET,
                .handler = root_handler
            };
            httpd_register_uri_handler(server, &root);
            
            httpd_uri_t coords = {
                .uri = "/coords",
                .method = HTTP_GET,
                .handler = coords_handler
            };
            httpd_register_uri_handler(server, &coords);
            
            httpd_uri_t led = {
                .uri = "/led",
                .method = HTTP_GET,
                .handler = led_handler
            };
            httpd_register_uri_handler(server, &led);
            
            ESP_LOGI(TAG, "Web server başlatıldı!");
        }
    }
}

// WiFi on
static void wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

static inline void xpt2046_rearm_irq(void) {
    if (!tp_io) return;
    esp_lcd_panel_io_tx_param(tp_io, XPT2046_CMD_Y, NULL, 0);
    uint8_t pad[2] = {0,0};
    esp_lcd_panel_io_tx_param(tp_io, 0x00, pad, sizeof(pad));
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    gpio_intr_disable(TP_IRQ);
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void touch_task(void* arg) {
    uint32_t io_num;
    static uint32_t last_touch_time = 0;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            uint32_t current_time = xTaskGetTickCount();
            
            if (io_num == TP_IRQ && gpio_get_level(TP_IRQ) == 0) {
                uint16_t x = 0, y = 0;
                uint8_t count = 0;
                
                if ((current_time - last_touch_time) > pdMS_TO_TICKS(100)) {
                    lvgl_port_lock(pdMS_TO_TICKS(200));
                    
                    if (esp_lcd_touch_read_data(tp) == ESP_OK) {
                        if (esp_lcd_touch_get_coordinates(tp, &x, &y, NULL, &count, 1) && count > 0) {
                            g_touch_pressed = true;
                            g_touch_x = x;
                            g_touch_y = y;
                            last_touch_time = current_time;
                            
                            char buf[32];
                            snprintf(buf, sizeof(buf), "x=%u y=%u", (unsigned)x, (unsigned)y);
                            lv_label_set_text(coord_label, buf);
                        }
                    }
                    lvgl_port_unlock();
                }
                xpt2046_rearm_irq();
            }
            gpio_intr_enable(TP_IRQ);
        }
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    if (g_touch_pressed && gpio_get_level(TP_IRQ) == 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = g_touch_x;
        data->point.y = g_touch_y;
    } else {
        g_touch_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void app_main(void) {
    // Start NVS (Requires for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    // WS2812 RGB LED RMT set
    ESP_LOGI(TAG, "WS2812 LED başlatılıyor (GPIO %d)...", LED_GPIO);
    
    // 1) Create RMT TX channel
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RES_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags = { .with_dma = false },
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &rmt_chan));
    
    // 2) Create encoder for WS2812
    led_strip_encoder_config_t enc_cfg = {
        .resolution = RMT_RES_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&enc_cfg, &led_encoder));
    
    // 3) Activate channel
    ESP_ERROR_CHECK(rmt_enable(rmt_chan));
    
    // Turn off the LED at startup
    uint8_t grb[3] = {0, 0, 0};
    rmt_transmit_config_t tx_conf = { .loop_count = 0 };
    rmt_transmit(rmt_chan, led_encoder, grb, sizeof(grb), &tx_conf);
    
    ESP_LOGI(TAG, "WS2812 LED hazır!");
    
    // WiFi on
    wifi_init();
    
    // SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_HRES * 40 * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Panel IO
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    // ILI9341 panel
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));

    // Backlight
    gpio_config_t bk = { .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = 1ULL << PIN_NUM_BCKL };
    gpio_config(&bk);
    gpio_set_level(PIN_NUM_BCKL, 1);

    // LVGL port
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_HRES * 40,
        .double_buffer = true,
        .hres = LCD_HRES,
        .vres = LCD_VRES,
        //.rotation = { .swap_xy = true, .mirror_x = false, .mirror_y = false }, // For horizontal dispaly (CH01)
        .rotation = { .swap_xy = false, .mirror_x = true, .mirror_y = false },
        .monochrome = false,
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    lv_disp_set_default(disp);

    // IRQ pin config
    gpio_config_t irq_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << TP_IRQ),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&irq_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
    gpio_isr_handler_add(TP_IRQ, gpio_isr_handler, (void*) TP_IRQ);

    // Touch panel IO
    esp_lcd_panel_io_spi_config_t tp_io_cfg = {
        .dc_gpio_num = -1,
        .cs_gpio_num = TP_CS,
        .pclk_hz = 1 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_HRES,
        .y_max = LCD_VRES,
        .rst_gpio_num = -1,
        .int_gpio_num = TP_IRQ,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags  = { .swap_xy = true, .mirror_x = false, .mirror_y = false }
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_cfg, &tp));

    xpt2046_rearm_irq();

    // LVGL input device
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    gpio_intr_enable(TP_IRQ);

    ESP_LOGI(TAG, "=== FONT DEBUG ===");
    ESP_LOGI(TAG, "LVGL Version: %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    ESP_LOGI(TAG, "ink_free_12 pointer: %p", (void*)&ink_free_12);
    ESP_LOGI(TAG, "ink_free_12.line_height: %d", ink_free_12.line_height);
    ESP_LOGI(TAG, "ink_free_12.base_line: %d", ink_free_12.base_line);
    ESP_LOGI(TAG, "ink_free_12.get_glyph_dsc: %p", (void*)ink_free_12.get_glyph_dsc);
    ESP_LOGI(TAG, "ink_free_12.get_glyph_bitmap: %p", (void*)ink_free_12.get_glyph_bitmap);
    
    
    // LVGL UI
    lvgl_port_lock(0);
    label = lv_label_create(lv_scr_act());
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(label, &ink_free_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, ip_address_str);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_pos(label, 10, 125);

    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_obj_t *scr2 = lv_scr_act();
    lv_obj_set_style_bg_color(scr2, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr2, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label2, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label2, "Font Monserrat");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_pos(label2, 60, 150);

    
    coord_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(coord_label, lv_color_white(), 0);
    lv_label_set_text(coord_label, "x=-- y=--");
    lv_obj_set_pos(coord_label, 4, 4);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "System Ready!");
    ESP_LOGI(TAG, "Enter the IP address in your browser");
}
