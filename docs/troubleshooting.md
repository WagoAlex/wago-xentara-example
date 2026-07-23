# Troubleshooting

← [Back to README](../README.md)

## Deployment and licensing (all apps)

| Symptom | Fix |
|---|---|
| Licence error in logs | Activate the node ID at customerportal.xentara.io, then restart. |
| TUI 401 after setting the password | Restart the container - the password is read only at startup. |
| TUI SSL error on 8080 | Try port **8006** instead. |
| Config/model/licence gone after redeploy | No persistent volume - back up before recreating, or add bind mounts. |
| Jittery cycle time | Real-time thread preempted - fix `XENTARA_AFFINITY`, isolate the core, use an RT kernel. |

## EtherCAT discovery and I/O (App 2 / App 3)

| Symptom | Fix |
|---|---|
| Discovery: "can't open interface" | Another Xentara instance owns the NIC - stop it first. |
| Discovery: "connected devices less than configured" | Coupler state machine out of sync; run `xentara-ethercat-device-info --interface <your-nic>` once, then retry. |

## Building your own control module (App 2 / App 3 / App 4)

| Symptom | Fix |
|---|---|
| A control's `step()` never runs, no error | `controlPath` had a `control/` prefix - use the bare filename. |
| `multiple controls are enrolled` | One C++ control per instance; remove the extra `@Skill.CPP.Control`. |

## MQTT / AI-driven outputs (App 4)

| Symptom | Fix |
|---|---|
| `FATAL ERROR: ... unknown member "asJSONText" for data point element` | You're loading this model into `xentara-tryout` (Docker). It needs `xentara-mqtt-client >= 2.0+2.2`; the Docker image ships `2.0+2.1`. Run it on the native install instead - see the [App 4 warning](app-mqtt-payload-control.md#app-4---mqtt-payload-control-ai-driven-outputs). |
| `Registers.MQTT-Payload` stays `""` | Broker unreachable, or wago-hailo-example isn't publishing yet - confirm with `mosquitto_sub -h <broker> -t inference/yolov5m-results -v` first, outside Xentara entirely. |
| `Connection.DO*` reads `true`/`false` but nothing physically switches | Stale EtherCAT addresses - the model wasn't (re)discovered against the *current* physical row. Regenerate it: [Step K](app-mqtt-payload-control.md#step-k---discover-your-io-modules). `xentara-ethercat-device-info` reporting `Invalid sync manager configuration` is a symptom of the same thing. |
| Payload updates but no `Connection.DO*` ever flips | The `detections` text needs the literal substrings `"head"`, `"white helmet"`, or `"blue helmet"` - a generic `"helmet"` class won't match anything; see [Step I](app-mqtt-payload-control.md#step-i---run-the-inference-source-external). |
| MQTT connects, then drops repeatedly | Check `Track MQTT Reconnect`'s 5s retry isn't fighting a broker that's also restarting; confirm `brokerAddress` and port in the model match your actual broker, not this repo's own test device. |
| Native install and `xentara-tryout` both show bus errors / bad quality | Only one can own the EtherCAT NIC at a time - stop the other one first (`systemctl stop xentara@<user>.service` or `docker stop xentara-tryout`). |
