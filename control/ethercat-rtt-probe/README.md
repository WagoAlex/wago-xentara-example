# ethercat-rtt-probe (optional)

A tiny Xentara C++ control that measures the real EtherCAT **cycle time** and
publishes it as live values. It touches **no I/O**, so every output stays free
for you to edit in the TUI - the probe just tells you how fast the bus is
actually cycling.

You only need this if you want RTT numbers. The core "discover -> edit I/O in
the TUI" journey in the top-level `README.md` needs no custom control at all.

## What it publishes

Once per cycle it writes five registers (visible in the TUI under the `RTT`
group of `model/template-rtt.json`):

| Register | Meaning |
|---|---|
| `RttLastMs` | Last cycle interval (ms) |
| `RttMinMs` / `RttMaxMs` | Min / max seen |
| `RttAvgMs` | Running mean |
| `RttSampleCount` | Cycles measured |

**What "RTT" means:** the interval between successive `step()` calls, i.e. the
pipeline's real achieved cycle time and its jitter around the configured 1 ms
timer. It is not wire propagation delay (sub-microsecond, not observable from
software). On a live bus expect the mean to sit on the timer period, with
`RttMax` tighter the better your real-time tuning.

## Build

No SDK install on the host - build inside Xentara's build image. Produces both
architectures:

```bash
docker run --rm -v "$PWD":/work -w /work xentara/xentara-build:latest bash -lc '
  cmake -B build-amd64 -G Ninja
  cmake --build build-amd64
  cmake -B build-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/usr/share/xentara/toolchains/arm64-gcc.toolchain.cmake
  cmake --build build-arm64
'
```

Use `build-amd64/libEtherCATRttProbe.so` on an x86 edge computer, the arm64
one on ARM targets (e.g. a WAGO PFC300).

## Deploy

1. Copy the module into the Xentara instance's control directory, named exactly
   `EtherCATRttProbe.so` (the model's `controlPath` is a bare filename that
   Xentara resolves against `control/` - do **not** add a `control/` prefix).
2. Use `model/template-rtt.json` (instead of the minimal one) when you run
   discovery, so the model wires `Control.EtherCATRttProbe.step` into the
   pipeline ahead of the bus loop.

> Xentara enrolls exactly one C++ control per instance. Don't try to load this
> alongside another `@Skill.CPP.Control` module.
