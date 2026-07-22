# Models

Six model files: three generator inputs, two real generated examples, and
one hand-written reference - all running the bus by pairing a 1 ms `@Timer`
with a `@Pipeline` that calls `EtherCAT Terminal.loop` every cycle, plus
Xentara's own hardware-free demo.

> [!WARNING]
> **`template-minimal.json`, `template-rtt.json`, and `template-rtt-kbus.json`
> are not Xentara models** - don't open them in the Workbench or copy them
> to the device as `model.json`. Each one is *input* to
> `xentara-ethercat-model-file-generator`: it contains the literal text
> `#CoE.Bus:EtherCAT Terminal`, which is required generator syntax (see
> [the generator's identifier syntax docs](https://docs.xentara.io/xentara-ethercat-driver/ethercat_driver_model_file_generator.html#ethercat_driver_model_file_generator_identifier))
> but isn't valid JSON per the Xentara model schema - a string where an
> object is expected. Opening one directly always fails with "expected a
> JSON object" at that line; that's not a bug, it's what an unprocessed
> template looks like. Run the generator first (`../README.md`, Step C) and
> use its output file, which is the real, importable model.

| File | Use it when | Custom C++ needed? | Importable as-is? |
|---|---|---|---|
| `template-minimal.json` | **Start here.** You want to discover your real bus and edit I/O in the TUI, nothing more. | No | No - generator input only |
| `template-rtt.json` | You also want live cycle-time (RTT) numbers. | Yes - `control/ethercat-rtt-probe` | No - generator input only |
| `template-rtt-kbus.json` | You also want a *verified hardware* round trip (real DO->DI and AO->AI, not just cycle time), and have two loopback wires spare. | Yes - `control/ethercat-kbus-rtt-probe` | No - generator input only |
| `example-rtt.json` | Reference: `template-rtt.json`'s actual generator output from a real WAGO 750-354 coupler (App 2). Shows what a *finished* file looks like, and imports straight into the Workbench without running anything. | Yes | Yes |
| `example-rtt-kbus.json` | Reference: `template-rtt-kbus.json`'s actual generator output from the same coupler (App 3), with the `Connection.Do`/`Di`/`Ao`/`Ai` channels already bound to this repo's tested loopback wiring. | Yes | Yes |
| `example-8di8do.json` | Reference: a complete, hand-written model for one coupler + one 8DI/8DO module, to read and learn from. | No | Yes |

No EtherCAT hardware yet? `../schemas/sample-model.json` is Xentara's own
demo model - a signal generator feeding synthetic waveforms into a debug
inspector, no bus or wiring required, and it's directly loadable as-is. See
the top-level `README.md` for how to run it.

`example-rtt.json` and `example-rtt-kbus.json` are frozen snapshots from one
real coupler, not templates - their `bufferAddress` values are correct only
for that exact module layout (see "Two things you always set for your
hardware", below). Use them to see what a real deployed file looks like or
to confirm the Workbench opens a generator-output file correctly; run your
own discovery (Step C) for your own hardware.

## The templates are for the discovery tool

The marker string every template contains:

```json
"#CoE.Bus:EtherCAT Terminal"
```

The `xentara-ethercat-model-file-generator` **replaces that marker** with the
`@Skill.CoE.Bus` it discovers from your live bus, and keeps the rest of the
template (the track, and for the RTT ones the probe control) intact - so the
output is a complete, runnable `model.json`. See `../README.md`, Step C.

## Two things you always set for your hardware

1. **`interface`** - the NIC wired to the coupler (e.g. `eth0`, `enp3s0`; on
   WAGO edge devices it's often `X11`). The generator sets this from its `-b`
   flag; `example-8di8do.json` has `REPLACE_WITH_YOUR_NIC` for you to fill in.
   `example-rtt.json` and `example-rtt-kbus.json` already have a real
   interface name baked in from the device they were generated on - change
   it to yours before deploying.
2. **`bufferAddress`** (byte.bit offsets) - these depend on the coupler's own
   control/diagnostic words **and every terminal ahead of a module on the
   row**. Never hand-guess them; the generator reads the real values off the
   bus. `example-8di8do.json`'s addresses assume a single 8DI/8DO module
   first on the bus, and `example-rtt.json`/`example-rtt-kbus.json`'s match
   a specific WAGO 750-354 coupler's exact module row - all three are
   correct only for the layout they were made from.

## Free-run synchronization

The generator writes the bus **without** a synchronization mode, but the bus
negotiates one at power-up. Every known-good WAGO model uses free run, so make
sure your bus has:

```json
"@Skill.CoE.Bus": {
  "synchronization": { "mode": "free run" },
  ...
}
```

`example-8di8do.json` already has it. The templates carry no bus of their own
(the generator supplies it), so after discovery set free run on the generated
bus - a dropdown in the Xentara Workbench, or add the two lines above.
