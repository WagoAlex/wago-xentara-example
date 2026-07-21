# Models

Three model files, for three situations. All of them run the EtherCAT bus by
pairing a 1 ms `@Timer` with a `@Pipeline` that calls `EtherCAT Terminal.loop`
every cycle - that loop is what actually exchanges I/O with the coupler.

| File | Use it when | Custom C++ needed? |
|---|---|---|
| `template-minimal.json` | **Start here.** You want to discover your real bus and edit I/O in the TUI, nothing more. | No |
| `template-rtt.json` | You also want live cycle-time (RTT) numbers. | Yes - `control/ethercat-rtt-probe` |
| `example-8di8do.json` | Reference: a complete, hand-written model for one coupler + one 8DI/8DO module, to read and learn from. | No |

## The templates are for the discovery tool

`template-minimal.json` and `template-rtt.json` both contain the marker string:

```json
"#CoE.Bus:EtherCAT Terminal"
```

The `xentara-ethercat-model-file-generator` **replaces that marker** with the
`@Skill.CoE.Bus` it discovers from your live bus, and keeps the rest of the
template (the track, and for the RTT one the probe control) intact - so the
output is a complete, runnable `model.json`. See `../README.md`, step 4.

## Two things you always set for your hardware

1. **`interface`** - the NIC wired to the coupler (e.g. `eth0`, `enp3s0`; on
   WAGO edge devices it's often `X11`). The generator sets this from its `-b`
   flag; `example-8di8do.json` has `REPLACE_WITH_YOUR_NIC` for you to fill in.
2. **`bufferAddress`** (byte.bit offsets) - these depend on the coupler's own
   control/diagnostic words **and every terminal ahead of a module on the
   row**. Never hand-guess them; the generator reads the real values off the
   bus. `example-8di8do.json`'s addresses assume a single 8DI/8DO module first
   on the bus - correct only for that exact layout.

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
