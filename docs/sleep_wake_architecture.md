# Guida: Architettura Sleep/Wake — SenseCAP Watcher + OpenClaw Gateway

## Panoramica

Il SenseCAP Watcher ha 3 stati di risparmio energetico:

| Stato                  | Display                      | WiFi        | WS       | CPU      | Wake                      |
| ---------------------- | ---------------------------- | ----------- | -------- | -------- | ------------------------- |
| **Idle**               | ON 100%                      | Full        | Connesso | Full     | —                         |
| **Display sleep** (R4) | OFF (batteria) o 5% (carica) | Modem sleep | Connesso | Full     | Bottone, knob, MCP, timer |
| **Deep sleep** (R5)    | OFF                          | OFF         | Morto    | RTC only | Solo bottone GPIO2        |

## Display Sleep (R4)

### Entrata

- **Automatica**: 30s di inattivita (PowerSaveTimer, R6)
- **Manuale**: long-press bottone 5-10s (R3)

### Cosa succede

```
PowerSaveCheck(): ticks >= 30
    → OnEnterSleepMode()
    → EnterSleepMode()
        ├── Batteria: "Dormo..." → display OFF
        └── Carica: "Mi carico..." → display 5%
        → WiFi modem sleep (attivo ma basso consumo)
        → WS resta connesso
```

### Uscita

```
Evento wake (bottone/knob/MCP)
    → WakeUp() o ResetInactivityTimer()
    → ExitSleepMode()
        ├── WiFi full power
        ├── UI → idle
        └── Display ON 100%
    → ticks = 0 (timer riparte)
```

### Cosa resetta il timer (WakeUp)

- Click bottone → `power_save_timer_->WakeUp()`
- Rotazione knob → `power_save_timer_->WakeUp()`
- Audio input/output → `Application::OnAudioInput/Output()` → `WakeUp()`
- Messaggio WS actionable (R8) → `ResetInactivityTimer()` → `WakeUp()`

### Cosa NON resetta il timer

- Keepalive ping gateway (ogni 8s) — **lezione R6**: inizialmente aggiunto
  `ResetInactivityTimer()` in `websocket_protocol.cc` OnData, rimosso perche
  i ping ogni 8s impedivano al timer di raggiungere 30s.
- Messaggi `stt`, `llm`, `ping` — non actionable

## Deep Sleep (R5)

### Entrata

- **Manuale**: long-press bottone >10s
- Solo da batteria; se in carica → charging sleep (R4) invece di deep sleep

### Cosa succede

```
EnterDeepSleep()
    → "Mi spengo..." su display
    → Spegni LCD, camera, audio PA
    → esp_deep_sleep_start() con ext0 wakeup GPIO2
```

### Uscita

- **Solo bottone fisico** (GPIO2 ext0 wakeup)
- Fa un **reboot completo**: WiFi riconnette, WS riconnette, app_main() riparte

## Gateway — MCP durante sleep

### Problema risolto (R8)

I tool MCP (`laragoci_led`, `laragoci_play`, ecc.) usavano `queueDeferredHwAction()`
che eseguiva solo dopo un turno vocale (speaking → idle → flush).
Se nessuno parlava, le azioni restavano in coda per sempre.

### Soluzione

`queueDeferredHwAction()` controlla `activePipeline.isIdle`:

- **Pipeline idle**: flush immediato via microtask (con delay 400ms tra azioni)
- **Pipeline attivo**: deferred come prima

### Nei log

```
# Pipeline idle → esecuzione immediata
[XZ bridge] R8 immediate flush (pipeline idle)
[XZ bridge] executing 1 deferred hw action(s)
[XZ bridge] MCP response id=3 ok

# Pipeline attivo → deferred
[XZ bridge] deferred hw action queued: led (self.led.set)
... turno vocale ...
[XZ bridge] executing 1 deferred hw action(s)
```

## Sessioni e canali

### Sessione voce = `agent:main:voice`

- Il device e la web UI vedono la **stessa conversazione**
- Ma la risposta TTS torna solo al canale da cui e arrivato il messaggio:
  - Voce → TTS al device
  - Web UI → testo alla web UI

### Cosa arriva al device dalla web UI

- **Tool MCP**: `laragoci_led`, `laragoci_play`, `laragoci_haptic`, `laragoci_eye_color` → SI
- **Risposte testo/TTS**: NO (tornano alla web UI)

## Limiti e comportamenti noti

### Device disconnesso (deep sleep o WiFi perso)

- `activePipeline` diventa `null`
- Le azioni dalla web UI vanno in `deferredHwActions[]`
- Alla riconnessione + primo turno vocale → vengono eseguite
- Azioni con stessa key (es. `led`) si sovrascrivono (ultimo vince)

### MCP multipli simultanei

- Il device non gestisce piu di 1-2 MCP contemporanei
- 5+ play azioni nello stesso momento → timeout 5000ms
- Fix: microtask batching + delay 400ms tra azioni in `executeDeferredHwActions()`

### LVGL durante sleep

- Operazioni LVGL (`SetEyeColor`, `SetState`) su display spento possono causare
  **watchdog timeout** (`lv_obj_invalidate` blocca il main task)
- Fix (R8 firmware): `SET_EYE_COLOR` e `SET_UI` nel filtro wake →
  `ExitSleepMode()` prima di qualsiasi operazione LVGL

### USB plug/unplug durante sleep

- USB collegato → passa da "Dormo..." a "Mi carico..." (display 5%)
- USB scollegato → wake completo (torna a idle)
- Gestito in `GetBatteryLevel()` di `sensecap_watcher.cc`

## File chiave

### Firmware (`Note/main/`)

| File                                          | Ruolo                                                     |
| --------------------------------------------- | --------------------------------------------------------- |
| `boards/common/power_save_timer.cc`           | Timer 1Hz, conta ticks, chiama callbacks                  |
| `boards/common/board.h`                       | `ResetInactivityTimer()` virtual                          |
| `boards/sensecap-watcher/sensecap_watcher.cc` | `EnterSleepMode()`, `ExitSleepMode()`, `EnterDeepSleep()` |
| `application.cc`                              | `CanEnterSleepMode()`, filtro wake R8, `OnIncomingJson()` |
| `display/ada_ui_manager.cc`                   | Animazione dots, stati boot/sleep/shutdown                |

### Gateway (`extensions/xiaozhi/src/`)

| File                | Ruolo                                                      |
| ------------------- | ---------------------------------------------------------- |
| `bridge.ts`         | `queueDeferredHwAction()`, `activePipeline`, MCP immediato |
| `audio-pipeline.ts` | `isIdle` getter, state machine, `[XZ STATE]` logging       |
| `tools.ts`          | Tool MCP (`laragoci_*`), `queueDeferredHwAction()` calls   |

## Comandi utili per debug

```bash
# Serial monitor device (USB collegato)
idf.py -p COM3 monitor

# Log gateway
tail -f /tmp/openclaw-gateway.log | sed 's/\x1b\[[0-9;]*m//g'

# Grep stati specifici
grep "XZ STATE\|PowerSaveTimer\|sleep\|wake" /tmp/openclaw-gateway.log

# Copiare firmware modificato
scp openclaw@100.115.130.6:/home/openclaw/openclaw/Note/main/application.cc .\main\application.cc
```
