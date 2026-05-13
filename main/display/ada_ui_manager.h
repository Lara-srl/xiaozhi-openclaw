#ifndef ADA_UI_MANAGER_H
#define ADA_UI_MANAGER_H

#include <esp_timer.h>
#include <lvgl.h>
#include "screen_state_gen.h"

enum AdaUiStateCode {
    kAdaUiBoot = 0,
    kAdaUiWifiConnecting = 10,
    kAdaUiWifiConfig = 20,
    kAdaUiActivating = 50,
    kAdaUiIdle = 100,
    kAdaUiListening = 200,
    kAdaUiThinking = 300,
    kAdaUiActing = 400,
    kAdaUiSpeaking = 500,
    kAdaUiCompaction = 600,
    kAdaUiSleeping = 800,        // Display off, WiFi attivo
    kAdaUiChargingSleep = 810,   // In carica + sleep: display off, WiFi attivo
    kAdaUiShutdown = 900,
};

class AdaUiManager {
public:
    static AdaUiManager& GetInstance() {
        static AdaUiManager instance;
        return instance;
    }
    void Initialize();
    // Metodo pubblico:
    void ShowState(const char* text);
    void ShowBootState(const char* text);
    void SetState(AdaUiStateCode state);
    AdaUiStateCode GetState() const { return current_state_; }
    void SetBootStatus(const char* text);
    void SetEyeColor(lv_color_t color);
    

private:
    AdaUiManager() = default;

    // Aggiungere membro:
    lv_obj_t* state_screen_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* active_label_ = nullptr;  // label corrente animata dal dots_timer
    esp_timer_handle_t dots_timer_ = nullptr;
    int dots_count_ = 0;
    const char* base_text_ = nullptr;

    lv_obj_t* boot_screen_ = nullptr;
    lv_obj_t* idle_screen_ = nullptr;

    lv_obj_t* listening_screen_ = nullptr;
    esp_timer_handle_t pulse_timer_ = nullptr;

    
    lv_color_t eye_color_ = lv_color_hex(0x00AAFF);  // default ADA_BLUE     

    esp_timer_handle_t blink_timer_ = nullptr;
    AdaUiStateCode current_state_ = kAdaUiBoot;
    static constexpr int BLINK_INTERVAL_MS = 2000;
    static void BlinkTimerCallback(void* arg);
    static void PulseTimerCallback(void* arg);
    static void DotsTimerCallback(void* arg);    
 
};

#endif