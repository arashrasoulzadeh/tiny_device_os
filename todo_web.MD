# Web flash console (`make web`)

Agents implementing this file: read **For agents** below before writing code.
Rules live in `AGENTS.md`. The map of code that exists today is `docs/agent-guide.md`.
Do not treat `PLAN.md` as the list of things that already work.

Local web UI for the full connected-device workflow: see what is plugged in,
choose what to compile, build, flash, read sizes and errors, and open the
device shell. It wraps the tools that already exist. It does not replace
`make usb`.

## What this must do

- Start with `make web` and open a page on this machine.
- Detect USB serial devices and match them to a board profile.
- Show device info (board, arch, port, flash, LCD, pins, Wi-Fi secrets status).
- Choose which apps are compiled into the image.
- Choose the app that starts after boot.
- Choose kernel and feature flags that actually change the compile.
- Show kernel and per-app size before compile (estimate) and after compile (measured).
- Build and flash, with a live log and structured errors.
- Open a TTY console on the flashed board.
- Keep one operator, one serial port, one job at a time.

## Reuse (do not reimplement)

| Need | Existing code |
| --- | --- |
| Board profile, pins, app fit | `device_config.yaml`, `scripts/device_config.py` |
| Wi-Fi and link key | `device_secrets.yaml`, `scripts/device_secrets.py` (never echo secrets) |
| Build + USB upload | `scripts/usb_flash.py`, `platformio.ini`, `scripts/nodemcu_upload.py` |
| Which stdapps exist | `apps/stdapps/*/app.json`, `scripts/ardubot.py list` |
| Enabled-app macros | `scripts/pio_app_manifest.py`, `apps/stdapps/CMakeLists.txt` |
| Build stamp | `scripts/pio_build_info.py`, `scripts/gen_build_info.py` |
| Encrypted shell | `scripts/link_monitor.py`, `scripts/link_codec.py` |
| Feature menus | `Kconfig`, `cmake/kconfig.cmake` |
| Port scan | `make usb-ports` / `list_serial_ports()` |

PlatformIO is the hardware compiler. The web server drives it. The host
simulator (`make run`) stays a separate target and is not flashed over USB.

## Locked decisions

1. **Local only.** `make web` binds `127.0.0.1` on a free port (default 8765).
   No auth token until a non-loopback bind exists. `--host` other than
   loopback is refused in v1.
2. **Python server, static page.** One process under `scripts/` serves the UI
   and a small JSON/SSE API. No Node, no extra web framework unless stdlib
   proves insufficient for websockets (TTY). If a dependency is required, pin
   it in the existing `.venv-pio` / `scripts/install_deps.sh` path.
3. **One job.** Build, flash, and TTY share the serial port. Starting a second
   job returns a busy error. Cancel kills the child process group.
4. **Profiles stay YAML.** The UI reads and writes `device_config.yaml` fields
   the flash scripts already understand (`device`, `lcd`, `inputs`, `apps`,
   plus new `main_app` and `features`). Secrets stay in `device_secrets.yaml`.
   The API returns `secrets_present: true/false` and never the password or
   link key.
5. **Selections must affect the image.** App checkboxes write `apps:` and the
   PlatformIO enabled-app macros. Feature checkboxes write a generated header
   or build flags the NodeMCU env compiles. A control that only changes the
   page is a bug.
6. **Size is two numbers, two moments.** Before: last measured size, or an
   estimate from the previous `.pio` map plus a flash budget. After: parse the
   PlatformIO memory summary and `firmware.map` into kernel vs each app vs
   free flash/RAM. Label estimate vs measured. Do not invent bytes.
7. **TTY is the link shell**, not a raw UART dump, when a link key exists.
   Raw monitor (`scripts/ardubot.py monitor`) is a fallback tab when the key
   is missing.
8. **NodeMCU / ESP8266 first.** The API is board-shaped (`pio_env`, arch,
   flash size) so ESP32 can be added later without a second UI.

## Screens

1. **Devices** — live list of USB ports, which profile they match, connect state.
2. **Device** — identity, flash geometry, LCD, buttons, pin conflicts, secrets status.
3. **Image** — apps, main app, features, size preview, build and flash actions.
4. **Job** — progress, streaming log, compiler/linker/upload errors, size result.
5. **Console** — TTY (and raw monitor fallback).

---

## Phase 0 — Contract

Outcome: the API and YAML fields are named before UI work, so later phases
do not invent a second config format.

- [ ] Add `docs/web.md` outline (one page): `make web`, bind address, screens, what is written to YAML.
- [ ] Define job states: `idle`, `building`, `uploading`, `failed`, `flashed`, `cancelled`.
- [ ] Define the JSON shapes for device, app, feature, size report, diagnostic, job.
- [ ] Add YAML keys the flash path will honor:
  - `apps:` explicit stdapp list (already supported by `resolve_apps`).
  - `main_app:` name of the app started after boot (new; default `launcher`).
  - `features:` map of Kconfig symbol to value (new).
- [ ] State that `device_secrets.yaml` is read-only from the UI in v1 (status only).

## Phase 1 — `make web`

Outcome: `make web` serves a page on localhost and shuts down cleanly.

- [ ] Add `scripts/web_server.py` (argparse: `--host`, `--port`, `--config`, `--open`).
- [ ] Add `make web` to the Makefile. Default host `127.0.0.1`, port `8765`. Pass `DEVICE_CONFIG`.
- [ ] Refuse non-loopback `--host` in v1.
- [ ] Serve `tools/web/` static files (HTML, CSS, JS). No build step.
- [ ] `GET /api/health` returns version, repo root, platformio found or not.
- [ ] Print the URL. Open the browser when stdout is a TTY (`--open`, default on).
- [ ] Document `make web` in the Makefile `help` target and `README.md`.
- [ ] Test: server binds, `/api/health` is JSON, second bind fails with a clear message, process exits on SIGINT.

## Phase 2 — Detect connected devices

Outcome: the Devices screen lists real USB ports and the profile they match.

- [ ] `GET /api/ports` wraps `list_serial_ports()` (same globs as `make usb-ports`).
- [ ] `GET /api/devices` returns the active `device_config.yaml` plus whether `device.port` exists, is `auto`, or is missing.
- [ ] Poll from the page (about 1s) or SSE `GET /api/events`. Show plug and unplug without reload.
- [ ] Mark the port `resolve_port()` would pick. Allow picking another detected port for this session (same as `PORT=`).
- [ ] If no port is present, show the same recovery text `usb_flash.py` already prints (unplug, wait, replug, not a hub).
- [ ] Surface `pin_conflict_warnings()` on the device card.
- [ ] Test with a fake port list (do not require hardware in unit tests).

## Phase 3 — Device info

Outcome: the Device screen shows everything `make device-config` shows, plus live port state.

- [ ] Show name, arch, board, `pio_env`, baud, `flash_baud`, `flash_size`, `flash_mode`.
- [ ] Show LCD type, size, bus, address, pin labels and resolved GPIO numbers.
- [ ] Show each button (name, pin, active level, pull).
- [ ] Show secrets status: file present, ssid set, link key set. Never return secret values.
- [ ] Show toolchain status: PlatformIO on PATH, ESP8266 toolchain runnable, Rosetta warning on Apple Silicon (`check_xtensa_toolchain_arch`).
- [ ] Read-only in v1 for pins and secrets. Port override is the only device field the UI writes before flash (stored as the session `PORT`, not silently rewritten into YAML until the user clicks Save).
- [ ] `POST /api/device/port` sets the session port and validates the path exists.
- [ ] Test: a sample YAML renders the fields above; a missing secrets file is a warning, not a crash.

## Phase 4 — Select apps to flash

Outcome: checking an app changes which stdapps are compiled in.

- [ ] `GET /api/apps` lists `apps/stdapps/*` via `app.json`: name, title, description, version, depends, `min_display`, enabled, exclude reason.
- [ ] Default selection matches `resolve_apps()` (explicit `apps:` or display-fit).
- [ ] UI: checkbox per app, disable when the display is too small, show the reason from `resolve_apps`.
- [ ] Dependency check: enabling an app enables or warns on `depends`. Disabling an app warns if another enabled app depends on it.
- [ ] `launcher` stays available. The UI explains it is the shell other apps return to.
- [ ] `POST /api/apps` writes `apps:` into `device_config.yaml` (backup the previous file beside it once, e.g. `device_config.yaml.bak`).
- [ ] Drive the PlatformIO image from that list. See **Enabled apps on PlatformIO** in For agents. `platformio.ini` currently hardcodes `-DARDUBOT_APP_*_ENABLED` and `build_src_filter` entries; a pre script must own both, or the checkbox does nothing.
- [ ] `scripts/pio_app_manifest.py` only emits manifests for enabled apps. It includes every `app.json` today (`write_manifest` on all of `apps/stdapps`).
- [ ] Test: toggling an app changes `resolve_apps()` output and the generated source filter. Unknown names are rejected.

## Phase 5 — Select main app

Outcome: the image boots into the chosen app.

- [ ] `main_app` in YAML. Allowed values: an enabled app name. Default `launcher`.
- [ ] UI control on the Image screen, only listing enabled apps.
- [ ] Generate a macro in `build/generated/device_config.h` (name to confirm against `write_header()`).
- [ ] Boot path: `stdapps_start_name()` in `apps/stdapps_register.c`. When `ARDUBOT_MAIN_APP` is defined, return that string literal. When it is not defined, keep the current chain (sensors, then info, clock, pomodoro, launcher). `tests/unit/test_stdapps_register.c` asserts `"sensors"` on a full image; do not break that. Fall back to `launcher` only when the named app is not enabled.
- [ ] If the user disables the current main app, the UI switches the selection back to `launcher` and says so.
- [ ] Test: header contains the chosen name; a unit test of the start-name helper returns it; an unknown name falls back.

## Phase 6 — Select features to compile

Outcome: feature checkboxes change the firmware, using the Kconfig menus as the catalog.

- [ ] `GET /api/features` walks `Kconfig` menus that apply to the device arch (Kernel, Memory, Display, Storage, Developer Helpers). Skip Simulator-only symbols when arch is not `sim`. `Kconfig` is not applied to PlatformIO today (`cmake/kconfig.cmake` is the CMake path only). Do not shell out to `menuconfig`. Read the file, or use `tools/kconfiglib` if it is already present.
- [ ] Each row: symbol, type (bool/int), prompt, default, current value, range when present.
- [ ] Seed current values from YAML `features:`, else Kconfig defaults.
- [ ] `POST /api/features` validates types and ranges, writes `features:`, and generates `build/generated/web_features.h` (or the existing autoconf header) included by the PlatformIO build.
- [ ] Map the symbols that already have a compile effect first: `HEAP_SIZE_KB`, `TICK_RATE_HZ`, `ENABLE_POWER_MANAGEMENT`, `USE_LITTLEFS`, `USE_FATFS`, display driver flags, helper flags. Symbols with no source effect yet are listed as "not wired" and cannot be toggled.
- [ ] Display size in the header stays owned by `device_config.yaml` `lcd.width` / `lcd.height` (`APP_DISPLAY_WIDTH` / `APP_DISPLAY_HEIGHT`). Do not let a feature override the LCD the device profile describes.
- [ ] Changing features invalidates the size preview (mark it stale until the next build).
- [ ] Test: an out-of-range int is rejected; a bool feature changes the generated header; an unwired symbol is not writable.

## Phase 7 — Size before compile

Outcome: the Image screen shows a flash and RAM budget and the last known split before the user builds.

- [ ] Show device `flash_size` as the budget (4MB on the current NodeMCU profile) and the PlatformIO flash layout the env actually uses (`eagle.flash.4m.ld`).
- [ ] If `.pio/build/<env>/firmware.map` and the last PlatformIO size line exist, show them as **last build**: total flash, total RAM, and the kernel vs app split from Phase 9's parser.
- [ ] If they do not exist, show budget only and "no measured size yet".
- [ ] When the user changes apps or features, keep the last measured numbers on screen and badge them **stale**.
- [ ] Optional estimate: sum the previous `.o` sizes for kernel sources vs enabled app sources when those objects exist. Label **estimate**. Omit the row when the objects are absent.
- [ ] Do not block flash on a missing estimate.
- [ ] Test: parser fixture for "no map", "stale map", and a saved size report.

## Phase 8 — Build, flash, errors

Outcome: one button builds and flashes the current profile, and failures are readable.

- [ ] `POST /api/jobs` starts build-only or build-and-flash. Body: session port, upload bool. Reject when a job is active or the port is held by the console.
- [ ] Job runs the same steps as `usb_flash.py`: pin warnings, `write_header`, secrets header (required off-sim), PlatformIO `run -e <pio_env>`, upload via `scripts/nodemcu_upload.py` when a port is present.
- [ ] Stream stdout and stderr on `GET /api/jobs/current/log` (SSE). Redact secret-looking lines if any tool prints them.
- [ ] `POST /api/jobs/current/cancel` terminates the process group and marks `cancelled`.
- [ ] Parse diagnostics into `{severity, stage, file, line, message}` for:
  - missing `device_config.yaml` / `device_secrets.yaml`
  - pin conflicts
  - PlatformIO or toolchain missing (include the Rosetta hint)
  - compiler and linker errors (gcc `file:line: error:`)
  - undefined reference / region overflow (`section .text will not fit`)
  - upload failures (port vanished, CH340 reset hint already in `usb_flash.py`)
- [ ] UI Job screen: stage (configure, compile, link, upload), raw log, diagnostic list. Click a diagnostic to highlight the log line.
- [ ] No port: build still runs, upload is skipped, job ends `failed` or `built` with an explicit "not flashed" reason. Match current `usb_flash.py` behavior and make the reason visible in the UI.
- [ ] Success: job state `flashed`, firmware path, elapsed time, and a link to the size report and the console.
- [ ] Test: a fake compiler log yields diagnostics; cancel sets `cancelled`; a second `POST /api/jobs` while busy returns 409.

## Phase 9 — Size after compile

Outcome: a successful link shows kernel size and each app's size against the budget.

- [ ] Capture the PlatformIO "Flash" / "RAM" summary lines from the job log.
- [ ] Parse `.pio/build/<env>/firmware.map` (already requested in `platformio.ini`).
- [ ] Split contributions:
  - **kernel** — objects from `kernel/` and `hal/`
  - **apps** — one row per enabled stdapp (`apps/stdapps/<name>/`)
  - **runtime** — `apps/` files that are not a stdapp (launcher shell, UI, services)
  - **other** — everything else
- [ ] Report text, data, and bss where the map has them. Show flash used, flash budget, RAM used, percent.
- [ ] Compare to the previous successful report: delta per row (bytes and sign). First build has no delta.
- [ ] Store the report as `build/web/last_size.json` so Phase 7 can show it before the next compile.
- [ ] Overflow: if the link failed because a region overflowed, still show the diagnostic and any partial sizes. Do not show a green total.
- [ ] Test: a checked-in map fixture produces stable kernel and app rows.

## Phase 10 — TTY console

Outcome: the Console screen talks to the board without flashing.

- [ ] `POST /api/console` opens the session port. Fail with a clear error if a flash job holds it or the port is gone.
- [ ] When a link key exists, bridge bytes through `link_monitor.py` / `link_codec.py` (the encrypted shell `make tty` uses).
- [ ] WebSocket (or SSE plus `POST /api/console/input` if websockets are avoided) carries terminal bytes. Browser side: a small terminal view (xterm.js from a vendored file, or a plain `<pre>` plus input if vendoring is too heavy for v1 — prefer a real terminal if the asset can live under `tools/web/vendor/`).
- [ ] Local keystrokes match `tty_local_action` (so the page does not fight the link protocol).
- [ ] Raw monitor tab uses the same port at `device.baud` when the link key is absent. Label it as unencrypted text.
- [ ] Disconnect on page leave and on flash start. Flash is refused while the console holds the port, with a "close console" action.
- [ ] `make tty-run` style one-shot is out of this phase. Interactive session only.
- [ ] Test: codec round-trip without hardware; API returns 409 when the port is busy.

## Phase 11 — Safe session

Outcome: the page cannot brick the workflow or leak secrets, and the user can see what last happened.

- [ ] Confirm secrets never appear in API JSON, logs, or the size report. Add a test that posts a fake password into a log line and expects it redacted.
- [ ] YAML writes are atomic (write temp, rename). Keep one `.bak` of `device_config.yaml`.
- [ ] Image screen shows a dirty state when apps, main app, or features differ from the last flashed job.
- [ ] Job history: last N jobs in `build/web/jobs.json` (state, time, port, error summary, size). No secrets.
- [ ] Empty states: no device, no PlatformIO, secrets missing, build never run, console disconnected.
- [ ] Buttons: Build, Flash, Cancel, Open console, Save profile. Flash is disabled while the form is invalid (unknown main app, pin conflict treated as a warning that still allows flash, matching `usb_flash.py`).

## Phase 12 — Tests and docs

Outcome: `make web` is documented and the non-hardware parts are tested like the rest of the repo.

- [ ] Unit tests for port matching, app resolution writes, feature validation, log diagnostics, map size split, secret redaction.
- [ ] One integration test that starts the server on an ephemeral port and hits health, apps, and features.
- [ ] Hardware flash and live TTY stay manual. Document the manual check in `docs/web.md`: plug in NodeMCU, select apps, flash, confirm size, open console.
- [ ] Update `README.md`, Makefile `help`, and `docs/agent-guide.md` with `make web`.
- [ ] `docs/web.md` lists the API routes and the YAML keys from Phase 0.
- [ ] Warnings-as-errors on any C added for `main_app`. Python stays consistent with `scripts/usb_flash.py` style.

---

## For agents

Implement in the phase order above. A phase is done when its tests pass and
the checkbox behavior changes the image, the log, or the YAML — not when the
page merely renders the control.

### Rules that bind this work

- C changes are TDD-gated. `scripts/tdd_check.py` rejects a new or staged `.c`
  outside `tests/` unless a matching `tests/unit/test_*.c` already contains
  `RUN_TEST` / `TEST_ASSERT`. For `stdapps_start_name()`, extend
  `tests/unit/test_stdapps_register.c` in the same change. Python is not
  gated by that hook; still add `tests/unit/test_*.py` for new script behavior.
- Warnings are errors on C (`-Wall -Wextra -Wpedantic -Werror`). Unused
  parameters need `(void)param;`.
- Host tests: `cmake -B build -DARDUBOT_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ctest --test-dir build --output-on-failure`.
  Python tests are run directly, the same way as the existing ones:

  ```bash
  python3 tests/unit/test_device_config.py
  python3 tests/unit/test_device_secrets.py
  ```

- New `tests/unit/test_*.c` files are globbed by `tests/unit/CMakeLists.txt`.
  Re-run CMake configure if a new C test is not a target yet.
- Docs in the same change as the public surface: `README.md` (the `make usb`
  section), Makefile `help` (the target list around the `usb` / `tty` lines),
  `docs/agent-guide.md` (the "USB / hardware" section), and new `docs/web.md`.
  Leave `PLAN.md` alone.
- Do not put Wi-Fi passwords or `link.key` in the repo, the API, or logs.
  `device_secrets.yaml` is gitignored. Copy `device_secrets.yaml.example`.

### Files to add

| Path | Role |
| --- | --- |
| `scripts/web_server.py` | HTTP server, JSON API, job process, port lock |
| `scripts/web_jobs.py` | Build/flash subprocess and log parsing (keeps the server thin) |
| `scripts/web_sizes.py` | `firmware.map` and PlatformIO size-line parser |
| `scripts/pio_enabled_apps.py` | PlatformIO `pre:` script: enabled apps, `ARDUBOT_MAIN_APP`, feature header |
| `tools/web/index.html` | The five screens, one static page |
| `tools/web/app.js` | Fetch + SSE client |
| `tools/web/app.css` | Layout |
| `tests/unit/test_web_jobs.py` | Diagnostics, cancel, busy |
| `tests/unit/test_web_sizes.py` | Map fixture |
| `tests/unit/test_web_config.py` | YAML surgical edit, feature validation, secret redaction |
| `docs/web.md` | Operator and API doc |

`make web` belongs next to the `usb` target in the Makefile (`.PHONY` list
and `help`). It should call `python3 scripts/web_server.py` with
`--config $(DEVICE_CONFIG)` and `--port` from `WEB_PORT` (default `8765`).

### Files to edit, and the symbol to call

| Change | Where |
| --- | --- |
| Port list, profile, app fit, pin warnings, header | `scripts/device_config.py`: `list_serial_ports`, `resolve_port`, `load_device_config`, `resolve_apps`, `load_stdapp_packages`, `pin_conflict_warnings`, `generate_header`, `write_header`, `KNOWN_TARGETS` |
| Secrets status, secrets header | `scripts/device_secrets.py`: `load_wifi_secrets`, `credentials_error`, `load_link_secrets`, `link_key_error`, `write_secrets_header`, `SecretsError` |
| Build and upload | `scripts/usb_flash.py`: `find_platformio`, `run_platformio`, `pio_env_for_target`, `check_xtensa_toolchain_arch`. Call these. Do not fork a second PlatformIO invocation style. |
| Upload reset | `scripts/nodemcu_upload.py` (already the `upload_command` in `[env:nodemcu]`) |
| Shell | `scripts/link_monitor.py`: `tty_local_action`, `tty_present`, `session_target`. Session bytes go through `scripts/link_codec.py` `Link.open_usb`. |
| Boot app | `apps/stdapps_register.c` `stdapps_start_name`, declared in `apps/stdapps_register.h`. Sim calls it from `sim/sim_main.c`. |
| Install list | `stdapps_install()` in the same file. One `#ifdef ARDUBOT_APP_<NAME>_ENABLED` block per app. |
| Manifests | `scripts/pio_app_manifest.py` → `scripts/app_manifest.py` `write_manifest` |
| Build stamp | `scripts/pio_build_info.py` → `scripts/gen_build_info.py` `write_build_info` |
| Feature catalog | `Kconfig` (menus start at "Kernel Configuration", "Memory Configuration", "Display Configuration", "Storage Configuration", "Developer Helpers", "Simulator Configuration") |
| Image contents | `platformio.ini` `[env:nodemcu]` and `[env:esp32-c6]` |

`docs/agent-guide.md` says the NodeMCU image does not host the app framework.
`platformio.ini` `[env:nodemcu]` already compiles `apps/stdapps_register.c`,
`apps/app.c`, and several stdapps. Trust `platformio.ini` for what the USB
image contains. Update that sentence in `docs/agent-guide.md` when `make web`
can flash those apps.

### YAML the scripts can actually read

`load_device_config()` uses `parse_simple_yaml()`, not PyYAML. Supported
shape: nested maps, lists of scalars, and lists of maps (see
`tests/unit/test_device_config.py`). Flow style (`apps: [a, b]`), anchors,
and folded strings will fail the next `make usb`.

There is no writer. Add a surgical update that replaces only the `apps:`,
`main_app:`, and `features:` blocks and leaves every other line unchanged,
including comments. Round-trip the result through `parse_simple_yaml` in the
test. Do not regenerate the whole file with another library.

`apps:` is a list of scalars. `resolve_apps()` already prefers it over
display-fit. `features:` is a map of scalars (`true` / `false` / ints).
`main_app:` is one scalar. Example of what the parser accepts:

```yaml
apps:
  - launcher
  - counter
  - clock

main_app: clock

features:
  HEAP_SIZE_KB: 32
  ENABLE_POWER_MANAGEMENT: true
```

Write `device_config.yaml` by temp file in the same directory, then
`os.replace`. Copy the previous bytes to `device_config.yaml.bak` once per
save. Never write `device_secrets.yaml` from the web UI.

### Enabled apps on PlatformIO

CMake already does this. `apps/stdapps/CMakeLists.txt` reads
`ARDUBOT_ENABLED_APPS`, always keeps `launcher`, and writes
`build/generated/ardubot_enabled_apps.h` with
`#define ARDUBOT_APP_<NAME>_ENABLED 1`. The Makefile passes that list for
`ARCH != sim` via `scripts/device_config.py apps --cmake-list`.

PlatformIO does not read that header. `apps/stdapps_register.c` skips it
when `ARDUBOT_PIO` is set and uses compiler defines instead. Those defines
and the `+<../../../apps/stdapps/...>` lines are hardcoded in
`platformio.ini`.

Add `scripts/pio_enabled_apps.py` as a `pre:` extra script on `[env:nodemcu]`
and `[env:esp32-c6]`, next to `pio_app_manifest.py` and `pio_build_info.py`.
The script should:

1. `load_device_config("device_config.yaml")` and `resolve_apps()`.
2. Always include `launcher` (same rule as the CMake file).
3. `env.Append(CPPDEFINES=[("ARDUBOT_APP_<NAME>_ENABLED", 1)])` for each
   enabled app that `stdapps_install()` knows how to install.
4. Append that app's `*_app.c` and `*_icon.c` (and any extra `.c` in the
   same directory, such as `clock/clock.c`) to the source filter.
5. If `main_app` is set and enabled, append
   `("ARDUBOT_MAIN_APP", '\\"name\\"')` so the macro is a C string.
6. Include `build/generated/web_features.h` when `features:` is non-empty.

Then delete the matching hardcoded `-DARDUBOT_APP_*` flags and per-app
`build_src_filter` lines from those two envs so the script is the only
source of truth. Leave kernel, HAL, and `apps/*.c` runtime sources in the
ini. Those are not optional apps.

Only offer apps that `stdapps_install()` can register. Today that is
`launcher` (always), `counter`, `info`, `stopwatch`, `pong`, `widgets`,
`pomodoro`, `taskmgr`, `clock`, `sensors`. `settings`, `fileman`, `shell`,
and `demo` are in `ARDUBOT_ALL_STDAPPS` but `test_stdapps_register.c` asserts
they are not installed. Do not show them as flashable until an
`ARDUBOT_APP_<NAME>_ENABLED` block exists. See `docs/agent-guide.md`
"Add a built-in app".

`[env:nodemcu]` currently lists counter, info, stopwatch, clock, and
launcher. `[env:esp32-c6]` lists the full install set. After the script
lands, both envs follow `device_config.yaml`. The sim image stays
"compile every stdapp" (`Makefile` only passes `ARDUBOT_ENABLED_APPS` when
`ARCH != sim`).

### Boot app

`stdapps_start_name()` is a compile-time chain, not a runtime setting.
`docs/apps.md` and `README.md` document that a full image starts `sensors`.
Keep that when `ARDUBOT_MAIN_APP` is absent. The web UI sets the macro only
for the PlatformIO build it launches.

Check the chosen name against the enabled set before writing the header.
`generate_header()` does not emit app macros today; either add
`#define ARDUBOT_MAIN_APP "name"` there or emit it from
`pio_enabled_apps.py`. One of those, not both.

### Features that are real

`Kconfig` symbols are not on the PlatformIO command line. A checkbox is
"wired" only after some source reads the generated macro. Start with
symbols the tree already branches on, and mark everything else
`wired: false` in the API so the page cannot toggle it.

Search before claiming a symbol is wired (`rg` the config name). Likely
first set, only if the search hits a real `#if` / `#ifdef`:

- `HEAP_SIZE_KB`, `TICK_RATE_HZ`, `ENABLE_POWER_MANAGEMENT`, `ENABLE_TICKLESS_IDLE`
- `USE_LITTLEFS`, `USE_FATFS`
- `DISPLAY_DRIVER_SSD1306`, `DISPLAY_DRIVER_ILI9341`, `DISPLAY_DRIVER_ST7789`

LCD pixel size is `lcd.width` / `lcd.height` in YAML, emitted as
`ARDUBOT_LCD_WIDTH` / `ARDUBOT_LCD_HEIGHT` by `generate_header()`.
`platformio.ini` also passes `APP_DISPLAY_WIDTH` and `APP_DISPLAY_HEIGHT`
(128×32 on nodemcu, 320×172 on esp32-c6). Do not let a feature override
those. If the UI ever edits LCD size, it must change both the YAML and
those two `-D` flags together.

`USE_FATFS` is not a real FAT filesystem yet. `docs/agent-guide.md` says
`fs/fatfs/fatfs_vfs.c` is a raw passthrough. Leave the toggle unwired
until that changes.

### Sizes

`[env:nodemcu]` already links with `-Wl,-Map=.pio/build/nodemcu/firmware.map`.
After a successful `platformio run -e nodemcu`, the log contains a memory
summary (Flash and RAM percent). Parse that line and the map. Group object
paths:

- `kernel/`, `hal/` → kernel
- `apps/stdapps/<name>/` → that app
- other `apps/` → runtime
- else → other

Store `build/web/last_size.json`. The "before" panel reads that file. If it
is missing, show `device.flash_size` from YAML (NodeMCU profile: `4MB`) and
the text "no measured size yet". The linker script in the nodemcu env is
`eagle.flash.4m.ld`. Quote the measured section sizes; do not convert a
percent into bytes unless the log also prints the byte counts.

### Jobs

`run_platformio()` in `scripts/usb_flash.py` is the command shape:
`platformio run -e <env> -d <repo> [-j N] [-t upload --upload-port <port>]`.
`find_platformio()` resolves the CLI (`platformio` on PATH, else
`python -m platformio`). The repo venv from `scripts/install_deps.sh` is
`.venv-pio`.

One job at a time. The console and the job share a lock named by the port
path. `POST` while busy returns HTTP 409. Cancel sends SIGTERM to the
process group, then SIGKILL. On Windows, `terminate()` the process and
document that process-group kill is POSIX.

Stream logs with `text/event-stream`. Parse gcc lines
`file:line:column: error: message` and linker lines containing
`undefined reference` or `will not fit`. Reuse the human text already in
`usb_flash.py` for a missing port, a missing PlatformIO, and the Apple
Silicon Rosetta case (`check_xtensa_toolchain_arch`, `ROSETTA_HINT`).

No serial port: still compile, skip upload, finish as `built` with reason
`not flashed`. That matches `usb_flash.py`, which falls back to
`upload=False`.

### Console

`make tty` runs `scripts/link_monitor.py`, which opens `Link.open_usb()`
from `scripts/link_codec.py`. That path uses `termios` and `fcntl`. It is
Unix-only. On Windows the console API returns a clear error. Do not import
`termios` at import time in `web_server.py`; CI is Ubuntu, Windows, and
macOS (`.github/workflows/sim.yml`), and a top-level import breaks Windows
collection if a test imports the server.

Ctrl+] is exit (`tty_local_action`, byte `0x1D`). Send every other byte to
the board. `tty_present()` turns bare newlines into CR LF and treats EOT
(`0x04`) as close. The web terminal must use those two functions so the
page matches `make tty`.

Opening the port can reset the board. `docs/agent-guide.md`: the host keeps
sending Hello until the board answers. Reuse that handshake inside `Link`.
Do not write a second crypto session.

Raw monitor (`python3 scripts/ardubot.py monitor`) is the fallback when
`load_link_secrets()` raises or the key is empty. Baud is `device.baud`
(115200 in the current YAML).

### HTTP

Stdlib only (`http.server`). No new package. Console input is
`POST /api/console/input` with a base64 body, and output is SSE, unless
you later vendor a terminal. Do not add npm or a bundler.

Bind `127.0.0.1` only. Reject other hosts with exit code 2.

Suggested routes:

| Method | Path | Calls |
| --- | --- | --- |
| GET | `/api/health` | `find_platformio()` |
| GET | `/api/ports` | `list_serial_ports()` |
| GET | `/api/devices` | `load_device_config`, `resolve_port`, `pin_conflict_warnings` |
| POST | `/api/device/port` | session override, same role as `PORT=` |
| GET | `/api/apps` | `load_stdapp_packages`, `resolve_apps` |
| POST | `/api/apps` | surgical YAML `apps:` |
| GET/POST | `/api/features` | `Kconfig` plus `features:` |
| GET/POST | `/api/main-app` | `main_app:` |
| GET | `/api/size` | `build/web/last_size.json` or budget only |
| POST | `/api/jobs` | `run_platformio` / `usb_flash` steps |
| GET | `/api/jobs/current` | job state |
| GET | `/api/jobs/current/log` | SSE |
| POST | `/api/jobs/current/cancel` | kill child |
| POST | `/api/console` | `session_target`, `Link.open_usb` |
| POST | `/api/console/input` | `tty_local_action` |
| GET | `/api/console/output` | SSE of `tty_present` |
| POST | `/api/console/close` | close the fd |

### Tests an agent can run without a board

- Parser and YAML edit tests follow `tests/unit/test_device_config.py`
  (`unittest`, `tempfile`, `sys.path` insert of `scripts/`).
- Size tests read a small map fixture under `tests/unit/fixtures/`. Do not
  commit a real `.pio` tree.
- Diagnostic tests feed a string, not a compiler.
- Server test binds `127.0.0.1` port `0`, hits `/api/health` and `/api/apps`,
  then closes the socket. Do not open a serial device.
- After touching `stdapps_register.c`: `cmake --build build -j --target test_stdapps_register && ./build/tests/unit/test_stdapps_register`.
- Flash and live TTY stay a manual note in `docs/web.md`.

### Do not

- Do not flash from the test suite.
- Do not call `menuconfig` or rewrite `Kconfig` as part of the web UI.
- Do not change sim app policy (sim still builds every stdapp).
- Do not add a second device-config language. `device_config.yaml` stays the profile.
- Do not bypass `scripts/tdd_check.py` or commit with `--no-verify`.
- Do not print `link.key`, `wifi.password`, or the generated secrets header.

## Suggested build order

Phases 1 → 2 → 3 unblock the page. Phase 4 is the first change that alters the binary. Phase 5 and 6 follow because they share the generated-header path. Phase 8 is the job runner. Phase 7 reads what Phase 9 writes, so implement the parser in Phase 9 and the "last build" panel immediately after. Phase 10 needs the port lock from Phase 8. Phase 11 and 12 close the loop.

## Done when

`make web` opens a localhost page that sees the NodeMCU port, shows the profile, lets the user pick apps, the boot app, and wired features, builds and flashes through PlatformIO, lists compiler and upload errors, shows kernel and per-app size before (last or estimate) and after (measured), and opens the link TTY on that port.
