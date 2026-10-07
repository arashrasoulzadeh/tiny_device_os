# Link: host and device communication

Status: **phases 1–4 are in the tree.** The frame, AES-128-GCM, session,
four calls, ESP32-C6 USB bridge, Python SDK, and secrets gate are
implemented. After Hello the board runs `apps/tty_service.c` on
`LINK_CH_TTY`. The post-flash monitor is a raw terminal (Ctrl+] exits).
Sensor samples are sent only when the shell runs `sensors` or
`sensor <key>`. Bluetooth, on-device Wi-Fi, the TypeScript SDK, and
radio (phases 5–8) are not. One language on the wire.
USB is the first transport. Bluetooth, Wi-Fi, and radio stay later
transports of the same frames. Notifications, app messages, app events,
and key input are calls on that language, not separate protocols.

## Problem

The host and the board need to talk after a flash, and later over
Bluetooth, Wi-Fi, and radio. A notification, a message for one app, an
event for one app, and a key press are different deliveries, but they must
share framing, identity, and encryption. Apps and host programs must call
an SDK. They must not write bytes for one port.

Today `apps/notify_service.c` draws an OS card. `sim_gpio_handle_key()` is
how a press reaches the focused app. `os_event_publish()` is the in-process
bus. `hal/include/hal_uart.h`, `hal_ble.h`, and `hal_net.h` are the
transport edges. None of them speaks to the host as a session. BLE can
advertise and does not yet carry payload bytes.

## Goal

- One frame and one session API for every transport.
- C on the device and in the simulator. Python on the host first.
  TypeScript later, generated from the same schema.
- Four calls: `notify.post`, `app.message`, `app.event`, `input.key`.
- A pre-shared communication key. Payloads after Hello are encrypted.
- When `notifications.forward_from_host` is set, `make usb` stays in the
  same process after a successful flash and runs the device shell as a
  terminal until Ctrl+].

## Calls

The host names the call. The device delivers it through the path that kind
already uses.

| Call | Reaches | Device does |
|---|---|---|
| `notify.post` | The OS card, not an app | `notify_post()`. Slides in over the focused app. `duration_ms` 0 holds 3 seconds, then hides. A key dismisses it early. |
| `app.message` | One app, by install name | Queued for that app. The queue accepts the message even if the app is not focused. The app reads it when it runs. |
| `app.event` | One app, by install name | Published as `app.<name>.<event>`. Only a subscriber in that app runs. |
| `input.key` | The focused app | `sim_gpio_handle_key()` with `up`, `down`, `left`, `right`, `enter`, `escape`. Press and release are separate calls. A visible notification consumes the press first, same as a physical button. |

```text
notify.post(title, body, extent, duration_ms) -> ack
app.message(app, text) -> ack
app.event(app, name, payload) -> ack
input.key(key, pressed) -> ack
```

`app` is the install name (`sensors`, `pomodoro`, `clock`). An unknown name
returns an error on that call id. A notification is not an app event. A
message is data the app keeps. An event is a signal it subscribed to. A
key is input.

Wire shape, same on every transport:

```text
magic u16 = 0xAB07
version u8
flags u8          ack-required, error, more-fragments, encrypted
channel u8
msg_id u16
length u16
payload           tagged fields
crc16
```

Payload fields are tagged so a new field does not break an older peer.
Device payload cap is a few hundred bytes. A transport with a shorter
packet (Bluetooth) splits and rejoins a frame inside the adapter. Callers
never see fragments.

Hello is the only cleartext message: version, key id, and which services
this side implements. After Hello both sides send only encrypted frames.
A failed decrypt or a replay is dropped and counted. It is not delivered
as a call.

## Encryption and the communication key

Algorithm: AES-128-GCM. The key is 16 bytes, pre-shared. Nonce is 12
bytes: 4 bytes of direction and session, 8 bytes of counter. Each
direction has its own counter, starting at 1 after Hello. A repeated or
older counter is rejected.

The key does not go in `device_config.yaml`. That file is committed.
`device_secrets.yaml` (gitignored, same file as Wi-Fi) holds it:

```yaml
link:
  key_id: 1
  key: "00112233445566778899aabbccddeeff"   # 16 bytes, hex
```

`make usb` already refuses to flash until Wi-Fi secrets are set. The same
gate applies to `link.key` when `notifications.forward_from_host` is on.
The generated secrets header exposes the key to the firmware. The host
monitor reads the yaml. Both sides use `key_id` in Hello so a later second
key can be added without a new frame.

Rotation is a new `key_id` and a new key in the secrets file, then a
reflash. The device accepts only the key it was built with. There is no
key exchange over the air in these phases.

## Transports

Adapters move frames. They do not interpret calls.

| Transport | When | How |
|---|---|---|
| USB | First | CDC serial on the ESP32-C6. The host opens the same port `make usb` just flashed. |
| Bluetooth | After the USB session is real | GATT write one way, notify the other, same frames. `hal_ble` must grow a byte pipe. Advertising alone is not enough. |
| Wi-Fi | After Bluetooth or in parallel once USB works | TCP on `hal_net` / the Wi-Fi driver. Same frames, length-prefixed on the socket. |
| Radio | Later | Another adapter. No new calls, no new crypto. The radio only has to deliver the frame bytes. |

The ESP32-C6 image is the first device. The NodeMCU image does not run the
app framework, so it does not host the session.

## Host monitor

`device_config` for the board:

```yaml
notifications:
  forward_from_host: true
```

`scripts/device_config.py` emits `ARDUBOT_LINK_FORWARD 1` when that flag is
set. The firmware includes the USB reader only then.

After `scripts/usb_flash.py` compiles and flashes successfully, it stays
on the main thread and runs the monitor. It does not spawn a second
program. Stdin sends calls. Device acks and errors print on stdout.
Ctrl+C closes the port and returns. If the flag is off, flash exits as it
does today.

A bare line is `notify.post` with extent `band` and `duration_ms` 0 (show,
hold 3 seconds, hide). A prefix selects the other calls:

```text
hello world
msg sensors battery low
event pomodoro skip
key enter
key up down
```

`key up down` is press then release.

## SDKs

Generated from one schema in the repo so the opcode and field numbers
cannot drift.

- **C** (`link/` on the device and the simulator). Register a handler.
  Do not parse frames in an app.
- **Python** first host SDK. The monitor is a caller, not a private
  codec.
- **TypeScript** later, same schema, for a desktop or browser tool.

```python
link = Link.open_usb(port, key=bytes.fromhex(secret), key_id=1)
link.call("notify.post", title="hello world", extent="band", duration_ms=0)
link.call("app.message", app="sensors", text="battery low")
link.call("app.event", app="pomodoro", name="skip")
link.call("input.key", key="enter", pressed=True)
```

## Phases

Each phase builds and has tests before the next one starts.

1. **Codec and schema.** Frame encode and decode, Hello, Call, Result,
   Error, and the four calls above. AES-128-GCM with the nonce rule.
   One set of vectors runs in Python and in `tests/unit` for the C codec.
   Cover a bad CRC, a bad tag, and a replayed counter.
2. **USB adapter and device handlers.** ESP32-C6 boot loop reads CDC when
   `ARDUBOT_LINK_FORWARD` is set. `notify.post` calls `notify_post()`.
   `input.key` calls `sim_gpio_handle_key()`. `app.message` queues by app
   name. `app.event` publishes `app.<name>.<event>`.
3. **Python SDK and the monitor.** Secrets gate for `link.key`. `make usb`
   runs the monitor on the same thread when `forward_from_host` is set.
   Ctrl+C exits.
4. **Events the other way.** The device can publish a sensor sample. The
   monitor prints it. Still the same frames and key.
5. **Bluetooth adapter.** Byte pipe on `hal_ble`, same session, same key.
6. **Wi-Fi adapter.** TCP on `hal_net`, same session, same key.
7. **TypeScript SDK** from the schema.
8. **Radio adapter.** Same frames. No protocol change.

## Out of scope for these phases

- Key exchange or pairing. The key is pre-shared and flashed.
- A second copy of the notification UI. The card stays
  `apps/notify_service.c`.
- Teaching the NodeMCU image the session.
- Per-app encryption keys. One device key for the session.
