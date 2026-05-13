#include "ada_ui_manager.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include "ada_ui.h"

#define TAG "AdaUI"

void AdaUiManager::Initialize() {
    ESP_LOGI(TAG, "Initializing Ada UI Manager");

    ada_ui_init(NULL);

    // Boot screen first (shown immediately)
    boot_screen_ = screen_boot_create();
    lv_screen_load(boot_screen_);

    // Set title text and font
    if (lvgl_port_lock(100)) {
        lv_obj_t* title = lv_obj_find_by_name(boot_screen_, "title_label");
        if (title) {
            lv_label_set_text(title, "aDa");
            lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
        }
        lv_obj_t* status = lv_obj_find_by_name(boot_screen_, "status_label");
        if (status) {
            lv_obj_add_flag(status, LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
    }

    // Idle screen prepared but NOT loaded yet
    idle_screen_ = screen_idle_create();

    // Listening screen prepared but NOT loaded yet
    listening_screen_ = screen_listening_create();

    // Pulse timer for listening animation
    const esp_timer_create_args_t pulse_args = {
        .callback = PulseTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ada_pulse",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&pulse_args, &pulse_timer_);

    // Blink timer created but NOT started (starts on kAdaUiIdle)
    const esp_timer_create_args_t timer_args = {
        .callback = BlinkTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ada_blink",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &blink_timer_);

    current_state_ = kAdaUiBoot;
    ESP_LOGI(TAG, "Ada UI initialized — BOOT state active");
}

void AdaUiManager::BlinkTimerCallback(void* arg) {
    auto* self = static_cast<AdaUiManager*>(arg);
    if (self->current_state_ != kAdaUiIdle || self->idle_screen_ == nullptr)
        return;

    if (lvgl_port_lock(100)) {
        lv_anim_timeline_t* blink =
            screen_idle_get_timeline(self->idle_screen_, SCREEN_IDLE_TIMELINE_BLINK);
        if (blink) {
            lv_anim_timeline_set_progress(blink, 0);
            lv_anim_timeline_start(blink);
        }
        lvgl_port_unlock();
    }
}

void AdaUiManager::PulseTimerCallback(void* arg) {
    auto* self = static_cast<AdaUiManager*>(arg);
    if (self->current_state_ != kAdaUiListening || self->listening_screen_ == nullptr)
        return;

    if (lvgl_port_lock(100)) {
        lv_anim_timeline_t* pulse =
            screen_listening_get_timeline(self->listening_screen_, SCREEN_LISTENING_TIMELINE_PULSE);
        if (pulse) {
            lv_anim_timeline_set_progress(pulse, 0);
            lv_anim_timeline_start(pulse);
        }
        lvgl_port_unlock();
    }
}

void AdaUiManager::SetState(AdaUiStateCode state) {
    if (state == current_state_)
        return;
    ESP_LOGI(TAG, "State: %d -> %d", current_state_, state);

    AdaUiStateCode prev = current_state_;
    current_state_ = state;

    // Stop dots timer when leaving any state
    if (dots_timer_) {
        esp_timer_stop(dots_timer_);
    }

    if (lvgl_port_lock(100)) {
        lv_obj_t* boot_status = lv_obj_find_by_name(boot_screen_, "status_label");

        switch (state) {
            case kAdaUiBoot:
                if (boot_status)
                    lv_obj_add_flag(boot_status, LV_OBJ_FLAG_HIDDEN);
                if (lv_screen_active() != boot_screen_)
                    lv_screen_load(boot_screen_);
                break;

            case kAdaUiWifiConnecting:
                if (boot_status) {
                    lv_label_set_text(boot_status, "Connessione al WiFi..");
                    lv_obj_remove_flag(boot_status, LV_OBJ_FLAG_HIDDEN);
                }
                if (lv_screen_active() != boot_screen_)
                    lv_screen_load(boot_screen_);
                break;

            case kAdaUiWifiConfig:
                if (boot_status)
                    lv_obj_remove_flag(boot_status, LV_OBJ_FLAG_HIDDEN);
                if (lv_screen_active() != boot_screen_)
                    lv_screen_load(boot_screen_);
                break;

            case kAdaUiActivating:
                if (boot_status) {
                    lv_label_set_text(boot_status, "Attivazione...");
                    lv_obj_remove_flag(boot_status, LV_OBJ_FLAG_HIDDEN);
                }
                if (lv_screen_active() != boot_screen_)
                    lv_screen_load(boot_screen_);
                break;

            case kAdaUiIdle:
                lv_screen_load(idle_screen_);
                if (!lv_color_eq(eye_color_, lv_color_hex(0x00AAFF))) {
                    SetEyeColor(eye_color_);
                }
                break;

            case kAdaUiListening:
                ShowState("Ascolto");
                break;

            case kAdaUiThinking:
                ShowState("Ragiono");
                break;

            case kAdaUiActing:
                ShowState("Eseguo");
                break;

            case kAdaUiSpeaking:
                ShowState("Parlo");
                break;

            case kAdaUiCompaction:
                ShowState("Memorizzo");
                break;
            
            case kAdaUiSleeping:
            case kAdaUiChargingSleep:
            case kAdaUiShutdown:
                // gestiti fuori dal lock via show_boot_text (vedi sotto)
                break;

            default:
                break;
        }
        lvgl_port_unlock();
    }

    // Blink timer: start on IDLE, stop on leave
    if (state == kAdaUiIdle && prev != kAdaUiIdle && blink_timer_) {
        esp_timer_start_periodic(blink_timer_, BLINK_INTERVAL_MS * 1000);
    } else if (prev == kAdaUiIdle && state != kAdaUiIdle && blink_timer_) {
        esp_timer_stop(blink_timer_);
    }

    // Stati con dots animati sul boot_screen — chiamata fuori dal lock LVGL
    if (state == kAdaUiSleeping)           ShowBootState("Dormo");
    else if (state == kAdaUiChargingSleep) ShowBootState("Mi carico");
    else if (state == kAdaUiShutdown)      ShowBootState("Mi spengo");
}

void AdaUiManager::DotsTimerCallback(void* arg) {
    auto* self = static_cast<AdaUiManager*>(arg);
    if (!self->active_label_)
        return;

    if (lvgl_port_lock(100)) {
        self->dots_count_ = (self->dots_count_ + 1) % 4;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%.*s", self->base_text_, self->dots_count_, "...");
        lv_label_set_text(self->active_label_, buf);
        lvgl_port_unlock();
    }
}

void AdaUiManager::ShowBootState(const char* text) {
    if (!boot_screen_) return;

    if (lvgl_port_lock(100)) {
        lv_obj_t* status = lv_obj_find_by_name(boot_screen_, "status_label");
        if (status) {
            lv_label_set_text(status, text);
            lv_obj_remove_flag(status, LV_OBJ_FLAG_HIDDEN);
            active_label_ = status;
        }
        if (lv_screen_active() != boot_screen_)
            lv_screen_load(boot_screen_);
        lvgl_port_unlock();
    }

    base_text_ = text;
    dots_count_ = 0;

    if (!dots_timer_) {
        const esp_timer_create_args_t args = {
            .callback = DotsTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ada_dots",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&args, &dots_timer_);
    }
    esp_timer_start_periodic(dots_timer_, 400000);
}

void AdaUiManager::SetBootStatus(const char* text) {
    if (!boot_screen_)
        return;
    if (lvgl_port_lock(100)) {
        lv_obj_t* status = lv_obj_find_by_name(boot_screen_, "status_label");
        if (status) {
            lv_label_set_text(status, text);
            lv_obj_remove_flag(status, LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
    }
}

void AdaUiManager::ShowState(const char* text) {
    // Create screen_state on first use
    if (!state_screen_) {
        state_screen_ = screen_state_create();
        state_label_ = lv_obj_find_by_name(state_screen_, "status_text");
    }

    base_text_ = text;
    dots_count_ = 0;
    active_label_ = state_label_;
    lv_label_set_text(state_label_, text);
    lv_screen_load(state_screen_);

    // Create dots timer on first use
    if (!dots_timer_) {
        const esp_timer_create_args_t args = {
            .callback = DotsTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ada_dots",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&args, &dots_timer_);
    }
    esp_timer_start_periodic(dots_timer_, 400000);
}

void AdaUiManager::SetEyeColor(lv_color_t color) {
    eye_color_ = color;
    if (lvgl_port_lock(100)) {
        lv_obj_t* screen = lv_screen_active();
        lv_obj_t* left = lv_obj_find_by_name(screen, "left_eye");
        lv_obj_t* right = lv_obj_find_by_name(screen, "right_eye");
        if (left)
            lv_obj_set_style_bg_color(left, color, 0);
        if (right)
            lv_obj_set_style_bg_color(right, color, 0);
        lvgl_port_unlock();
    }
}