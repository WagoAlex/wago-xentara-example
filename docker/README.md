# `docker/` - run any app straight from this clone

[`docker-compose.yml`](docker-compose.yml) runs the official
`wagoalex/xentara-base:2026-11-13` image and bind-mounts everything an app needs
from this repo: the model, its control `.so`, the licence directory and the log
directory. Nothing is copied into the container and no image is built, so
editing `model/*.json` here and restarting is the whole edit loop.

One profile per app, one at a time:

```bash
docker compose -f docker/docker-compose.yml --profile app1 up -d   # Xentara demo
docker compose -f docker/docker-compose.yml --profile app2 up -d   # WAGO RTT
docker compose -f docker/docker-compose.yml --profile app3 up -d   # WAGO RTT + K-Bus
docker compose -f docker/docker-compose.yml --profile app4 up -d   # MQTT payload control
```

All four use the same `container_name`, so Docker refuses to start a second
one. That is intentional - the EtherCAT master needs exclusive use of the NIC,
and so does a native `xentara@edge.service` if the device has one
(`systemctl stop xentara@edge.service` first).

Defaults follow Xentara's own conventions plus a clone in `Downloads`:

| Variable | Default | Meaning |
|---|---|---|
| `REPO` | `${HOME}/Downloads/wago-xentara-example` | this clone |
| `LICENCE` | `${HOME}/.local/share/xentara/licence` | directory holding `licences.json` |

Set them explicitly in Portainer (Stacks -> Add stack -> Web editor -> environment
variables), together with `COMPOSE_PROFILES` - `HOME` is not defined there, so
the defaults do not resolve.

## Before the first run

- **Build the control libraries.** They are build output, `.gitignore`d, and
  therefore missing from a fresh clone. Apps 2, 3 and 4 will not start without
  them - see each `control/*/README.md`. Expected paths:
  `control/ethercat-rtt-probe/build-amd64/libEtherCATRttProbe.so`,
  `control/ethercat-kbus-rtt-probe/build-amd64/libEtherCATKbusRttProbe.so`,
  `control/mqtt-payload-control/build-amd64/libMQTTPayloadControl.so`.
  On an ARM device (PFC300) point the mounts at `build-arm64/` instead.
- **Have a licence.** It is tied to the machine's node ID and is not in this
  repo. Without one Xentara exits with `FATAL ERROR: no Xentara licence found`
  and prints a `try.xentara.io` activation link.
- `mkdir -p /var/log/xentara` on the host.

## Notes

- `command: ["xentara"]` overrides the image's own `CMD`, because the
  2026-07-24 image ships `/usr/bin/run-xentara.sh` with CRLF line endings -
  `exec` fails with "no such file or directory" and `bash` cannot parse it
  either. `restart: unless-stopped` covers what that script's retry loop did.
  Revert to the script once upstream fixes the line endings.
- The image's stock `config.json` applies, so the Web Service listens on
  **8080**. Mount your own `config.json` over
  `/home/xentara/.config/xentara/config.json` to change that.
- The root [`../docker-compose.yml`](../docker-compose.yml) is the plain
  bring-up stack with no mounts - use it to get a runtime licensed before you
  care about a specific app. This folder is for running the apps themselves.

## `portainer-app*.yml` - baked images, TUI on the console

`portainer-app1.yml` … `portainer-app4.yml` are standalone, copy-pastable
stacks for Portainer (Stacks -> Add stack -> Web editor -> Deploy). They use the
prebuilt images, so the model and its control library are already inside - only
the licence is mounted. Nothing needs to be cloned or built on the device.

| File | Image |
|---|---|
| `portainer-app1.yml` | `wagoalex/xentara-example-app1-demo:latest` |
| `portainer-app2.yml` | `wagoalex/xentara-example-app2-rtt:latest` |
| `portainer-app3.yml` | `wagoalex/xentara-example-app3-rtt-kbus:latest` |
| `portainer-app4.yml` | `wagoalex/xentara-example-app4-mqtt-payload-control:latest` |

### The TUI opens by itself

The container's `.bashrc` execs `xentara-tui`, so Portainer's **Console ->
Connect** (command `/bin/bash`) lands in the live inspector instead of a shell
prompt. Quitting the TUI with `q` ends the console session. Non-interactive
`docker exec … bash -c "…"` is unaffected.

Two console quirks are fixed in the image rather than worked around, because
neither is the TUI's fault alone:

- **Portainer writes its shell init to stdin after bash starts** (`export
  LANG=C.UTF-8`, `alias ls=…`, a clear-screen `echo`). Since `.bashrc` `exec`s
  the TUI immediately, that text was echoed onto the screen - and stayed there,
  because the TUI's full redraw uses cursor-home, not clear-screen - while the
  same bytes were also delivered as keystrokes, each triggering a redraw and
  sometimes navigating on its own. The hook now sets `stty -echo`, drains stdin
  with `read -t 0.6 -N 100000` (`-N`, not `-n`: `-n` stops at the first newline
  and leaves the remaining lines to arrive as keypresses), clears the screen,
  then hands over.
- **The selected row overwrote the box border.** `drawLine()` pads a selected
  row to `$w-2` visible characters starting at `$col+1`, so it lands on
  `$col+$w-1` where `drawBox()` drew the right border; unselected rows pad to
  `$w-3`. The edge appeared to jump as the cursor moved. A one-character `sed`
  in the Dockerfile pads the selected row like the rest.

The TUI needs a password. Set it once - it persists in the named volume:

```bash
docker exec -it -u xentara -e HOME=/home/xentara xentara-app1 xentara-password xentara
```

Then either type it at the TUI prompt each time, or set
`XENTARA_TUI_PASSWORD` in the stack's environment to skip the prompt.

### Ports, and running more than one app at once

Each stack uses host networking, so every app needs its own port. They are
preassigned, one per app, and `XENTARA_TUI_PORT` is the only place the number
appears:

| Stack | Port |
|---|---|
| `portainer-app1.yml` | 8081 |
| `portainer-app2.yml` | 8082 |
| `portainer-app3.yml` | 8083 |
| `portainer-app4.yml` | 8084 |

The `command:` in each file stamps that value into `httpServer.port` in the
volume's `config.json` before starting Xentara, so the server port and the TUI
port can never drift apart. Nothing to edit by hand, and existing volumes get
corrected on the next restart. Stock 8080 is deliberately left free - it is
often already taken on these devices, which used to produce `FATAL ERROR: HTTP
server: could not listen on port 8080: Address already in use`.

To move an app, change its `XENTARA_TUI_PORT` and redeploy. The
volume name is pinned with `name:` in each file, so it stays
`xentara-appN-config` whether you deploy from the CLI or from Portainer (which
would otherwise prefix it with the stack name).

> [!WARNING]
> A second instance on the same EtherCAT NIC **does not fail loudly**. It
> starts, loads its model, logs nothing unusual, and binds its own port - but
> every EtherCAT entry reads quality `Bad` with a frozen value. Different ports
> are not enough; the NIC is the exclusive resource. Check with
> `docker ps | grep xentara` plus `systemctl is-active 'xentara@*'` before
> blaming wiring, and verify quality per data point with:
>
> ```bash
> docker exec -i xentara-app2 xentara-debugger <<'EOF'
> read "EtherCAT Terminal.WAGO Series 750 Bus Coupler.Channel 1, Data 1"
> exit
> EOF
> ```
>
> `xentara-debugger` prints a `TIME STAMP / QUALITY / VALUE` table, which is the
> quickest way to tell "bus not working" from "value happens to be false".

Two instances can run side by side only if they use different ports **and** do
not both need the EtherCAT NIC. Apps 2, 3 and 4 each drive the same EtherCAT
master on the same interface, so exactly one of them can run at a time - and a
native `xentara@edge.service` counts as one too. App 1 touches no hardware, so
it can run alongside any one of the others. The licence itself is not the limit:
its data-point budget is counted per instance, not summed across containers.

## `Dockerfile.xentara-example` - how the images are built

One Dockerfile builds all four images. Nothing app-specific is written in it:
the app *is* the build context.

```dockerfile
FROM wagoalex/xentara-base:2026-11-13

USER root
RUN sed -i 's/\r$//' /usr/bin/run-xentara.sh /usr/local/bin/xentara-tui
RUN printf '%s\n' \
      'case $- in' \
      '  *i*) exec xentara-tui --host "${XENTARA_TUI_HOST:-localhost}" … ;;' \
      'esac' \
    >> /home/xentara/.bashrc
COPY --chown=xentara:xentara . /home/xentara/.config/xentara/
USER xentara
```

Four decisions worth knowing:

**The base image is upstream's trial image, unmodified except for two bugs.**
Its 2026-07-24 build ships `/usr/bin/run-xentara.sh` **and**
`/usr/local/bin/xentara-tui` with CRLF line endings. The first makes the
container crash-loop with `exec /usr/bin/run-xentara.sh: no such file or
directory`; the second makes the TUI's shebang `#!/usr/bin/env php\r`. The one
`sed` fixes both. Drop it once upstream does.

**The app is the build context, not a build arg.** `COPY . →
/home/xentara/.config/xentara/` copies a staging directory holding exactly what
that app needs, so the same Dockerfile produces all four images and there is no
per-app `if` anywhere. See the staging commands below.

**Interactive shells become the TUI.** The appended `.bashrc` case matches only
when `$-` contains `i`, so Portainer's **Console → Connect** (`bash`) opens the
live inspector while `docker exec … bash -c "…"` stays a plain shell. It reads
`XENTARA_TUI_HOST/PORT/USER/PASSWORD` from the environment, which is why the
stacks set them.

**No entrypoint at all.** These images run on `CMD ["/usr/bin/run-xentara.sh"]`
inherited from the base - upstream's `while true; do xentara; sleep 5; done`
retry loop. That loop is why a fatal config error shows up as an endlessly
repeating `FATAL ERROR` with the container still reported "Up": the script is
restarting Xentara, not Docker. The stacks override `CMD` to stamp the port
first and then `exec` that same script.

Two things are deliberately **not** baked into the images:

| | Why | Where it comes from instead |
|---|---|---|
| the licence | tied to the node ID, so an image with one in it is useless anywhere else | bind-mounted read-only |
| the port | one image, several deployments | `XENTARA_TUI_PORT`, stamped into `config.json` at start |

### Building the images

Each build needs a staging dir. From a clone with the control libraries already
built (see each `control/*/README.md`):

```bash
cd /path/to/wago-xentara-example
DATE=$(date +%F)

# App 1 - model only, no control library.
rm -rf /tmp/stage-app1 && mkdir -p /tmp/stage-app1
cp schemas/sample-model.json /tmp/stage-app1/model.json
docker build -f docker/Dockerfile.xentara-example \
  -t wagoalex/xentara-example-app1-demo:$DATE \
  -t wagoalex/xentara-example-app1-demo:latest /tmp/stage-app1

# App 2 - RTT over EtherCAT.
rm -rf /tmp/stage-app2 && mkdir -p /tmp/stage-app2/control
cp model/example-rtt.json /tmp/stage-app2/model.json
cp control/ethercat-rtt-probe/build-amd64/libEtherCATRttProbe.so \
   /tmp/stage-app2/control/EtherCATRttProbe.so
docker build -f docker/Dockerfile.xentara-example \
  -t wagoalex/xentara-example-app2-rtt:$DATE \
  -t wagoalex/xentara-example-app2-rtt:latest /tmp/stage-app2

# App 3 - RTT + K-Bus.
rm -rf /tmp/stage-app3 && mkdir -p /tmp/stage-app3/control
cp model/example-rtt-kbus.json /tmp/stage-app3/model.json
cp control/ethercat-kbus-rtt-probe/build-amd64/libEtherCATKbusRttProbe.so \
   /tmp/stage-app3/control/EtherCATKbusRttProbe.so
docker build -f docker/Dockerfile.xentara-example \
  -t wagoalex/xentara-example-app3-rtt-kbus:$DATE \
  -t wagoalex/xentara-example-app3-rtt-kbus:latest /tmp/stage-app3

# App 4 - MQTT payload control.
rm -rf /tmp/stage-app4 && mkdir -p /tmp/stage-app4/control
cp model/example-mqtt-payload-control.json /tmp/stage-app4/model.json
cp control/mqtt-payload-control/build-amd64/libMQTTPayloadControl.so \
   /tmp/stage-app4/control/MQTTPayloadControl.so
docker build -f docker/Dockerfile.xentara-example \
  -t wagoalex/xentara-example-app4-mqtt-payload-control:$DATE \
  -t wagoalex/xentara-example-app4-mqtt-payload-control:latest /tmp/stage-app4
```

Then publish, and deploy on the device by pulling - not by
`docker save | ssh | docker load`, which has to be repeated per device:

```bash
for i in app1-demo app2-rtt app3-rtt-kbus app4-mqtt-payload-control; do
  docker push wagoalex/xentara-example-$i:$DATE
  docker push wagoalex/xentara-example-$i:latest
done
```

```bash
# on the device
docker compose -p xentara-app3 -f portainer-app3.yml pull
docker compose -p xentara-app3 -f portainer-app3.yml up -d
```

In Portainer that is **Stacks -> the stack -> Update the stack** with
*Re-pull image* ticked. Note the volume keeps its old `model.json` either way -
see [Updating](#updating).

The `control/<Name>.so` filename must match the `@Skill.CPP.Control` path in
that app's `model.json` - it is not the same as the `lib*.so` name the build
produces. On an ARM device (PFC300) stage from `build-arm64/` instead; the
images published on Docker Hub are x86_64.

Model files under `model/` are this repo's real verified generator output for
the reference device, not templates - see `model/README.md`. App 4's model has
the broker address `192.168.2.124` baked in; edit it before staging if yours
differs.

### Updating

The named volume is seeded from the image on **first** start only, so pulling a
newer image does not change an existing volume's `model.json`. To adopt a new
image's model, `docker compose -f portainer-appN.yml down -v` first - that also
discards the password you set.
