# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- MiniMax provider support (percent-based quota mapping, USERTrust RSA root CA)
- Provider mode config (kimi / minimax / both) with per-provider pre-save verification in the portal
- Tap-to-switch on the touchscreen in both mode (edge-detected, 300 ms debounce), per-provider RAM slots with independent error states
- Serial commands `SET:PROVIDER` / `SET:MMKEY`; `GET:CONFIG` now returns `mmkey` + `mode`

## [0.1.0] - 2026-08-05

Initial release.

### Added

- Weekly quota ring + 5-hour window bar display on ESP32-2432S028 (CYD), color-coded by usage level (<70% green, 70–90% yellow, >90% red)
- Direct HTTPS fetch of the Kimi `/coding/v1/usages` API with DigiCert Global Root G2 certificate validation after NTP sync (graceful `setInsecure` fallback with on-screen `!` indicator when the clock is not synced)
- Captive-portal WiFi provisioning (Chinese web form) with pre-save verification of WiFi credentials and API key; config persisted in NVS
- Four-state state machine: BOOT → PORTAL / CONNECTING → RUNNING
- Offline resilience: stale-data greying with age label, WiFi auto-reconnect, exponential backoff capped at 5 minutes on API failures
- Invalid-key (401/498 / booster wallet disabled) full-screen error page
- Serial backdoor (115200 baud): `GET:CONFIG` / `SET:WIFI` / `SET:KEY` / `SET:INTERVAL` / `REFRESH` / `GET:USAGE` / `RESET:CONFIG` / `REBOOT`, with masked API key output and a Python client (`scripts/send_command.py`)
- Hold BOOT (GPIO0) for 5 s to wipe config and re-enter provisioning, with on-screen countdown and release-to-cancel
- Pure-logic core library (`lib/core/`) with 29 host-side unit tests (time parsing, JSON parsing, formatting, color levels, command parsing, config validation, retry policy) runnable via `pio test -e native`
- uv-managed Python toolchain (PlatformIO + pyserial), Windows SCons spawn fix (`scripts/fix_spawn_cwd.py`)

### Fixed during development

- Left-align "5H WINDOW" label (was rendered off-screen by wrong text datum)
- HTML-escape scanned SSIDs in the portal form (injection hardening)
- Clear the key-invalid latch when config changes via the serial backdoor, so `SET:KEY` recovers fetching without a reboot

[0.1.0]: https://github.com/wx528/esp32-2432s028-kimi-usage/releases/tag/v0.1.0
