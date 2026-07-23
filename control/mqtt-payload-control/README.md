# mqtt-payload-control

A Xentara C++ control that reacts to a real external AI pipeline instead of
a loopback or a TUI write: it reads the raw JSON `detections` array
published by [wago-hailo-example](https://github.com/WagoAlex/wago-hailo-example)
(a Hailo-8 YOLOv5m helmet detector) over MQTT, and drives three physical
digital outputs from it directly. Validated end to end on real hardware -
see [Validated](#validated) below.

> [!IMPORTANT]
> **Runs on the native Xentara install, not `xentara-tryout` (Docker).** The
> model's `asJSONText` payload property requires `xentara-mqtt-client >=
> 2.0+2.2`. As of this writing, the `xentara/xentara-tryout:latest` image
> ships `2.0+2.1`, which doesn't have it - the model fails to load in that
> container at all. Every other app in this repo uses the Docker container;
> this one is the exception. See the
> [App 4 guide](../../docs/app-mqtt-payload-control.md)
> for the full walkthrough, including why.

## What it does

Once per cycle it reads the `Payload` string (the current frame's
`detections` array as raw JSON text) and does a plain substring search
against three of wago-hailo-example's five class labels:

| Output | Set `true` when the payload contains |
|---|---|
| `Head` | `"head"` (a detected bare head - no helmet) |
| `White` | `"white helmet"` |
| `Blue` | `"blue helmet"` |

> [!NOTE]
> wago-hailo-example's actual class list (`yolov5m-helmet.txt`) is
> `["blue helmet", "head", "red helmet", "white helmet", "yellow helmet"]`.
> This control only acts on three of those five - `"red helmet"` and
> `"yellow helmet"` detections are received (they're part of the same
> `Payload` string) but nothing here reacts to them. Extend `step()` if you
> need those too.

This is a substring match on the whole frame's JSON text, not a per-object
parse - it does **not** track individual people, use `confidence` or `box`,
or distinguish "one person in a white helmet" from "a white helmet visible
anywhere in frame." Multiple simultaneous classes in one frame set multiple
outputs `true` at once (OR semantics, not mutually exclusive). Good enough
for a live indicator light per class; not a substitute for parsing the JSON
properly if you need per-detection logic.

## Build

Same pattern as every control in this repo - no SDK install on the host,
build inside Xentara's build image:

```bash
docker run --rm -v "$PWD":/work -w /work xentara/xentara-build:latest bash -lc '
  cmake -B build-amd64 -G Ninja
  cmake --build build-amd64
  cmake -B build-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/usr/share/xentara/toolchains/arm64-gcc.toolchain.cmake
  cmake --build build-arm64
'
```

Use `build-amd64/libMQTTPayloadControl.so` on an x86 edge computer, the
arm64 one on ARM targets (e.g. a WAGO PFC300).

## Deploy (native install)

Unlike the other controls in this repo, this one goes into the **native**
Xentara instance's directories, not a Docker container's:

1. Copy the module in, named exactly `MQTTPayloadControl.so`:
   ```bash
   scp build-amd64/libMQTTPayloadControl.so \
     <user>@<device-ip>:~/.config/xentara/control/MQTTPayloadControl.so
   ```
2. **Don't hand-load the committed `model/example-mqtt-payload-control.json`
   as-is on different hardware** - its EtherCAT addresses are baked in from
   this repo's own coupler's physical row. Regenerate it for your row first;
   see [Step K in the App 4 guide](../../docs/app-mqtt-payload-control.md#step-k---discover-your-io-modules)
   for the discovery command, using
   [`model/template-mqtt-payload-control.json`](../../model/template-mqtt-payload-control.json).
3. Copy the freshly generated `model.json` in and restart the service:
   ```bash
   scp model.json <user>@<device-ip>:~/.config/xentara/model.json
   ssh <user>@<device-ip> 'sudo systemctl restart xentara@<user>.service'
   ```

> Xentara enrolls exactly one C++ control per instance. Don't try to load
> this alongside another `@Skill.CPP.Control` module.

## Validated

Confirmed end to end against a live camera feed on real WAGO 750-354
hardware: `wago-hailo-example` detections for `"white helmet"` and `"head"`
correctly set `Connection.DO1-White` / `DO3-Head` to `true` (with
`DO2-Blue` correctly staying `false` with no blue helmet in frame), tracked
live frame-to-frame, and the physical outputs were visually confirmed
switching.

Getting this working surfaced two real bugs, both now fixed in this repo:

1. **EtherCAT address drift.** The model was originally hand-written with
   the 8-channel output module's addresses starting right after the K-Bus
   control word - correct for a row with *no* analog module ahead of it.
   This device's actual row is analog → 8-channel digital → 16-channel
   digital, which shifts the real addresses by 4 bytes. Writes silently
   landed on the analog module's word instead of the digital output -
   `xentara-ethercat-device-info` even reported `Invalid sync manager
   configuration` against the stale layout. Fixed by converting the model
   to a proper discovery *template* (like `template-rtt.json`) and
   regenerating it against the live bus, the same way Apps 2/3 do - so it
   can never drift silently again.
2. **Docker image / native install version skew**, as described in the
   warning above - `asJSONText` needs `xentara-mqtt-client 2.0+2.2`, which
   only the native install had.
