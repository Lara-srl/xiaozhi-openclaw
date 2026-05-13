#include "application.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/lv_display.h"
#include "knob.h"
#include "led/single_led.h"
#include "lvgl_theme.h"
#include "misc/lv_event.h"
#include "power_save_timer.h"
#include "sensecap_audio_codec.h"
#include "sscma_camera.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_app_desc.h>
#include <esp_check.h>
#include <esp_console.h>
#include <esp_io_expander_tca95xx_16bit.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_sleep.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <iot_button.h>
#include <iot_knob.h>
#include <nvs_flash.h>

#include "assets/lang_config.h"
#include "ada_ui_manager.h"
#include "mcp_server.h"  // aggiunto per plan11

#define TAG "sensecap_watcher"

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle,
                     int width, int height, int offset_x, int offset_y, bool mirror_x,
                     bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x,
                        mirror_y, swap_xy) {
        // Note: UI customization should be done in SetupUI(), not in constructor
        // to ensure lvgl objects are created before accessing them
    }

    virtual void SetupUI() override {
        // Call parent SetupUI() first to create all lvgl objects
        SpiLcdDisplay::SetupUI();

        DisplayLockGuard lock(this);
        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();
        auto icon_font = lvgl_theme->icon_font()->font();

        lv_obj_set_size(top_bar_, LV_HOR_RES, text_font->line_height);
        lv_obj_set_style_layout(top_bar_, LV_LAYOUT_NONE, 0);
        lv_obj_set_style_pad_top(top_bar_, 10, 0);
        lv_obj_set_style_pad_bottom(top_bar_, 1, 0);

        lv_obj_set_size(status_bar_, LV_HOR_RES, text_font->line_height);
        lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);
        lv_obj_set_style_pad_top(status_bar_, 10, 0);
        lv_obj_set_style_pad_bottom(status_bar_, 1, 0);
        lv_obj_set_y(status_bar_, text_font->line_height);
        lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_IGNORE_LAYOUT);

        // Reparent mute and battery labels to top_bar_ to allow absolute positioning
        lv_obj_set_parent(mute_label_, top_bar_);
        lv_obj_set_parent(battery_label_, top_bar_);
        lv_obj_set_style_margin_left(battery_label_, 0, 0);

        // 针对圆形屏幕调整位置
        //      network  mute  battery     //
        //               status            //
        lv_obj_align(network_label_, LV_ALIGN_TOP_MID, -1.5 * icon_font->line_height, 0);
        lv_obj_align(mute_label_, LV_ALIGN_TOP_MID, 1.0 * icon_font->line_height, 0);
        lv_obj_align(battery_label_, LV_ALIGN_TOP_MID, 2.5 * icon_font->line_height, 0);

        lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_flex_grow(status_label_, 0);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

        lv_obj_align(notification_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_bg_color(low_battery_popup_, lv_color_hex(0xFF0000), 0);
        lv_obj_set_width(low_battery_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(low_battery_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

        // 针对圆形屏幕调整底部对话框位置，避免被圆角遮挡
        lv_obj_set_style_pad_bottom(bottom_bar_, 30, 0);
        lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75);  // 限制宽度，避免文字贴边
        AdaUiManager::GetInstance().Initialize();
    }
};

class SensecapWatcher : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    LcdDisplay* display_;
    std::unique_ptr<Knob> knob_;
    esp_io_expander_handle_t io_exp_handle;
    button_handle_t btns;
    PowerSaveTimer* power_save_timer_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    uint32_t long_press_cnt_;
    enum LongPressZone { kLongPressNone = 0, kLongPressSleep = 1, kLongPressShutdown = 2 };
    LongPressZone long_press_zone_ = kLongPressNone;
    button_driver_t* btn_driver_ = nullptr;
    static SensecapWatcher* instance_;
    SscmaCamera* camera_ = nullptr;
    bool is_sleeping_ = false;
    bool ignore_next_click_ = false;  // ignora il click spurio dopo long-press
    httpd_handle_t http_server_ = nullptr;

    // aggiunto per modficare piano 11
    esp_timer_handle_t led_duration_timer_ = nullptr;
    esp_timer_handle_t ap_deep_sleep_timer_ = nullptr;  // R10.3: 3min AP mode → deep sleep

    void EnterSleepMode() {
        if (is_sleeping_) return;
        is_sleeping_ = true;

        // Mostra "Mi carico..." se USB collegato, altrimenti "Dormo..."
        bool is_charging = (IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
        AdaUiManager::GetInstance().SetState(
            is_charging ? kAdaUiChargingSleep : kAdaUiSleeping);

        if (is_charging) {
            // In carica: display rimane acceso a bassa luminosità
            // così "Mi carico..." è sempre visibile
            vTaskDelay(pdMS_TO_TICKS(200));
            GetBacklight()->SetBrightness(5);
        } else {
            // A batteria: spegni display dopo 800ms
            vTaskDelay(pdMS_TO_TICKS(800));
            GetBacklight()->SetBrightness(0);
        }

        // WiFi modem sleep: riduce consumo ~20mA mantenendo la connessione
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        ESP_LOGI(TAG, "Entered sleep mode (charging=%d, WiFi active)", is_charging ? 1 : 0);
    }

    void ExitSleepMode() {
        if (!is_sleeping_) return;
        is_sleeping_ = false;

        // Ripristina WiFi full power
        esp_wifi_set_ps(WIFI_PS_NONE);

        // Aggiorna UI prima di accendere il backlight: LVGL renderizza idle
        // mentre il display e' ancora spento (o dimmed), evitando flash
        AdaUiManager::GetInstance().SetState(kAdaUiIdle);
        vTaskDelay(pdMS_TO_TICKS(50));  // attendi un ciclo LVGL (~2 frame a 30fps)

        // Ripristina luminosità piena
        GetBacklight()->RestoreBrightness();

        // Reset timer inattività
        power_save_timer_->WakeUp();

        ESP_LOGI(TAG, "Exited sleep mode");
    }

    void EnterDeepSleep() {
        // In carica: non fare deep sleep, vai in display-sleep con "Mi carico..."
        bool is_charging = (IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
        if (is_charging) {
            ESP_LOGI(TAG, "Charging — entering display sleep instead of deep sleep");
            EnterSleepMode();
            return;
        }

        ESP_LOGI(TAG, "Entering deep sleep...");

        // Mostra "Spegnimento..." (già visibile da R3 al trigger)
        AdaUiManager::GetInstance().SetState(kAdaUiShutdown);
        vTaskDelay(pdMS_TO_TICKS(1500));

        // Spegni periferiche via IO expander
        IoExpanderSetLevel(BSP_PWR_LCD, 0);       // LCD power off
        IoExpanderSetLevel(BSP_PWR_AI_CHIP, 0);   // Camera off
        IoExpanderSetLevel(BSP_PWR_CODEC_PA, 0);  // Audio PA off
        GetBacklight()->SetBrightness(0);

        // Wakeup source: GPIO2 = IO expander INT, active-low (bottone premuto)
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);

        // Deep sleep — non ritorna; al wakeup fa boot completo
        esp_deep_sleep_start();
    }

    // R10.3 override: WiFi perso → mostra "Connessione al WiFi..." → deep sleep
    // (il default in wifi_board.cc entrerebbe in AP mode, ma il Watcher va in deep sleep)
    void OnWifiLostTimeout() override {
        ESP_LOGW(TAG, "WiFi lost 30s — showing status then deep sleep");
        AdaUiManager::GetInstance().SetBootStatus("Connessione al WiFi...");
        AdaUiManager::GetInstance().SetState(kAdaUiWifiConnecting);
        vTaskDelay(pdMS_TO_TICKS(3000));
        EnterDeepSleep();
    }

    void InitializePowerSaveTimer() {
        // Timer AP mode: 3 minuti senza interazione → deep sleep
        esp_timer_create_args_t ap_timer_args = {
            .callback = [](void* arg) {
                auto self = static_cast<SensecapWatcher*>(arg);
                ESP_LOGI(TAG, "AP mode 3min timeout — entering deep sleep");
                self->EnterDeepSleep();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ap_deep_sleep",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&ap_timer_args, &ap_deep_sleep_timer_);

        // 30s auto-sleep; nessun auto-shutdown (gestito solo da bottone R3)
        power_save_timer_ = new PowerSaveTimer(-1, 30, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            if (Application::GetInstance().GetDeviceState() == kDeviceStateWifiConfiguring) {
                // AP mode: non dormire subito, avvia timer 3 min per deep sleep
                ESP_LOGI(TAG, "AP mode — starting 3min deep sleep timer");
                esp_timer_start_once(ap_deep_sleep_timer_, 180 * 1000000ULL);
            } else {
                EnterSleepMode();
            }
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ExitSleepMode();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            bool is_charging = (IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
            if (is_charging) {
                ESP_LOGI(TAG, "charging");
                GetBacklight()->SetBrightness(0);
            } else {
                IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
            }
        });
        power_save_timer_->SetEnabled(true);
    }

    void GetMacAddressString(char* out, size_t len) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(out, len, "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // Factory reset condiviso: NVS erase + reboot (usato da HTTP e MCP tool)
    void PerformFactoryReset() {
        ESP_LOGI(TAG, "Factory reset triggered — erasing NVS and rebooting");
        AdaUiManager::GetInstance().SetState(kAdaUiShutdown);
        vTaskDelay(pdMS_TO_TICKS(500));
        nvs_flash_erase();
        esp_restart();
    }

    static esp_err_t FactoryResetHandler(httpd_req_t* req) {
        auto self = static_cast<SensecapWatcher*>(req->user_ctx);

        // Auth: X-Reset-Token deve corrispondere al MAC address del device
        char token[32] = {0};
        httpd_req_get_hdr_value_str(req, "X-Reset-Token", token, sizeof(token));

        char mac_str[18];
        self->GetMacAddressString(mac_str, sizeof(mac_str));

        if (strlen(token) == 0 || strcmp(token, mac_str) != 0) {
            httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid token");
            return ESP_OK;
        }

        // Rispondi prima del reset
        httpd_resp_set_type(req, "application/json");
        const char* resp = "{\"status\":\"resetting\",\"message\":\"Device will reboot in 3 seconds\"}";
        httpd_resp_send(req, resp, strlen(resp));

        // Delay per far arrivare la risposta HTTP, poi reset
        vTaskDelay(pdMS_TO_TICKS(500));
        self->PerformFactoryReset();
        return ESP_OK;
    }

    // Avvia il server HTTP (chiamato solo dopo che lwIP è pronto)
    void StartHttpServer() {
        if (http_server_ != nullptr) return;  // già avviato

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = 80;
        config.max_open_sockets = 4;

        if (httpd_start(&http_server_, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return;
        }

        httpd_uri_t reset_uri = {
            .uri = "/api/factory-reset",
            .method = HTTP_POST,
            .handler = FactoryResetHandler,
            .user_ctx = this,
        };
        httpd_register_uri_handler(http_server_, &reset_uri);

        char mac_str[18];
        GetMacAddressString(mac_str, sizeof(mac_str));
        ESP_LOGI(TAG, "HTTP server started on port 80 (token: %s)", mac_str);
    }

    static void OnGotIpHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
        auto self = static_cast<SensecapWatcher*>(arg);
        self->StartHttpServer();
    }

    // Registra event handler: avvia HTTP server quando WiFi ottiene IP
    void InitializeHttpServer() {
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                   &SensecapWatcher::OnGotIpHandler, this);
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = BSP_GENERAL_I2C_SDA,
            .scl_io_num = BSP_GENERAL_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // pulldown for lcd i2c
        const gpio_config_t io_config = {
            .pin_bit_mask = (1ULL << BSP_TOUCH_I2C_SDA) | (1ULL << BSP_TOUCH_I2C_SCL) |
                            (1ULL << BSP_SPI3_HOST_PCLK) | (1ULL << BSP_SPI3_HOST_DATA0) |
                            (1ULL << BSP_SPI3_HOST_DATA1) | (1ULL << BSP_SPI3_HOST_DATA2) |
                            (1ULL << BSP_SPI3_HOST_DATA3) | (1ULL << BSP_LCD_SPI_CS) |
                            (1UL << DISPLAY_BACKLIGHT_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_config);

        gpio_set_level(BSP_TOUCH_I2C_SDA, 0);
        gpio_set_level(BSP_TOUCH_I2C_SCL, 0);

        gpio_set_level(BSP_LCD_SPI_CS, 0);
        gpio_set_level(DISPLAY_BACKLIGHT_PIN, 0);
        gpio_set_level(BSP_SPI3_HOST_PCLK, 0);
        gpio_set_level(BSP_SPI3_HOST_DATA0, 0);
        gpio_set_level(BSP_SPI3_HOST_DATA1, 0);
        gpio_set_level(BSP_SPI3_HOST_DATA2, 0);
        gpio_set_level(BSP_SPI3_HOST_DATA3, 0);
    }

    esp_err_t IoExpanderSetLevel(uint16_t pin_mask, uint8_t level) {
        return esp_io_expander_set_level(io_exp_handle, pin_mask, level);
    }

    uint8_t IoExpanderGetLevel(uint16_t pin_mask) {
        uint32_t pin_val = 0;
        esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
        pin_mask &= DRV_IO_EXP_INPUT_MASK;
        return (uint8_t)((pin_val & pin_mask) ? 1 : 0);
    }

    void InitializeExpander() {
        esp_err_t ret = ESP_OK;
        esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_001,
                                              &io_exp_handle);

        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_INPUT_MASK, IO_EXPANDER_INPUT);
        ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, IO_EXPANDER_OUTPUT);
        ret |= esp_io_expander_set_level(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, 0);
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_SYSTEM, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_START_UP, 1);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        uint32_t pin_val = 0;
        ret |= esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
        ESP_LOGI(TAG, "IO expander initialized: %x", DRV_IO_EXP_OUTPUT_MASK | (uint16_t)pin_val);

        assert(ret == ESP_OK);
    }

    void OnKnobRotate(bool clockwise) {
        auto codec = GetAudioCodec();
        int current_volume = codec->output_volume();
        int new_volume = current_volume + (clockwise ? -5 : 5);

        // 确保音量在有效范围内
        if (new_volume > 100) {
            new_volume = 100;
            ESP_LOGW(TAG, "Volume reached maximum limit: %d", new_volume);
        } else if (new_volume < 0) {
            new_volume = 0;
            ESP_LOGW(TAG, "Volume reached minimum limit: %d", new_volume);
        }

        codec->SetOutputVolume(new_volume);
        ESP_LOGI(TAG, "Volume changed from %d to %d", current_volume, new_volume);

        // 显示通知前检查实际变化
        if (new_volume != codec->output_volume()) {
            ESP_LOGE(TAG, "Failed to set volume! Expected:%d Actual:%d", new_volume,
                     codec->output_volume());
        }
        GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ": " +
                                       std::to_string(codec->output_volume()));
        power_save_timer_->WakeUp();
    }

    void InitializeKnob() {
        knob_ = std::make_unique<Knob>(BSP_KNOB_A_PIN, BSP_KNOB_B_PIN);
        knob_->OnRotate([this](bool clockwise) {
            ESP_LOGD(TAG, "Knob rotation detected. Clockwise:%s", clockwise ? "true" : "false");
            OnKnobRotate(clockwise);
        });
        ESP_LOGI(TAG, "Knob initialized with pins A:%d B:%d", BSP_KNOB_A_PIN, BSP_KNOB_B_PIN);
    }

    void InitializeButton() {
        // 设置静态实例指针
        instance_ = this;

        // watcher 是通过长按滚轮进行开机的, 需要等待滚轮释放, 否则用户开机松手时可能会误触成单击
        ESP_LOGI(TAG, "waiting for knob button release");
        while (IoExpanderGetLevel(BSP_KNOB_BTN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        button_config_t btn_config = {.long_press_time = 5000, .short_press_time = 0};
        btn_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        btn_driver_->enable_power_save = false;
        btn_driver_->get_key_level = [](button_driver_t* button_driver) -> uint8_t {
            return !instance_->IoExpanderGetLevel(BSP_KNOB_BTN);
        };

        ESP_ERROR_CHECK(iot_button_create(&btn_config, btn_driver_, &btns));

        // Toggle: click per iniziare, click per fermare
        iot_button_register_cb(
              btns, BUTTON_SINGLE_CLICK, nullptr,
              [](void* button_handle, void* usr_data) {
                  auto self = static_cast<SensecapWatcher*>(usr_data);
                  // Ignora il click spurio che arriva al rilascio del long-press
                  if (self->ignore_next_click_) {
                      self->ignore_next_click_ = false;
                      return;
                  }
                  // Se in sleep, il click serve solo a svegliarsi (R4)
                  if (self->is_sleeping_) {
                      self->ExitSleepMode();
                      return;
                  }
                  self->power_save_timer_->WakeUp();
                  auto& app = Application::GetInstance();
                  if (app.GetDeviceState() == kDeviceStateStarting) {
                      self->EnterWifiConfigMode();
                      return;
                  }
                  if (app.GetDeviceState() == kDeviceStateListening) {
                      app.StopListening();
                  } else {
                      app.StartListening();
                  }
              },
              this);

        iot_button_register_cb(
            btns, BUTTON_LONG_PRESS_START, nullptr,
            [](void* button_handle, void* usr_data) {
                auto self = static_cast<SensecapWatcher*>(usr_data);
                self->long_press_cnt_ = 0;
                self->long_press_zone_ = kLongPressSleep;
                AdaUiManager::GetInstance().SetState(kAdaUiSleeping);
                ESP_LOGI(TAG, "Long press: sleep preview (5s)");
            },
            this);

        iot_button_register_cb(
              btns, BUTTON_LONG_PRESS_HOLD, nullptr,
              [](void* button_handle, void* usr_data) {
                  auto self = static_cast<SensecapWatcher*>(usr_data);
                  self->long_press_cnt_++;
                   if (self->long_press_cnt_ == 250) {
                        // 10s: anteprima power-off — rilasciare per spegnere
                        self->long_press_zone_ = kLongPressShutdown;
                        AdaUiManager::GetInstance().SetState(kAdaUiShutdown);
                        ESP_LOGI(TAG, "Long press: shutdown preview (10s)");
                    }
              },
              this);
         iot_button_register_cb(
              btns, BUTTON_LONG_PRESS_UP, nullptr,
              [](void* button_handle, void* usr_data) {
                  auto self = static_cast<SensecapWatcher*>(usr_data);
                  switch (self->long_press_zone_) {
                      case kLongPressShutdown:
                          ESP_LOGI(TAG, "Long press release: shutdown");
                          self->ignore_next_click_ = true;
                          self->EnterDeepSleep();
                          break;
                      case kLongPressSleep:
                          ESP_LOGI(TAG, "Long press release: sleep");
                          self->ignore_next_click_ = true;
                          self->EnterSleepMode();
                          break;
                      case kLongPressNone:
                      default:
                          // Se siamo già in sleep (auto-shutdown ha chiamato EnterDeepSleep),
                          // non resettare lo stato UI
                          if (!self->is_sleeping_) {
                              ESP_LOGI(TAG, "Long press release: cancelled (<7s)");
                              AdaUiManager::GetInstance().SetState(kAdaUiIdle);
                          }
                          break;
                  }
                  self->long_press_zone_ = kLongPressNone;
                  self->long_press_cnt_ = 0;
              },
              this);
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SSCMA SPI bus");
        spi_bus_config_t spi_cfg = {0};

        spi_cfg.mosi_io_num = BSP_SPI2_HOST_MOSI;
        spi_cfg.miso_io_num = BSP_SPI2_HOST_MISO;
        spi_cfg.sclk_io_num = BSP_SPI2_HOST_SCLK;
        spi_cfg.quadwp_io_num = -1;
        spi_cfg.quadhd_io_num = -1;
        spi_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_1;
        spi_cfg.max_transfer_sz = 4095;

        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Initialize QSPI bus");

        spi_bus_config_t qspi_cfg = {0};
        qspi_cfg.sclk_io_num = BSP_SPI3_HOST_PCLK;
        qspi_cfg.data0_io_num = BSP_SPI3_HOST_DATA0;
        qspi_cfg.data1_io_num = BSP_SPI3_HOST_DATA1;
        qspi_cfg.data2_io_num = BSP_SPI3_HOST_DATA2;
        qspi_cfg.data3_io_num = BSP_SPI3_HOST_DATA3;
        qspi_cfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * DRV_LCD_BITS_PER_PIXEL / 8 /
                                   CONFIG_BSP_LCD_SPI_DMA_SIZE_DIV;

        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &qspi_cfg, SPI_DMA_CH_AUTO));
    }

    void Initializespd2010Display() {
        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = BSP_LCD_SPI_CS,
            .dc_gpio_num = -1,
            .spi_mode = 3,
            .pclk_hz = DRV_LCD_PIXEL_CLK_HZ,
            .trans_queue_depth = 2,
            .lcd_cmd_bits = DRV_LCD_CMD_BITS,
            .lcd_param_bits = DRV_LCD_PARAM_BITS,
            .flags =
                {
                    .quad_mode = true,
                },
        };
        spd2010_vendor_config_t vendor_config = {
            .flags =
                {
                    .use_qspi_interface = 1,
                },
        };
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &panel_io_);

        ESP_LOGD(TAG, "Install LCD driver");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = BSP_LCD_GPIO_RST,  // Shared with Touch reset
            .rgb_ele_order = DRV_LCD_RGB_ELEMENT_ORDER,
            .bits_per_pixel = DRV_LCD_BITS_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_spd2010(panel_io_, &panel_config, &panel_);

        esp_lcd_panel_reset(panel_);
        esp_lcd_panel_init(panel_);
        esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel_, true);

        display_ = new CustomLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // 使每次刷新的起始列数索引是4的倍数且列数总数是4的倍数，以满足SPD2010的要求
        lv_display_add_event_cb(
            lv_display_get_default(),
            [](lv_event_t* e) {
                lv_area_t* area = (lv_area_t*)lv_event_get_param(e);
                uint16_t x1 = area->x1;
                uint16_t x2 = area->x2;
                // round the start of area down to the nearest 4N number
                area->x1 = (x1 >> 2) << 2;
                // round the end of area up to the nearest 4M+3 number
                area->x2 = ((x2 >> 2) << 2) + 3;
            },
            LV_EVENT_INVALIDATE_AREA, NULL);
    }

    uint16_t BatterygetVoltage(void) {
        static bool initialized = false;
        static adc_oneshot_unit_handle_t adc_handle;
        static adc_cali_handle_t cali_handle = NULL;
        if (!initialized) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
            };
            adc_oneshot_new_unit(&init_config, &adc_handle);

            adc_oneshot_chan_cfg_t ch_config = {
                .atten = BSP_BAT_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            adc_oneshot_config_channel(adc_handle, BSP_BAT_ADC_CHAN, &ch_config);

            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .chan = BSP_BAT_ADC_CHAN,
                .atten = BSP_BAT_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
                initialized = true;
            }
        }
        if (initialized) {
            int raw_value = 0;
            int voltage = 0;  // mV
            adc_oneshot_read(adc_handle, BSP_BAT_ADC_CHAN, &raw_value);
            adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage);
            voltage = voltage * 82 / 20;
            // ESP_LOGI(TAG, "voltage: %dmV", voltage);
            return (uint16_t)voltage;
        }
        return 0;
    }

    uint8_t BatterygetPercent(bool print = false) {
        int voltage = 0;
        for (uint8_t i = 0; i < 10; i++) {
            voltage += BatterygetVoltage();
        }
        voltage /= 10;
        int percent = (-1 * voltage * voltage + 9016 * voltage - 19189000) / 10000;
        percent = (percent > 100) ? 100 : (percent < 0) ? 0 : percent;
        if (print) {
            printf("voltage: %dmV, percentage: %d%%\r\n", voltage, percent);
        }
        return (uint8_t)percent;
    }

    void InitializeCmd() {
        esp_console_repl_t* repl = NULL;
        esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
        repl_config.max_cmdline_length = 1024;
        repl_config.prompt = "SenseCAP>";

        const esp_console_cmd_t cmd1 = {.command = "reboot",
                                        .help = "reboot the device",
                                        .hint = nullptr,
                                        .func = [](int argc, char** argv) -> int {
                                            esp_restart();
                                            return 0;
                                        },
                                        .argtable = nullptr};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd1));

        const esp_console_cmd_t cmd2 = {
            .command = "shutdown",
            .help = "shutdown the device",
            .hint = nullptr,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void* context, int argc, char** argv) -> int {
                auto self = static_cast<SensecapWatcher*>(context);
                self->GetBacklight()->SetBrightness(0);
                self->IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
                return 0;
            },
            .context = this};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd2));

        const esp_console_cmd_t cmd3 = {
            .command = "battery",
            .help = "get battery percent",
            .hint = NULL,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void* context, int argc, char** argv) -> int {
                auto self = static_cast<SensecapWatcher*>(context);
                self->BatterygetPercent(true);
                return 0;
            },
            .context = this};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd3));

        const esp_console_cmd_t cmd4 = {
            .command = "factory_reset",
            .help = "factory reset and reboot the device",
            .hint = NULL,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void* context, int argc, char** argv) -> int {
                nvs_flash_erase();
                esp_restart();
                return 0;
            },
            .context = this};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd4));

        const esp_console_cmd_t cmd5 = {
            .command = "read_mac",
            .help = "Read mac address",
            .hint = NULL,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void* context, int argc, char** argv) -> int {
                uint8_t mac[6];
                esp_read_mac(mac, ESP_MAC_WIFI_STA);
                printf("wifi_sta_mac: " MACSTR "\n", MAC2STR(mac));
                esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
                printf("wifi_softap_mac: " MACSTR "\n", MAC2STR(mac));
                esp_read_mac(mac, ESP_MAC_BT);
                printf("bt_mac: " MACSTR "\n", MAC2STR(mac));
                return 0;
            },
            .context = this};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd5));

        const esp_console_cmd_t cmd6 = {
            .command = "version",
            .help = "Read version info",
            .hint = NULL,
            .func = NULL,
            .argtable = NULL,
            .func_w_context = [](void* context, int argc, char** argv) -> int {
                auto self = static_cast<SensecapWatcher*>(context);
                auto app_desc = esp_app_get_description();
                const char* region = "UNKNOWN";
#if defined(CONFIG_LANGUAGE_ZH_CN)
                region = "CN";
#elif defined(CONFIG_LANGUAGE_EN_US)
                region = "US";
#elif defined(CONFIG_LANGUAGE_JA_JP)
                region = "JP";
#elif defined(CONFIG_LANGUAGE_ES_ES)
                region = "ES";
#elif defined(CONFIG_LANGUAGE_DE_DE)
                region = "DE";
#elif defined(CONFIG_LANGUAGE_FR_FR)
                region = "FR";
#elif defined(CONFIG_LANGUAGE_IT_IT)
                region = "IT";
#elif defined(CONFIG_LANGUAGE_PT_PT)
                region = "PT";
#elif defined(CONFIG_LANGUAGE_RU_RU)
                region = "RU";
#elif defined(CONFIG_LANGUAGE_KO_KR)
                region = "KR";
#endif
                printf(
                    "{\"type\":0,\"name\":\"VER?\",\"code\":0,\"data\":{\"software\":\"%s\","
                    "\"hardware\":\"watcher xiaozhi agent\",\"camera\":%d,\"region\":\"%s\"}}\n",
                    app_desc->version, self->GetCamera() == nullptr ? 0 : 1, region);
                return 0;
            },
            .context = this};
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd6));

        esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
        ESP_ERROR_CHECK(esp_console_start_repl(repl));
    }

    void InitializeCamera() {
        ESP_LOGI(TAG, "Initialize Camera");

        // !!!NOTE: SD Card use same SPI bus as sscma client, so we need to disable SD card CS pin
        // first
        const gpio_config_t io_config = {
            .pin_bit_mask = (1ULL << BSP_SD_SPI_CS),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t ret = gpio_config(&io_config);
        if (ret != ESP_OK)
            return;

        gpio_set_level(BSP_SD_SPI_CS, 1);

        camera_ = new SscmaCamera(io_exp_handle);
    }

    void InitializeTools() {
        // One-shot timer to auto-restore LED state after duration_ms
        esp_timer_create_args_t timer_args = {
            .callback =
                [](void* arg) {
                    auto led = static_cast<SingleLed*>(Board::GetInstance().GetLed());
                    if (led) {
                        led->OnStateChanged();
                    }
                },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "led_duration",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &led_duration_timer_));

        auto& mcp = McpServer::GetInstance();

        // Tool 1: self.led.set
        mcp.AddTool("self.led.set",
                    "Set the RGB LED color and mode.\n"
                    "Args:\n"
                    "  hex_color: 6-char hex color string (e.g. \"FF0000\" for red)\n"
                    "  mode: \"static\" (solid), \"pulse\" (slow blink), \"blink\" (fast blink). "
                    "Default: \"static\"\n"
                    "  duration_ms: auto-restore after N ms (0 = permanent). Default: 0",
                    PropertyList({Property("hex_color", kPropertyTypeString),
                                  Property("mode", kPropertyTypeString, std::string("static")),
                                  Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000)}),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto hex = properties["hex_color"].value<std::string>();
                        auto mode = properties["mode"].value<std::string>();
                        auto duration = properties["duration_ms"].value<int>();

                        // Validate hex_color: must be exactly 6 hex digits
                        if (hex.length() != 6) {
                            throw std::runtime_error("hex_color must be 6 characters");
                        }
                        for (char c : hex) {
                            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                                throw std::runtime_error("hex_color must contain only hex digits");
                            }
                        }

                        // Validate mode
                        if (mode != "static" && mode != "pulse" && mode != "blink") {
                            throw std::runtime_error("mode must be static, pulse, or blink");
                        }

                        // Parse hex color
                        uint8_t r = (uint8_t)strtol(hex.substr(0, 2).c_str(), nullptr, 16);
                        uint8_t g = (uint8_t)strtol(hex.substr(2, 2).c_str(), nullptr, 16);
                        uint8_t b = (uint8_t)strtol(hex.substr(4, 2).c_str(), nullptr, 16);

                        auto* led = static_cast<SingleLed*>(GetLed());
                        led->SetColor(r, g, b);

                        if (mode == "static") {
                            led->TurnOn();
                        } else if (mode == "pulse") {
                            led->StartContinuousBlink(500);
                        } else {
                            led->StartContinuousBlink(200);
                        }

                        // Auto-restore after duration_ms
                        esp_timer_stop(led_duration_timer_);
                        if (duration > 0) {
                            esp_timer_start_once(led_duration_timer_, (int64_t)duration * 1000);
                        }

                        ESP_LOGI(TAG, "LED set: #%s mode=%s duration=%d", hex.c_str(), mode.c_str(),
                                 duration);
                        return true;
                    });

        // Tool 2: self.haptic.feedback
        mcp.AddTool(
            "self.haptic.feedback",
            "Play a haptic feedback sound pattern through the speaker.\n"
            "Args:\n"
            "  pattern: \"short\" (single tap), \"double\" (double tap), \"long\" (success chime)",
            PropertyList({Property("pattern", kPropertyTypeString)}),
            [](const PropertyList& properties) -> ReturnValue {
                auto pattern = properties["pattern"].value<std::string>();

                std::string_view sound;
                if (pattern == "short") {
                    sound = Lang::Sounds::OGG_VIBRATION;
                } else if (pattern == "double") {
                    sound = Lang::Sounds::OGG_EXCLAMATION;
                } else if (pattern == "long") {
                    sound = Lang::Sounds::OGG_SUCCESS;
                } else {
                    throw std::runtime_error("pattern must be short, double, or long");
                }

                Application::GetInstance().PlaySound(sound);
                ESP_LOGI(TAG, "Haptic feedback: %s", pattern.c_str());
                return true;
            });

        // Tool 3: self.sensor.read
        mcp.AddTool("self.sensor.read",
                    "Read device sensor data. Returns battery percentage, charging status, and "
                    "current volume.",
                    PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                        int level = 0;
                        bool charging = false, discharging = false;
                        GetBatteryLevel(level, charging, discharging);

                        auto* codec = GetAudioCodec();
                        int volume = codec->output_volume();

                        cJSON* json = cJSON_CreateObject();
                        cJSON_AddNumberToObject(json, "battery_pct", level);
                        cJSON_AddBoolToObject(json, "charging", charging);
                        cJSON_AddNumberToObject(json, "volume", volume);
                        return json;
                    });
        // Tool 4: self.audio_player.play                                                                                                                                      
        mcp.AddTool("self.audio_player.play",                                                                                                                                  
                    "Play a sound effect from the device speaker.\n"                                                                                                           
                    "Args:\n"                                                                                                                                                  
                    "  url: sound name: success, vibration, exclamation, popup, welcome",                                                                                    
                  PropertyList({Property("url", kPropertyTypeString)}),                                                                                                      
                  [](const PropertyList& properties) -> ReturnValue {
                      auto name = properties["url"].value<std::string>();                                                                                                    
                                                                                                                                                                             
                      std::string_view sound;                                                                                                                                
                      if (name == "success") {
                          sound = Lang::Sounds::OGG_SUCCESS;
                      } else if (name == "vibration") {
                          sound = Lang::Sounds::OGG_VIBRATION;
                      } else if (name == "exclamation") {
                          sound = Lang::Sounds::OGG_EXCLAMATION;
                      } else if (name == "popup") {
                          sound = Lang::Sounds::OGG_POPUP;
                      } else if (name == "welcome") {
                          sound = Lang::Sounds::OGG_WELCOME;
                      } else {
                          sound = Lang::Sounds::OGG_VIBRATION;
                          ESP_LOGW(TAG, "Unknown sound '%s', using vibration", name.c_str());
                      }

                      Application::GetInstance().PlaySound(sound);
                      ESP_LOGI(TAG, "Playing sound: %s", name.c_str());
                      return true;
                  });

        // Tool 5: self.system.factory_reset
        mcp.AddTool("self.system.factory_reset",
                    "Factory reset the device: erases all settings (WiFi, config) and reboots.\n"
                    "WARNING: This is destructive and irreversible. Only use when explicitly requested.\n"
                    "Args:\n"
                    "  confirm: must be true to proceed",
                    PropertyList({Property("confirm", kPropertyTypeBoolean)}),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto confirm = properties["confirm"].value<bool>();
                        if (!confirm) {
                            throw std::runtime_error("confirm must be true to proceed");
                        }
                        ESP_LOGI(TAG, "Factory reset via MCP tool");
                        PerformFactoryReset();
                        // Non ritorna — il device reboota
                        return true;
                    });

        // Tool 6: self.system.sleep
        mcp.AddTool("self.system.sleep",
                    "Put the device to sleep. Display turns off, WiFi enters modem sleep.\n"
                    "Use when the user says 'dormi', 'vai a dormire', 'sleep'.",
                    PropertyList(),
                    [this](const PropertyList& properties) -> ReturnValue {
                        ESP_LOGI(TAG, "Sleep via MCP tool");
                        EnterSleepMode();
                        return true;
                    });
    }

public:
    SensecapWatcher() {
        ESP_LOGI(TAG, "Initialize Sensecap Watcher");
        InitializePowerSaveTimer();
        InitializeI2c();
        InitializeSpi();
        InitializeExpander();
        InitializeCmd();  // 工厂生产测试使用
        InitializeButton();
        InitializeKnob();
        Initializespd2010Display();
        GetBacklight()->RestoreBrightness();  // 对于不带摄像头的版本，InitializeCamera需要3s,
                                              // 所以先恢复背光亮度
        InitializeCamera();
        InitializeTools();
        InitializeHttpServer();
        // Initialize Ada UI (Astro Bot eyes on IDLE screen)
        //{
        //DisplayLockGuard lock(display_);
        //AdaUiManager::GetInstance().Initialize();
        //}
    }

    virtual AudioCodec* GetAudioCodec() override {
        static SensecapAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7243E_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // 根据
    // https://github.com/Seeed-Studio/OSHW-SenseCAP-Watcher/blob/main/Hardware/SenseCAP_Watcher_v1.0_SCH.pdf
    // RGB LED型号为 ws2813 mini, 连接在GPIO 40，供电电压 3.3v, 没有连接 BIN 双信号线
    // 可以直接兼容SingleLED采用的ws2812
    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            esp_timer_stop(ap_deep_sleep_timer_);
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    virtual void ResetInactivityTimer() override {
        esp_timer_stop(ap_deep_sleep_timer_);
        power_save_timer_->WakeUp();
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = (IoExpanderGetLevel(BSP_PWR_VBUS_IN_DET) == 0);
        discharging = !charging;
        level = (int)BatterygetPercent(false);

        if (discharging != last_discharging) {
            last_discharging = discharging;
            // Timer sempre attivo indipendentemente dalla carica:
            // in carica → sleep mostra "Mi carico..." a 5%
            // a batteria → sleep spegne display

            // USB rimosso durante charging sleep → sveglia normale
            if (discharging && is_sleeping_) {
                ExitSleepMode();
            }
            // USB collegato durante sleep a batteria → passa a charging sleep
            if (charging && is_sleeping_) {
                ESP_LOGI(TAG, "USB plugged during sleep — switching to charging sleep");
                GetBacklight()->SetBrightness(5);
                AdaUiManager::GetInstance().SetState(kAdaUiChargingSleep);
            }
        }
        if (level <= 1 && discharging) {
            ESP_LOGI(TAG, "Battery level is low, shutting down");
            IoExpanderSetLevel(BSP_PWR_SYSTEM, 0);
        }
        return true;
    }

    virtual Camera* GetCamera() override { return camera_; }
};

DECLARE_BOARD(SensecapWatcher);

// 定义静态成员变量
SensecapWatcher* SensecapWatcher::instance_ = nullptr;
