# XiaoZhi ESP32 + OpenClaw — Self-hosted Voice Assistant

Fork of [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) with **Ada UI**, power management, and [OpenClaw](https://github.com/openclaw/openclaw) gateway integration.

No cloud dependency. Privacy-first. EU-hosted AI stack. Choose your own LLM provider.

## Supported Devices

| Device | Button Mode | Sleep | Display |
|---|---|---|---|
| **SenseCAP Watcher** (~$59) | Click-to-talk (toggle) | Auto-sleep 30s, deep sleep, charging sleep | 1.45" TFT |


## Features

### Ada UI — 13-state animated interface

Custom LVGL display with smooth state transitions:

- **Boot** — "aDa" title with animated status dots (WiFi connecting, activating)
- **Idle** — Astro Bot eyes with 2-second blink animation
- **Listening** — Pulsing dots while capturing speech
- **Thinking** — Ring animation while LLM processes
- **Speaking** — Visual feedback during TTS playback
- **Acting** — Tool execution indicator
- **Sleeping** — "Dormo..." / "Mi carico..." (battery vs charging)
- **Shutdown** — Clean shutdown animation

### Power Management (SenseCAP Watcher)

- **Auto-sleep**: 30s inactivity timer (active even while charging)
- **Progressive long-press**: 5s = sleep, 10s = deep sleep
- **Charging sleep**: display at 5% brightness, "Mi carico..." text
- **Deep sleep**: ext0 wakeup on GPIO2, button press to wake
- **USB plug/unplug**: automatic sleep mode transitions
- **WiFi lost**: 30s without WiFi = deep sleep

### MCP Hardware Tools

Device-side tools controllable via voice or chat:

| Tool | Description |
|---|---|
| `self.led.set` | Set RGB LED color and brightness |
| `self.haptic.buzz` | Vibration feedback |
| `self.camera.photo` | Capture photo (Pixtral vision) |
| `self.sensor.read` | Read temperature, humidity, light |
| `self.volume.set` | Adjust speaker volume |
| `self.system.sleep` | Enter sleep mode via voice command |
| `self.system.factory_reset` | Factory reset via HTTP/MCP |
| `self.display.set` | Control Ada UI state |

### Voice Pipeline (EU Stack)

All processing on Mistral servers in Paris — no data leaves the EU:

```
[ESP32 Device] <--WS/Opus--> [OpenClaw Gateway] <--API--> [Mistral Paris]
    firmware                  extensions/xiaozhi/          STT + LLM + TTS
```

| Component | Model | Provider |
|---|---|---|
| STT | Voxtral Mini | Mistral (Paris) |
| LLM | Mistral Small | Mistral (Paris) |
| TTS | Voxtral Mini TTS | Mistral (Paris) |

Custom voice cloning supported via [Mistral Console](https://console.mistral.ai) (10-30s WAV sample).

Other providers also supported: Anthropic Claude, Google Gemini, Groq, OpenAI.

## Architecture

```
                    ┌─────────────────────────┐
                    │   ESP32-S3 Device        │
                    │                          │
                    │  ┌──────────┐ ┌───────┐  │
                    │  │ Ada UI   │ │ Audio │  │
                    │  │ Manager  │ │ Codec │  │
                    │  └──────────┘ └───┬───┘  │
                    │       │       Opus│PCM   │
                    │  ┌────┴───────────┴───┐  │
                    │  │  WebSocket Client  │  │
                    └──┼───────────────────┼──┘
                       │  WSS (port 443)   │
                       ▼                   ▲
                    ┌──┼───────────────────┼──┐
                    │  │  OpenClaw Gateway  │  │
                    │  ▼                   │  │
                    │  ┌─────────────────┐ │  │
                    │  │ xiaozhi bridge  │ │  │
                    │  │  audio-pipeline │ │  │
                    │  │  context-mgr    │ │  │
                    │  └────────┬────────┘ │  │
                    │           │ API      │  │
                    │     ┌─────┴─────┐    │  │
                    │     │ STT → LLM │    │  │
                    │     │   → TTS   │    │  │
                    │     └───────────┘    │  │
                    └─────────────────────────┘
```

## Prerequisites

- **ESP-IDF** 5.5.2+ ([install guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/))
- **OpenClaw** server ([install guide](https://github.com/openclaw/openclaw))
- **Mistral API key** (or another supported LLM provider)
- **LVGL Pro** (optional, for UI customization)

## Build & Flash

### 1. Clone and configure

```bash
git clone https://github.com/lara-ai-eu/xiaozhi-esp32.git
cd xiaozhi-esp32
idf.py set-target esp32s3
```

### 2. Menuconfig

```bash
idf.py menuconfig
```

Key settings:

- **Board Selection** → `SenseCAP Watcher`
- **Connection** → `WebSocket` protocol
- **Display** → Enable Ada UI
- **LVGL** → Enable `lv_font_montserrat_48` (for boot title)

### 3. Build and flash

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

On Windows, replace `/dev/ttyACM0` with `COM3` (or your port).

## OpenClaw Gateway Setup

### 1. Install OpenClaw

```bash
npm i -g openclaw@latest
```

### 2. Configure

```bash
openclaw config set gateway.mode local
openclaw config set messages.stt.provider mistral
openclaw config set messages.llm.provider mistral
openclaw config set messages.llm.model mistral-small-latest
openclaw config set messages.tts.provider openai
openclaw config set messages.tts.openai.voice <your-voice-uuid>
```

Set your API key:
```bash
export MISTRAL_API_KEY=<your-key>
export OPENAI_TTS_BASE_URL=https://api.mistral.ai/v1
```

### 3. Run the gateway

```bash
openclaw gateway run --bind loopback --port 18789 --force
```

### 4. Connect the device

On first boot, the device enters AP mode (captive portal). Connect to its WiFi and configure:
- Your home WiFi SSID and password
- Server: `your-server-address` (IP or domain)

## Differences from Upstream XiaoZhi

| Area | Upstream (78/xiaozhi-esp32) | This Fork |
|---|---|---|
| **Display** | Basic emoji/text states | Ada UI: 13 animated states with LVGL Pro components |
| **Power** | Basic sleep | Progressive sleep (auto 30s, long-press 5/10s, charging mode, deep sleep) |
| **Backend** | Chinese cloud servers | OpenClaw gateway (self-hosted, any LLM provider) |
| **Tools** | Limited | 9+ MCP hardware tools (LED, camera, haptic, sensors) |
| **Button** | Hold-to-talk only | Click-to-talk (Watcher) + Hold-to-talk (BOX-3) |
| **Boot UX** | Generic | Custom boot screen with WiFi status, animated dots |
| **Privacy** | Cloud-dependent | EU-only processing, no data leaves your infrastructure |

## Documentation

- [Ada UI State Machine](docs/ada_state_machine.md) — All 13 states, transitions, and timers
- [MCP Hardware Tools](docs/MCP_Hardware_Tools.md) — Adding and using device tools
- [Flash Firmware](docs/flash_firmware.md) — Detailed build and flash guide
- [Power Management](docs/sleep_wake_architecture.md) — Sleep/wake architecture

## License

MIT License — same as upstream [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32).

See [LICENSE](LICENSE) for details.

## Credits

- [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) — Original firmware
- [OpenClaw](https://github.com/openclaw/openclaw) — Gateway and voice pipeline
- [Mistral AI](https://mistral.ai) — EU-hosted STT, LLM, and TTS
- [LVGL](https://lvgl.io) — UI framework

