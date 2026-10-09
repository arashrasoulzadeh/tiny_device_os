# Web Flash Console (`make web`)

## Overview

The web flash console provides a local web UI for the full connected-device workflow:
- See what USB devices are plugged in
- Choose which apps to compile into the image
- Select the main app that starts after boot
- Configure kernel and feature flags
- Build and flash with live logs and structured errors
- Open a TTY console on the flashed board

## Quick Start

```bash
make web
```

This starts a local HTTP server on `127.0.0.1:8765` and opens the web UI in your browser.

## Server Options

| Option | Default | Description |
|--------|---------|-------------|
| `--host` | `127.0.0.1` | Bind address (loopback only in v1) |
| `--port` | `8765` | Port to listen on |
| `--config` | `device_config.yaml` | Path to device config |
| `--open` | auto | Open browser when stdout is a TTY |

## Screens

1. **Devices** — Live list of USB ports, matched profiles, connect state
2. **Device** — Identity, flash geometry, LCD, buttons, pin conflicts, secrets status
3. **Image** — Apps, main app, features, size preview, build and flash actions
4. **Job** — Progress, streaming log, compiler/linker/upload errors, size result
5. **Console** — TTY (encrypted link shell) with raw monitor fallback

## Configuration

The UI reads and writes these YAML files:

### `device_config.yaml` (read/write)

```yaml
device:
  name: nodemcu
  arch: esp8266
  board: nodemcu
  pio_env: nodemcu
  port: auto
  baud: 115200

lcd:
  type: ssd1306
  width: 128
  height: 32
  bus: i2c
  address: 0x3C
  scl: D1
  sda: D2

inputs:
  buttons:
    - name: up
      pin: D5
      active_low: true
      pull: up
    - name: select
      pin: D6
      active_low: true
      pull: up

# Apps to compile (explicit list overrides auto-fit)
apps:
  - launcher
  - counter
  - clock

# App to start after boot
main_app: clock

# Feature flags (Kconfig symbols)
features:
  HEAP_SIZE_KB: 32
  ENABLE_POWER_MANAGEMENT: true
  TICK_RATE_HZ: 1000
```

### `device_secrets.yaml` (read-only in v1)

```yaml
wifi:
  ssid: "your-network"
  password: "your-password"

link:
  key_id: 1
  key: "0123456789abcdef0123456789abcdef"
```

The UI shows `secrets_present: true/false` but never returns secret values.

## API Routes

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/health` | Server version, repo root, PlatformIO status |
| GET | `/api/ports` | List detected USB serial ports |
| GET | `/api/devices` | Current device config + port state |
| POST | `/api/device/port` | Set session port override |
| GET | `/api/apps` | List stdapps with metadata |
| POST | `/api/apps` | Write `apps:` to device_config.yaml |
| GET | `/api/main-app` | Get current main_app |
| POST | `/api/main-app` | Set main_app |
| GET | `/api/features` | List Kconfig features with values |
| POST | `/api/features` | Write `features:` to device_config.yaml |
| GET | `/api/size` | Size report (last build or budget) |
| POST | `/api/jobs` | Start build/flash job |
| GET | `/api/jobs/current` | Current job state |
| GET | `/api/jobs/current/log` | SSE log stream |
| POST | `/api/jobs/current/cancel` | Cancel current job |
| POST | `/api/console` | Open console session |
| POST | `/api/console/input` | Send input to console |
| GET | `/api/console/output` | SSE console output |
| POST | `/api/console/close` | Close console |

## Job States

- `idle` — No job running
- `building` — PlatformIO compile running
- `uploading` — Flashing firmware to device
- `flashed` — Successfully built and flashed
- `built` — Built successfully, not flashed (no port)
- `failed` — Build or upload failed
- `cancelled` — User cancelled the job

## Size Report

The size report shows two numbers at two moments:

**Before compile (preview):**
- Last measured size from `build/web/last_size.json`
- Or flash budget from device config (4MB for NodeMCU)
- Marked "stale" when apps/features change

**After compile (measured):**
- Parsed from PlatformIO memory summary + `firmware.map`
- Split: kernel, per-app, runtime, other
- Flash used, flash budget, RAM used, percent
- Delta vs previous build

## Security

- Binds `127.0.0.1` only (no auth in v1)
- Never exposes Wi-Fi passwords or link keys in API, logs, or size reports
- YAML writes are atomic (temp file + rename)
- One `.bak` kept of `device_config.yaml`

## Testing

```bash
# Unit tests (no hardware)
python3 tests/unit/test_web_config.py
python3 tests/unit/test_web_jobs.py
python3 tests/unit/test_web_sizes.py

# Integration test (starts server on ephemeral port)
python3 tests/integration/test_web_integration.py
```

## Manual Verification

```bash
# 1. Plug in NodeMCU
# 2. Run web UI
make web
# 3. Select apps, main app, features
# 4. Click Build + Flash
# 5. Confirm size report
# 6. Open Console
```