# ArdubotOS

A multi-architecture embedded OS (AVR / ESP8266 / ESP32) built simulator-first:
an SDL2 host emulator runs the exact same app runtime that ships to a real
board, so apps are written and iterated on without hardware in the loop.

Current real hardware target: NodeMCU (ESP8266) with an SSD1306 128×32 OLED.
See [`PLAN.md`](PLAN.md) for the full architecture and roadmap, and
[`docs/agent-guide.md`](docs/agent-guide.md) for an honest map of what's
implemented versus still planned.

## Quick start

```bash
make run    # build (Debug, sim) and open the SDL2 emulator
make test   # build and run the headless test suite
```

Panel controls match the board: Up/Down move, Select launches, hold
Select/Escape to go back.

## Documentation

| Doc | What's in it |
|---|---|
| [`PLAN.md`](PLAN.md) | Locked architecture decisions and the phased (0–7) roadmap |
| [`docs/agent-guide.md`](docs/agent-guide.md) | What's actually implemented and tested vs. still roadmap |
| [`docs/architecture.md`](docs/architecture.md) | System architecture overview, with diagrams of the layer stack, the cooperative scheduler's boot/switch flow, and the TLSF allocator |
| [`docs/appkit.md`](docs/appkit.md) | The app runtime (`app_kit`, `app_ui`, focus/key-bind model) |
| [`docs/apps.md`](docs/apps.md) | The built-in stdapps |
| [`docs/tutorials/writing-an-app.md`](docs/tutorials/writing-an-app.md) | Step-by-step: scaffold, write, run, and test one app from nothing |
| [`AGENTS.md`](AGENTS.md) | Build/test commands, TDD gate, repo layout (for AI agents and humans) |

Generated reference docs (not checked in - build them locally):

```bash
make docs           # Doxygen API reference + mkdocs site
make docs-doxygen   # -> docs/doxygen/html/index.html (needs: brew install doxygen)
make docs-site      # -> site/index.html (needs: pip install -r docs/requirements.txt)
```

## Writing an app

New to the codebase? [`docs/tutorials/writing-an-app.md`](docs/tutorials/writing-an-app.md)
walks through building one small app end to end (scaffold, state, a
background task, installing it, and a test) before you need the full
reference below.

Apps live under `apps/stdapps/<name>/` and are built on the `app_ui` kit
(`apps/app_ui.h`) - row-based text (`app_ui_line`/`app_ui_linef`, always
scale-correct, no hand-computed pixel math) and table-based key binding
(`app_ui_bind_keys`). Scaffold one instead of copying an existing app by hand:

```bash
make create-app NAME=mygame
```

This writes `apps/stdapps/mygame/{mygame_app.c, mygame_icon.c, app.json}`,
registers it in `apps/stdapps/CMakeLists.txt`, and prints the two lines to add
to `sim/sim_main.c` to actually install it (install is a separate, explicit
step on purpose - see `sim/sim_main.c`'s comments on why only four of the
nine built-in apps are installed today).

### Package manager

Every stdapp carries an `app.json` (name/version/author/depends/
min_display). The compile copies name, version, author, and description
into the app manifest, so those fields are not repeated in `APP_HELPER`.
There's no hosted registry yet, so `ardubot install` only
resolves a local directory:

```bash
make list-apps                       # what's installed, with versions/deps
make install-app SRC=../some-app      # copy it in + register it (checks `depends`)
```

### Device-aware builds

A device profile (`device_config.yaml`) can restrict which stdapps actually
get *compiled* in - not just hidden at runtime. Without an explicit `apps:`
list, every stdapp whose `app.json` `min_display` fits the configured
`lcd.width`/`height` is included automatically:

```bash
python3 scripts/device_config.py apps   # show what would be compiled in, and why
```

This only affects real-hardware builds (`ARCH != sim`); the simulator always
builds every stdapp since it's the dev-iteration target.

## On device (NodeMCU / ESP8266)

`device_config.yaml` is the board profile (serial port, LCD, buttons, and
optionally which apps to compile in). `device_secrets.yaml` holds Wi-Fi
credentials and is required for flashing:

```bash
cp device_secrets.yaml.example device_secrets.yaml   # fill in ssid/password
cp device_config.yaml.example device_config.yaml     # fill in your board's port
```

```bash
python3 scripts/ardubot.py flash --device nodemcu   # or: make usb DEVICE=nodemcu
python3 scripts/ardubot.py monitor                   # serial monitor (needs pyserial)
make usb-ports                                       # list candidate serial ports
```

`make usb` with no `DEVICE` asks which target to compile. Override the port
with `PORT=/dev/cu.usbserial-0001` (or leave `device.port: auto`).

The status bar shows a Wi-Fi glyph, signal bars, and battery. Without
`device_secrets.yaml` the icons show Wi-Fi off; on the board the bars follow
the live signal, and the emulator shows "associated" once the file is filled
in.

Apple Silicon needs Rosetta for the ESP8266 toolchain (`Bad CPU type in
executable`):

```bash
softwareupdate --install-rosetta --agree-to-license
```

## Debugging

A crash in the simulator (SIGSEGV/SIGABRT/SIGBUS) writes a symbolized
backtrace to `crash.log` before the process exits - the sim's stand-in for
"crash dump to flash" (there's no real flash sector to target in a host
process, and a fault can happen before the VFS is even mounted). Backtraces
need `execinfo.h` (macOS/Linux); Windows CI still gets the signal logged,
just without frames.

## Tests and coverage

```bash
make test       # headless run of the full suite (ctest)
make coverage   # real gcov/lcov line+branch coverage -> build/coverage/html/
```

As of this writing, all 45 tests in `ctest --test-dir build` pass and
`make coverage` reports **55.0% line / 59.9% function** coverage across 59
host-buildable source files. That number is real (genuine `--coverage`
instrumentation, not the placeholder lcov file `sim_args.c`'s
`--coverage=FILE` flag writes for the headless test runner), but it is
**not, and will not realistically become, 100%**: large parts of
`hal/arch/{esp32,esp8266,avr}/` only compile for their target and can't run
under the host test suite at all, and several kernel/VFS/driver error
branches still require fault injection no test exercises yet. Treat
80% line / 70% branch (the existing gate in `scripts/check_coverage.cmake`)
as the realistic target to raise incrementally, not 100%.

## CLI reference

| Command | Does |
|---|---|
| `make run` / `make test` | Build + run the sim / headless test suite |
| `make usb [DEVICE=x] [PORT=y]` | Build + flash a real board |
| `make monitor` | Serial monitor on the configured device |
| `make create-app NAME=x` | Scaffold a new stdapp |
| `make install-app SRC=path [FORCE=1]` | Install an `app.json` app from a local dir |
| `make list-apps` | List installed stdapps |
| `make docs` | Build Doxygen + mkdocs reference docs |
| `make coverage` | Real lcov line/branch coverage report |
| `python3 scripts/device_config.py apps` | Show which stdapps fit the current device profile |
| `python3 scripts/device_config.py show\|ports\|gen-header` | Device profile inspection |

All of the above are also reachable as `python3 scripts/ardubot.py <flash\|monitor\|create-app\|install\|list>` directly.
