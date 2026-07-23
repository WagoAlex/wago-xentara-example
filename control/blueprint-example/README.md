# blueprint-example

The smallest complete Xentara C++ control - the reference implementation for
the [Blueprint](../../README.md#blueprint---build-your-own-c-and-ethercat-control)
section of the top-level `README.md`. Touches **no I/O**, needs **no
hardware**, and needs **no discovery step** - it exists purely to show the
initialize()/step() pattern, data point binding, and pipeline wiring that
every custom control follows, in isolation from EtherCAT specifics.

## What it does

Once per second it reads one input and writes two outputs (visible in the
TUI or Xentara Workbench under the `Registers` group of
[`model/example-blueprint.json`](../../model/example-blueprint.json)):

| Data point | Direction | Meaning |
|---|---|---|
| `Enable` | in | Toggle in the TUI to start/stop the counter |
| `Counter` | out | Increments once per step while `Enable` is true |
| `Active` | out | Mirrors `Enable`, so you can see the input echoed straight back |

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

Use `build-amd64/libBlueprintExample.so` on an x86 edge computer, the arm64
one on ARM targets (e.g. a WAGO PFC300).

## Deploy

1. Copy the module into the Xentara instance's control directory, named
   exactly `BlueprintExample.so` (the model's `controlPath` is a bare
   filename that Xentara resolves against `control/` - do **not** add a
   `control/` prefix):
   ```bash
   docker cp build-amd64/libBlueprintExample.so \
     xentara-tryout:/home/xentara/.config/xentara/control/BlueprintExample.so
   ```
2. Load [`model/example-blueprint.json`](../../model/example-blueprint.json)
   directly - it's a complete, real model, not a generator template, so no
   discovery run is needed:
   ```bash
   docker cp model/example-blueprint.json \
     xentara-tryout:/home/xentara/.config/xentara/model.json
   ```
3. Restart the container and open it in the TUI, or connect Xentara
   Workbench to the device to browse and screenshot the loaded model.

> Xentara enrolls exactly one C++ control per instance. Don't try to load
> this alongside another `@Skill.CPP.Control` module.
