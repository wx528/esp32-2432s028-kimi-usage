# esp32-cyd-llm-usage

[![CI](https://github.com/wx528/esp32-cyd-llm-usage/actions/workflows/ci.yml/badge.svg)](https://github.com/wx528/esp32-cyd-llm-usage/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/wx528/esp32-cyd-llm-usage)](LICENSE)
[![Release](https://img.shields.io/github/v/release/wx528/esp32-cyd-llm-usage)](https://github.com/wx528/esp32-cyd-llm-usage/releases)

[中文文档](README.zh-CN.md)

Standalone Kimi Coding Plan + MiniMax usage monitor on the ESP32-2432S028 (Cheap Yellow Display / CYD). The device connects to WiFi and calls the provider API directly — no PC required. Pick Kimi, MiniMax, or both; in both mode, tap the screen to switch views.

## Features

- Weekly quota ring (usage %, used/limit numbers, reset countdown)
- 5-hour window progress bar (used/limit + countdown)
- Four display orientations (0/90/180/270): short-press BOOT to cycle, persisted across reboots
- MiniMax provider support alongside Kimi (percent-based quota mapping)
- Provider mode `kimi` / `minimax` / `both`; tap-to-switch views in both mode (edge-detected, 300 ms debounce), each provider keeps its own data slot and error state
- Color coding by usage: <70% green / 70–90% yellow / >90% red
- Phone-based captive portal setup (Chinese web form); WiFi + API key are verified per provider before anything is saved; config persists in NVS
- Offline resilience: stale data turns grey with an age label, auto-reconnect, exponential backoff on API failures
- Invalid key (401/498) full-screen notice; hold BOOT 5s to wipe config and re-enter setup
- Serial backdoor for debugging (read config, change key, force refresh, etc.)
- Pure logic separated from hardware: 46 host-side unit tests, no device needed

## Hardware

- ESP32-2432S028 (CYD, 240×320 ILI9341, BOOT button on GPIO0)
- Pin map lives in `platformio.ini` build flags (no need to edit TFT_eSPI's `User_Setup.h`)

## Build & Flash

The Python environment is managed with [uv](https://docs.astral.sh/uv/):

```powershell
uv sync
.venv\Scripts\pio.exe run -e esp32-2432s028   # build
```

Flashing (this board's auto-download circuit does not work):

1. Hold the BOOT button
2. `.venv\Scripts\pio.exe run --target upload -e esp32-2432s028`
3. Release BOOT when you see `Connecting.....`
4. Press RST once after `[SUCCESS]`

On Linux/macOS use `.venv/bin/pio` instead, and set `upload_port`/`monitor_port` in `platformio.ini` to your actual serial port.

## Configuration

1. On first boot (or after a config wipe) the device enters Setup mode and shows hotspot details on screen
2. Connect your phone to the `CYD-Kimi-Setup` hotspot (password `kimisetup`); the config page should pop up automatically (or open 192.168.4.1)
3. Pick the provider mode (`kimi` / `minimax` / `both`), fill in WiFi credentials, the API key(s) for the chosen provider(s), and refresh interval (30–3600 s), then save
4. The device verifies WiFi + each required API key on the spot and only persists the config if all pass, then reboots

## Changing Configuration Later

**Via serial** (keeps other settings, fastest):

```powershell
python scripts/send_command.py "SET:KEY:sk-new-key"        --port COM7
python scripts/send_command.py "SET:MMKEY:mm-new-key"      --port COM7
python scripts/send_command.py "SET:PROVIDER:both"         --port COM7  # kimi | minimax | both
python scripts/send_command.py "SET:WIFI:new-ssid:pass"    --port COM7
python scripts/send_command.py "REBOOT"                    --port COM7  # recommended after SET:WIFI / SET:PROVIDER
```

A new API key takes effect immediately; send `REBOOT` after `SET:WIFI` to reconnect on the new network, and after `SET:PROVIDER` to re-fetch with the new mode.

**Via BOOT button** (full re-provisioning): hold BOOT for 5 s until the on-screen countdown finishes — the config is wiped and the device reboots into Setup mode (release early to cancel). Then repeat the phone setup flow. A short press (<0.5 s) instead cycles the display orientation (0/90/180/270) and persists it — only the full 5 s hold wipes the config.

## Serial Backdoor (115200 baud, `\n`-terminated)

| Command | Action |
|---|---|
| `GET:CONFIG` | Show config (API keys masked, includes provider mode) |
| `SET:WIFI:<ssid>:<pass>` | Change WiFi |
| `SET:KEY:<apikey>` | Change Kimi API key |
| `SET:MMKEY:<apikey>` | Change MiniMax API key |
| `SET:PROVIDER:<kimi\|minimax\|both>` | Switch provider mode |
| `SET:INTERVAL:<30-3600>` | Change refresh interval |
| `REFRESH` | Fetch immediately |
| `GET:USAGE` | Tells you to look at the screen |
| `RESET:CONFIG` | Wipe config and reboot |
| `REBOOT` | Reboot |

A Python client is included: `python scripts/send_command.py "GET:CONFIG" --port COM7`

## Unit Tests

Pure logic (JSON parsing per provider / formatting / color levels / command parsing / config validation / retry backoff) runs on the host — no hardware required:

```powershell
.venv\Scripts\pio.exe test -e native   # needs g++ (e.g. scoop install mingw-winlibs)
```

## Project Layout

- `lib/core/` — pure logic (native-testable)
- `src/` — hardware layer (display / kimi_net / portal / config_store / serial_console / main)
- `test/` — 9 native test suites
- `scripts/` — Windows build fix + serial client
- `docs/` — design spec and implementation plan

## Dependencies

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — display driver
- [ArduinoJson](https://arduinojson.org/) — JSON parsing
- ESP32 Arduino core built-ins: WebServer / DNSServer / Preferences / WiFiClientSecure

## License

[MIT](LICENSE)
