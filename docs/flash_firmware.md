# Flash Firmware SenseCAP Watcher — Guida

> Data: 2026-04-02
> Stato: IN CORSO — bloccato su partition table (asset troppo grandi)

---

## Device

- **Modello**: SenseCAP Watcher XiaoZhi Edition
- **Chip**: ESP32-S3 (QFN56) rev v0.2, PSRAM 8MB, Flash 16MB
- **MAC**: `10:b4:1d:e5:fe:a8`
- **Porta seriale**: **COM5** (ESP32-S3) — COM3 non risponde (è il Himax WE2)

---

## Ambiente di sviluppo

```powershell
# Attiva ESP-IDF v5.5.3 (v5.4.3 NON basta — errore "no versions of idf match >=5.5.2")
$env:PATH = (py -3.12 -c "import sys, os; print(os.path.dirname(sys.executable))") + ";" + $env:PATH
. C:\esp\v5.5.3\esp-idf\export.ps1

cd C:\esp\xiaozhi-esp32
```

---

## Menuconfig — impostazioni applicate

```
Xiaozhi Assistant Configuration
  ├── Board Type        → SENSECAP_WATCHER
  ├── Connection Type   → Websocket
  ├── Language          → Italian (IT) — solo questa selezionata
  ├── OTA Server URL    → https://laragoci.lara-ai.eu/xiaozhi/ota/
  └── Websocket URL     → wss://laragoci.lara-ai.eu/xiaozhi/v1/
```

---

## Modifica hold-to-talk — `main/boards/sensecap-watcher/sensecap_watcher.cc`

Nella funzione `InitializeButton()`, sostituire il blocco `BUTTON_SINGLE_CLICK` con:

```cpp
// HOLD-TO-TALK: sostituisce BUTTON_SINGLE_CLICK + ToggleChatState()
iot_button_register_cb(btns, BUTTON_PRESS_DOWN, nullptr, [](void* button_handle, void* usr_data) {
    auto self = static_cast<SensecapWatcher*>(usr_data);
    self->power_save_timer_->WakeUp();
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateStarting) {
        self->EnterWifiConfigMode();
        return;
    }
    // StartListening() apre la WS e imposta kListeningModeManualStop internamente.
    // NON chiamare SetListeningMode() prima: ha il side effect di cambiare lo stato
    // a kDeviceStateListening, rompendo il branch idle in HandleStartListeningEvent.
    app.StartListening();
}, this);

iot_button_register_cb(btns, BUTTON_PRESS_UP, nullptr, [](void* button_handle, void* usr_data) {
    auto self = static_cast<SensecapWatcher*>(usr_data);
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateListening) {
        app.StopListening();
    }
}, this);
```

I callback `BUTTON_LONG_PRESS_START` (shutdown) e `BUTTON_LONG_PRESS_HOLD` (factory reset 10s) restano invariati.

**NON serve modificare `application.h`** — `StartListening()` e `StopListening()` sono gia public.

---

## Comandi build e flash

```powershell
# Build pulita (necessaria per cambio board da BOX-3)
idf.py fullclean

# Menuconfig (se serve)
idf.py menuconfig

# Build + flash + monitor
idf.py -p COM5 -b 2000000 build flash monitor

# Esci dal monitor: Ctrl+]
```

---

## BLOCCO ATTUALE: generated_assets.bin troppo grande

### Errore

```
File generated_assets.bin (length 10815931) at offset 8388608 will not fit in 16777216 bytes of flash.
```

- `generated_assets.bin` = 10.8 MB
- Offset asset = 0x800000 (8 MB)
- Flash totale = 16 MB → solo 8 MB disponibili per asset
- Overflow: 10.8 MB > 8 MB

### Nota

- Lingua gia ridotta a solo italiano
- L'app (`xiaozhi.bin`) occupa solo 3.1 MB dei 7.9 MB allocati (0x20000 → 0x800000)

### Prossimi step per risolvere

1. **Trovare la partition table**:

   ```powershell
   dir C:\esp\xiaozhi-esp32\*.csv -Recurse | Select-Object FullName
   Select-String "PARTITION_TABLE" C:\esp\xiaozhi-esp32\sdkconfig
   ```

2. **Opzione A — Ridurre asset**: in menuconfig, cercare Font Configuration / Display Configuration e disabilitare font grandi, emoji, o asset display non necessari

3. **Opzione B — Modificare partition table**: spostare offset asset da 0x800000 (8 MB) a 0x400000 (4 MB) — l'app e 3.1 MB, ci sta in 4 MB. Questo darebbe 12 MB per asset (sufficiente per 10.8 MB)

4. **Opzione C — Verificare se il SenseCAP ha flash > 16 MB**: improbabile ma da verificare con `esptool.py flash_id -p COM5`

---

## Riferimenti

- Fix hold-to-talk BOX-3: `Note/plans/04_pipeline_button.md`
- Setup device originale: `Note/plans/02_device.md`
- Board file SenseCAP: `main/boards/sensecap-watcher/sensecap_watcher.cc`
- Board file BOX-3 (riferimento): `main/boards/esp-box-3/esp_box3_board.cc`
