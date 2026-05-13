# SenseCAP Watcher — Ada Firmware

Custom firmware for the [SenseCAP Watcher](https://www.seeedstudio.com/SenseCAP-Watcher-W1-p-5979.html) (~$59) with [OpenClaw](https://github.com/Lara-srl/openclaw) voice gateway integration.

```
[SenseCAP Watcher] ←—WS/Opus—→ [OpenClaw Gateway] ←—API—→ [Mistral Paris]
     firmware                    extensions/xiaozhi/          STT + LLM + TTS
```

## Features

### Ada UI — 13 Display States

The round 412x412 LCD shows animated states during the voice interaction:

| State | Display | Animation |
|-------|---------|-----------|
| Boot | **"aDa"** (large title) | — |
| WiFi Connecting | "Connecting to WiFi.." | Static |
| WiFi Config (AP) | "Connect to: aDa_XXXX" | Static |
| Activating | "Activating..." | Static |
| Idle | Astro Bot eyes | Blink every 2s |
| Listening | "Listening..." | Animated dots |
| Thinking | "Thinking..." | Animated dots |
| Acting | "Executing..." | Animated dots |
| Speaking | "Speaking..." | Animated dots |
| Memorizing | "Memorizing..." | Animated dots |
| Sleeping | "Sleeping..." | Animated dots |
| Charging Sleep | "Charging..." | Animated dots |
| Shutdown | "Shutting down..." | Animated dots |

<!-- TODO: Add demo video/GIF of Ada UI states -->

### Power Management

| Action | Behavior |
|--------|----------|
| **Auto-sleep** | 30 seconds of inactivity → sleep mode |
| **Button 5s hold** | Enter sleep mode |
| **Button 10s hold** | Deep sleep (ext0 wakeup on GPIO2) |
| **Button click while sleeping** | Wake up |
| **USB plugged during sleep** | Switch to charging sleep (display at 5%) |
| **USB unplugged** | Wake to idle |
| **WiFi lost 30s** | Deep sleep |
| **AP mode 3 min timeout** | Deep sleep |
| **Sleep via voice/chat** | MCP tool `self.system.sleep` |

<!-- TODO: Add demo video of sleep/wake behavior -->

### Button — Click-to-Talk

- **Single click**: Start / stop listening (toggle mode)
- **Long press 5s**: Sleep
- **Long press 10s**: Deep sleep (shutdown)
- No factory reset on button (removed for safety)

<!-- TODO: Add demo video of click-to-talk interaction -->

### Camera Vision

- Device captures JPEG via Himax AI chip (SSCMA)
- Image sent to gateway → analyzed by Pixtral (multimodal LLM)
- Response spoken back to the user

<!-- TODO: Add demo video of camera vision -->

### MCP Hardware Tools

Control device hardware through voice commands or chat UI:

| Tool | Description |
|------|-------------|
| `ada_status` | Check device connectivity |
| `ada_speak` | Text-to-speech output |
| `ada_emoji` | Display emotion on LCD |
| `ada_volume` | Speaker volume (0-100) |
| `ada_play` | Sound effects (success, vibration, exclamation) |
| `ada_eye_color` | Set eye color (hex) |
| `ada_led` | RGB LED (color, mode: static/pulse/blink) |
| `ada_haptic` | Vibration feedback |
| `ada_sleep` | Enter sleep mode |

<!-- TODO: Add demo videos for each MCP tool -->

## Hardware Specs

| Component | Detail |
|-----------|--------|
| MCU | ESP32-S3 |
| Display | 412x412 round LCD (SPD2010, QSPI) |
| Audio | ES8311 codec, 24kHz I/O |
| Camera | Himax AI chip (SSCMA) |
| LED | Single RGB LED (GPIO 40) |
| Button | Boot button (GPIO 0) + knob |
| Storage | 32MB flash |
| Battery | LiPo with charge detection |
| Connectivity | WiFi (2.4GHz) |

## Build & Flash

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) v5.5.2+
- Python 3.x

### One-click Build

```bash
python scripts/release.py sensecap-watcher
```

### Manual Build

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

In menuconfig, select:

```
Xiaozhi Assistant -> Board Type -> SenseCAP Watcher
```

Required config options:

```
CONFIG_BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/32m.csv"
CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH=y
CONFIG_ESPTOOLPY_FLASH_MODE_AUTO_DETECT=n
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
```

### Flash

```bash
idf.py -DBOARD_NAME=sensecap-watcher build flash
```

## Factory Information Backup

> **WARNING**: If your device was previously running SenseCAP firmware (not XiaoZhi), back up the factory partition **before** flashing. Without this backup, the device may not reconnect to the SenseCraft server if you restore the original firmware.

```bash
esptool.py --chip esp32s3 --baud 2000000 \
  --before default_reset --after hard_reset --no-stub \
  read_flash 0x9000 204800 nvsfactory.bin
```

## OpenClaw Gateway Setup

See the [XiaoZhi Extension README](https://github.com/Lara-srl/openclaw/tree/main/extensions/xiaozhi) for gateway installation and configuration.

## Files

| File | Description |
|------|-------------|
| `sensecap_watcher.cc` | Board init, power management, sleep/wake, button handling |
| `config.h` | GPIO pins, peripherals, hardware constants |
| `sensecap_audio_codec.cc/h` | ES8311 audio codec driver |
| `sscma_camera.cc/h` | Himax AI camera integration |

## License

Apache 2.0 — same as upstream [XiaoZhi](https://github.com/78/xiaozhi-esp32).
