# Guida: Macchina a Stati Ada — SenseCAP Watcher

> Riferimento: branch `feat/plan-12`, R1–R10 (2026-05-09)

## Panoramica

Ada ha **due sistemi di stati paralleli** che collaborano:

1. **DeviceState** (`application.h`) — stato logico dell'applicazione (WiFi, audio, protocollo)
2. **AdaUiStateCode** (`ada_ui_manager.h`) — stato visivo del display (cosa vede l'utente)

Più un livello hardware:

3. **Power state** (`sensecap_watcher.cc`) — sleep/wake/deep sleep del dispositivo

---

## 1. Stati UI (AdaUiStateCode)

### Enum completo

```cpp
enum AdaUiStateCode {
    kAdaUiBoot           = 0,     // Boot iniziale
    kAdaUiWifiConnecting = 10,    // Tentativo connessione WiFi
    kAdaUiWifiConfig     = 20,    // AP mode (config WiFi)
    kAdaUiActivating     = 50,    // Attivazione server
    kAdaUiIdle           = 100,   // Occhi Astro Bot
    kAdaUiListening      = 200,   // Registrazione audio
    kAdaUiThinking       = 300,   // LLM in elaborazione
    kAdaUiActing         = 400,   // Tool MCP in esecuzione
    kAdaUiSpeaking       = 500,   // TTS in riproduzione
    kAdaUiCompaction     = 600,   // Salvataggio memoria sessione
    kAdaUiSleeping       = 800,   // Sleep a batteria
    kAdaUiChargingSleep  = 810,   // Sleep in carica
    kAdaUiShutdown       = 900,   // Spegnimento/deep sleep
};
```

### Testi mostrati sul display

| Stato UI               | Schermo        | Testo display                | Animazione           |
| ---------------------- | -------------- | ---------------------------- | -------------------- |
| `kAdaUiBoot`           | `boot_screen`  | **"aDa"** (titolo, font 48)  | Nessuna              |
| `kAdaUiWifiConnecting` | `boot_screen`  | **"Connessione al WiFi.."**  | Statico              |
| `kAdaUiWifiConfig`     | `boot_screen`  | **"Connettiti a: aDa_XXXX"** | Statico              |
| `kAdaUiActivating`     | `boot_screen`  | **"Attivazione..."**         | Statico              |
| `kAdaUiIdle`           | `idle_screen`  | Occhi Astro Bot              | Blink ogni 2s        |
| `kAdaUiListening`      | `state_screen` | **"Ascolto..."**             | Dots animati (400ms) |
| `kAdaUiThinking`       | `state_screen` | **"Ragiono..."**             | Dots animati (400ms) |
| `kAdaUiActing`         | `state_screen` | **"Eseguo..."**              | Dots animati (400ms) |
| `kAdaUiSpeaking`       | `state_screen` | **"Parlo..."**               | Dots animati (400ms) |
| `kAdaUiCompaction`     | `state_screen` | **"Memorizzo..."**           | Dots animati (400ms) |
| `kAdaUiSleeping`       | `boot_screen`  | **"Dormo..."**               | Dots animati (400ms) |
| `kAdaUiChargingSleep`  | `boot_screen`  | **"Mi carico..."**           | Dots animati (400ms) |
| `kAdaUiShutdown`       | `boot_screen`  | **"Mi spengo..."**           | Dots animati (400ms) |

**Nota**: i testi con `...` sono animati — i puntini ciclano: `""` → `"."` → `".."` → `"..."` ogni 400ms.

### Schermi LVGL

| Schermo            | Contenuto                                                        |
| ------------------ | ---------------------------------------------------------------- |
| `boot_screen`      | Titolo "aDa" (font 48) + `status_label` (nascosto di default)    |
| `idle_screen`      | Occhi Astro Bot con blink periodico + colore occhi configurabile |
| `state_screen`     | `status_text` label centrata con dots animati                    |
| `listening_screen` | Animazione pulse per feedback audio                              |

---

## 2. Stati Device (kDeviceState)

| DeviceState                   | Descrizione                      | UI corrispondente        |
| ----------------------------- | -------------------------------- | ------------------------ |
| `kDeviceStateStarting`        | Boot, prima di WiFi              | `kAdaUiBoot`             |
| `kDeviceStateWifiConfiguring` | AP mode attiva                   | `kAdaUiWifiConfig`       |
| `kDeviceStateActivating`      | Connesso, attivazione protocollo | `kAdaUiActivating`       |
| `kDeviceStateIdle`            | Pronto, in attesa                | `kAdaUiIdle`             |
| `kDeviceStateConnecting`      | Audio channel in apertura        | `kAdaUiIdle` (invariato) |
| `kDeviceStateListening`       | Registra audio utente            | `kAdaUiListening`        |
| `kDeviceStateSpeaking`        | TTS in riproduzione              | `kAdaUiSpeaking`         |

---

## 3. Stati Power (SenseCAP Watcher)

| Stato power       | Display                  | WiFi        | WS       | CPU      | Wake                 |
| ----------------- | ------------------------ | ----------- | -------- | -------- | -------------------- |
| **Attivo**        | ON 100%                  | Full        | Connesso | Full     | —                    |
| **Display sleep** | OFF (batt) o 5% (carica) | Modem sleep | Connesso | Full     | Bottone, knob, MCP   |
| **Deep sleep**    | OFF                      | OFF         | Morto    | RTC only | Solo bottone (GPIO2) |

---

## 4. Diagramma transizioni completo

```
                         ┌──────────────────────────────────────────────────────────┐
                         │                        BOOT                             │
                         │  Display: "aDa"                                         │
                         │  kAdaUiBoot + kDeviceStateStarting                      │
                         └────────────┬──────────────────┬─────────────────────────┘
                                      │                  │
                              ha SSID salvati      no SSID salvati
                                      │                  │
                                      ▼                  ▼
                         ┌────────────────────┐  ┌──────────────────────┐
                         │  WIFI CONNECTING   │  │    AP MODE (config)  │
                         │  "Connessione al   │  │  "Connettiti a:      │
                         │   WiFi.."          │  │   aDa_XXXX"          │
                         │  kAdaUiWifiConnect │  │  kAdaUiWifiConfig    │
                         └───────┬──────┬─────┘  └───────┬──────────────┘
                                 │      │                │
                          connesso  timeout 60s    config ricevuta
                                 │      │                │
                                 │      └──────►─────────┤
                                 ▼                       ▼
                         ┌────────────────────┐  ┌──────────────────────┐
                         │    ACTIVATING      │  │  WIFI CONNECTING     │
                         │  "Attivazione..."  │  │  (riprova con nuovi  │
                         │  kAdaUiActivating  │  │   credenziali)       │
                         └───────┬────────────┘  └──────────────────────┘
                                 │
                         attivazione OK
                                 │
                                 ▼
                    ┌─────────────────────────────┐
                    │           IDLE               │
                    │   Occhi Astro Bot             │◄──────────────────────┐
                    │   Blink ogni 2s              │                       │
                    │   kAdaUiIdle                 │                       │
                    └──┬──────┬──────────┬────┬───┘                       │
                       │      │          │    │                            │
                  click │  WiFi perso    │    │ 30s inattività             │
                  bottone│  30s timer    │    │                            │
                       │      │     MCP tool  │                            │
                       ▼      ▼          ▼    ▼                            │
             ┌──────────┐ ┌────────┐ ┌──────┐ ┌──────────┐                │
             │LISTENING │ │WiFi    │ │ACTING│ │  SLEEP   │                │
             │"Ascolto."│ │perso   │ │"Ese- │ │"Dormo.." │                │
             │          │ │(vedi   │ │guo." │ │  (batt)  │                │
             └────┬─────┘ │sotto)  │ └──┬───┘ │"Mi cari- │                │
                  │       └────────┘    │     │ co.."    │                │
             STT done                tool done│ (carica) │                │
                  │                     │     └────┬─────┘                │
                  ▼                     │          │                      │
             ┌──────────┐               │     click bottone               │
             │THINKING  │               │     o knob                      │
             │"Ragiono."│               │          │                      │
             └────┬─────┘               │          ▼                      │
                  │                     │     ┌──────────┐                │
             LLM done                  │     │  WAKE    │                │
                  │                     │     │→ Idle    │────────────────┘
                  ▼                     │     └──────────┘
             ┌──────────┐              │
             │SPEAKING  │◄─────────────┘
             │"Parlo.." │
             └────┬─────┘
                  │
             TTS done
                  │
                  ▼
             ┌──────────┐
             │COMPACTION│ (se sessione lunga)
             │"Memori-  │
             │ zzo..."  │
             └────┬─────┘
                  │
                  └──────────────────────────────► IDLE
```

---

## 5. Flussi WiFi — con tempi

### 5A. Boot con SSID salvato — WiFi OK

```
t=0s     Boot                    "aDa"
t=0s     StartNetwork()          "Connessione al WiFi.."
t=2-10s  WiFi connesso           stop connect_timer_
t=~12s   Attivazione WS          "Attivazione..."
t=~15s   Pronto                  Occhi Astro Bot (IDLE)
```

### 5B. Boot con SSID salvato — WiFi non disponibile

```
t=0s     Boot                    "aDa"
t=0s     StartNetwork()          "Connessione al WiFi.."
t=60s    connect_timer_ scade    → StartWifiConfigMode()
t=60s    AP mode                 "Connettiti a: aDa_XXXX"
t=90s    PowerSaveTimer (30s)    → avvia ap_deep_sleep_timer_ (3min)
t=270s   ap_deep_sleep_timer_    → EnterDeepSleep()
t=270s   Deep sleep              (reboot al bottone → riprova da t=0)
```

### 5C. Boot senza SSID (primo avvio)

```
t=0s     Boot                    "aDa"
t=1.5s   No SSID → AP mode      "Connettiti a: aDa_XXXX"
t=31.5s  PowerSaveTimer (30s)    → avvia ap_deep_sleep_timer_ (3min)
t=211.5s ap_deep_sleep_timer_    → EnterDeepSleep()
```

### 5D. WiFi perso durante uso (SenseCAP Watcher)

```
t=0s     WiFi disconnesso        was_connected_=true, !in_config_mode_
t=0s     reconnect_timer_ 30s    (WiFi stack tenta auto-reconnect)
t=<30s   WiFi riconnesso?        → stop reconnect_timer_, tutto OK
t=30s    reconnect_timer_ scade  → OnWifiLostTimeout() override:
t=30s    "Connessione al WiFi..."  (boot screen + kAdaUiWifiConnecting)
t=33s    EnterDeepSleep()        → "Mi spengo..." → deep sleep
                                 (reboot al bottone → riprova)
```

### 5E. WiFi perso durante uso (altre board — default)

```
t=0s     WiFi disconnesso
t=30s    reconnect_timer_ scade  → OnWifiLostTimeout() base class:
t=30s    StopStation()           → EnterWifiConfigMode()
t=30s    AP mode                 "Connettiti a: aDa_XXXX"
```

---

## 6. Flussi Sleep/Wake — con tempi

### 6A. Auto-sleep (30s inattività, a batteria)

```
t=0s     Ultima attività (bottone/audio/MCP)
t=30s    PowerSaveTimer trigger  → EnterSleepMode()
         UI: "Dormo..."         (dots animati 400ms)
t=30.8s  Display OFF            (backlight 0)
         WiFi modem sleep       (connessione WS mantenuta)

--- WAKE ---
Bottone click / knob / MCP wake
         → ExitSleepMode()
         → WiFi full power
         → UI: Occhi IDLE
         → Display ON 100%
         → Timer riparte da 0
```

### 6B. Auto-sleep (30s inattività, in carica)

```
t=30s    PowerSaveTimer trigger  → EnterSleepMode()
         UI: "Mi carico..."     (dots animati)
t=30.2s  Display 5%             (backlight basso, testo visibile)
         WiFi modem sleep

--- WAKE ---
Stessi trigger di 6A
```

### 6C. USB plug durante sleep (batteria → carica)

```
Stato: "Dormo..." display OFF
USB collegato → GetBatteryLevel() rileva charging
         → Nessun cambio (timer continua, resta in sleep)
         (il prossimo EnterSleepMode detect è già gestito)
```

### 6D. USB unplug durante sleep (carica → batteria)

```
Stato: "Mi carico..." display 5%
USB rimosso → GetBatteryLevel() rileva discharging
         → ExitSleepMode()
         → UI: Occhi IDLE
         → Display ON 100%
         → Timer riparte (30s → sleep batteria)
```

### 6E. Long-press bottone (manuale)

```
t=0s     Bottone premuto
t=5s     LONG_PRESS_START       UI: "Dormo..." (anteprima)
         Utente rilascia        → EnterSleepMode() (display sleep)

t=10s    Se ancora premuto      UI: "Mi spengo..." (anteprima)
         Utente rilascia        → EnterDeepSleep()
                                → Batteria: deep sleep reale
                                → Carica: display sleep (fallback)
```

### 6F. Sleep via voce/chat

```
Utente: "dormi" / "vai a dormire"
LLM → tool laragoci_sleep
Gateway → bridge.callDeviceMcp("self.system.sleep")
Device → EnterSleepMode()
         → Come 6A o 6B (dipende da USB)
```

### 6G. AP mode → deep sleep

```
t=0s     Entra in AP mode       "Connettiti a: aDa_XXXX"
t=30s    PowerSaveTimer trigger → rileva kDeviceStateWifiConfiguring
         → NON entra in sleep
         → Avvia ap_deep_sleep_timer_ (180s = 3 minuti)
t=210s   ap_deep_sleep_timer_   → EnterDeepSleep()
         → Batteria: deep sleep (reboot al bottone)
         → Carica: "Mi carico..." display 5%

Se utente interagisce (bottone/knob) durante i 3 min:
         → ap_deep_sleep_timer_ cancellato
         → PowerSaveTimer riparte (30s → ri-trigger → ri-avvia 3min)
```

---

## 7. Timer attivi

| Timer                  | Durata | Dove creato                    | Trigger                          |
| ---------------------- | ------ | ------------------------------ | -------------------------------- |
| `connect_timer_`       | 60s    | `wifi_board.cc` constructor    | WiFi connection timeout → AP     |
| `reconnect_timer_`     | 30s    | `wifi_board.cc` constructor    | WiFi perso → OnWifiLostTimeout() |
| `power_save_timer_`    | 30s    | `sensecap_watcher.cc` init     | Inattività → sleep/AP check      |
| `ap_deep_sleep_timer_` | 180s   | `sensecap_watcher.cc` init     | AP mode → deep sleep             |
| `blink_timer_`         | 2s     | `ada_ui_manager.cc` Initialize | Blink occhi in IDLE              |
| `dots_timer_`          | 400ms  | `ada_ui_manager.cc` on demand  | Animazione "..." su testi        |
| `pulse_timer_`         | —      | `ada_ui_manager.cc` Initialize | Animazione listening             |
| `led_duration_timer_`  | var    | `sensecap_watcher.cc` tools    | Auto-off LED dopo duration_ms    |

---

## 8. Cosa resetta i timer

### PowerSaveTimer (30s inattività)

| Evento                    | Chiama                          | Resetta? |
| ------------------------- | ------------------------------- | -------- |
| Click bottone             | `power_save_timer_->WakeUp()`   | ✅       |
| Rotazione knob            | `power_save_timer_->WakeUp()`   | ✅       |
| Audio input/output        | `Board::ResetInactivityTimer()` | ✅       |
| MCP wake (LED, eye color) | `Board::ResetInactivityTimer()` | ✅       |
| SetPowerSaveLevel != LOW  | `power_save_timer_->WakeUp()`   | ✅       |
| Keepalive ping WS (8s)    | —                               | ❌       |
| Messaggi WS stt/llm/ping  | —                               | ❌       |

### ap*deep_sleep_timer* (3min AP mode)

| Evento                     | Resetta?                               |
| -------------------------- | -------------------------------------- |
| Click bottone              | ✅ (via ResetInactivityTimer → stop)   |
| Rotazione knob             | ✅ (via ResetInactivityTimer → stop)   |
| SetPowerSaveLevel != LOW   | ✅ (via SetPowerSaveLevel → stop)      |
| WiFi configurato (exit AP) | ✅ (timer non più attivo dopo connect) |

### reconnect*timer* (30s WiFi perso)

| Evento             | Resetta?                                |
| ------------------ | --------------------------------------- |
| WiFi riconnesso    | ✅ (`esp_timer_stop` in OnNetworkEvent) |
| Entrata in AP mode | ❌ (guard `!in_config_mode_`)           |

---

## 9. Override board-specific

### `OnWifiLostTimeout()` — virtuale in `wifi_board.h`

```cpp
// Base class (wifi_board.cc) — default per tutte le board:
virtual void OnWifiLostTimeout() {
    // WiFi perso 30s → entra in AP mode
    WifiManager::GetInstance().StopStation();
    EnterWifiConfigMode();  // → "Connettiti a: aDa_XXXX"
}

// SenseCAP Watcher override (sensecap_watcher.cc):
void OnWifiLostTimeout() override {
    // WiFi perso 30s → mostra testo → deep sleep
    SetBootStatus("Connessione al WiFi...");
    SetState(kAdaUiWifiConnecting);
    vTaskDelay(3000ms);
    EnterDeepSleep();  // → reboot al bottone
}
```

**Razionale**: il Watcher è a batteria, restare in AP mode consuma. Meglio deep sleep e riprovarci al prossimo boot.

---

## 10. File chiave

| File                                          | Ruolo                                           |
| --------------------------------------------- | ----------------------------------------------- |
| `display/ada_ui_manager.h`                    | Enum `AdaUiStateCode`, API `SetState()`         |
| `display/ada_ui_manager.cc`                   | Testi display, animazioni dots/blink/pulse      |
| `boards/common/wifi_board.h`                  | Timer WiFi, `OnWifiLostTimeout()` virtuale      |
| `boards/common/wifi_board.cc`                 | Gestione WiFi, AP mode, reconnect timer         |
| `boards/common/power_save_timer.h`            | Timer inattività, callbacks sleep/wake/shutdown |
| `boards/sensecap-watcher/sensecap_watcher.cc` | Sleep/wake/deep sleep, timer AP, override WiFi  |
| `application.cc`                              | Transizioni DeviceState ↔ AdaUiStateCode        |

---

## 11. Diagramma riassuntivo power states

```
              ┌──────────┐
              │  ATTIVO  │ (display ON, WiFi full)
              └─┬──────┬─┘
                │      │
         30s    │      │ long-press 5-10s
     inattività │      │
                ▼      ▼
        ┌───────────────────┐
        │   DISPLAY SLEEP   │
        │                   │
        │  Batteria:        │     Long-press >10s
        │  "Dormo..."       │──────────────┐
        │  display OFF      │              │
        │                   │              ▼
        │  Carica:          │     ┌────────────────┐
        │  "Mi carico..."   │     │  DEEP SLEEP    │
        │  display 5%       │     │  "Mi spengo.." │
        └─────┬───────┬─────┘     │  (solo batt)   │
              │       │           │  ↓             │
         bottone  USB unplug     │  esp_deep_sleep │
         / knob   (se carica)    │  ↓             │
              │       │           │  GPIO2 wake    │
              ▼       ▼           │  = REBOOT      │
        ┌──────────┐              └────────────────┘
        │  ATTIVO  │                    ↑
        └──────────┘                    │
                                   Anche da:
                                   - WiFi perso 30s (Watcher)
                                   - AP mode 3min timeout
```
