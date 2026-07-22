# ethercat-kbus-rtt-probe (optional)

A Xentara C++ control that measures real **hardware** round-trip time on a
K-Bus: a digital output flipped and read back through a wired-back input, and
an analog output stepped (and separately ramped) and read back the same way.
Unlike [`ethercat-rtt-probe`](../ethercat-rtt-probe/), this one needs two
physical loopback wires - it measures how long a value takes to actually
reach the hardware and come back, not just how evenly the software cycles.

You only need this if you want a verified hardware number, and you have the
wiring for it. Everything else in this repo works without it.

## Required wiring

Two loopbacks on the same coupler:

| From | To |
|---|---|
| A digital output | A digital input |
| An analog output | An analog input |

Any spare DO/DI and AO/AI pair works - this repo's own measurements used
`DO_8ch_8 -> DI_8ch_1` and `AO_1 -> AI_1` on a WAGO 750-354 coupler, but the
control doesn't care which physical channels you pick, only that they're
wired to each other.

## What it publishes

Once per cycle it writes the same five `Rtt*` cycle-time registers as
`ethercat-rtt-probe`, plus three groups of round-trip stats (visible in the
TUI under the `RTT` group of `model/template-rtt-kbus.json`):

| Register group | Meaning |
|---|---|
| `KbusConnected`, `KbusCycle*` | Digital Do->Di round trip |
| `AnalogConnected`, `AnalogStep*` | Analog instant-step round trip |
| `AnalogGradual*` | Analog gradual-ramp round trip |

Each group has `LastMs`/`MinMs`/`MaxMs`/`AvgMs`/`StdMs`/`SampleCount`.
`KbusConnected` / `AnalogConnected` only ever flip `false -> true`, on a real
observed match - never on a timeout - so a missing wire reads as "not
connected," not as a plausible-looking wrong number. The first round trip
after each connection is discarded rather than recorded, since it includes
however long the K-Bus took to resync at startup and would otherwise
permanently distort the running standard deviation.

The analog tolerance (40 counts) and ramp step (100 counts/cycle) are
calibrated against this repo's own hardware's measured noise floor (~20-25
counts) - a real DAC->wire->ADC loopback never reads back bit-exact.
Reusing these constants on different hardware, check your own noise floor
first (log a few AO/AI value pairs at steady state).

## Build

No SDK install on the host - build inside Xentara's build image. Produces
both architectures:

```bash
docker run --rm -v "$PWD":/work -w /work xentara/xentara-build:latest bash -lc '
  cmake -B build-amd64 -G Ninja
  cmake --build build-amd64
  cmake -B build-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/usr/share/xentara/toolchains/arm64-gcc.toolchain.cmake
  cmake --build build-arm64
'
```

Use `build-amd64/libEtherCATKbusRttProbe.so` on an x86 edge computer, the
arm64 one on ARM targets (e.g. a WAGO PFC300).

## Deploy

1. Copy the module into the Xentara instance's control directory, named
   exactly `EtherCATKbusRttProbe.so` (the model's `controlPath` is a bare
   filename that Xentara resolves against `control/` - do **not** add a
   `control/` prefix).
2. Use `model/template-rtt-kbus.json` when you run discovery, then set the
   `Connection.Do` / `Di` / `Ao` / `Ai` data points' `io` fields to your
   actual wired channels (the discovered bus names them after your specific
   hardware, so this repo can't fill them in for you - see the "Wire the
   loopback" step in the top-level `README.md`).

> Xentara enrolls exactly one C++ control per instance. Don't try to load
> this alongside `EtherCATRttProbe` or another `@Skill.CPP.Control` module.
