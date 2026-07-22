#!/usr/bin/env python3
"""Read and subscribe to the RTT.* registers over Xentara's WebSocket API.

Talks the same CBOR/RPC protocol as `xentara-tui` (see the "TUI isn't magic"
section of the README): sends the required Client Hello handshake, reads
every register once, subscribes to live updates, then prints each update as
it arrives.

Usage:
    pip install websockets cbor2
    python3 rtt_websocket_test.py --host <device-ip> --port 8080 \
        --user xentara --password <pw> --model /path/to/model.json

Port defaults to 8080 (the general default); pass --port 8006 if your
deployment's config.json overrides it, as this repo's own test device does.

Wire format notes (learned the hard way against a real device - the official
docs describe each packet type but not the framing around them):
  - Every WebSocket message is a *batch*: a CBOR array of one or more packets,
    even for a single request. Sending a bare packet (not wrapped in an outer
    array) gets parsed as a batch of that packet's own fields, one spurious
    sub-packet per field, each failing with -32600/-32700.
  - The Client Hello command (opcode 14) must be the very first packet sent
    on a new connection, before anything else, or every other command fails
    with "missing client hello request" (-32000).
  - cbor2 encodes/decodes Python uuid.UUID objects as CBOR tag 37 natively -
    no manual tagging needed for element UUIDs.
  - Read/write results are wrapped in CBOR tag 121 (success) or 122 (error);
    unwrap with the `untag()` helper below.
"""

import argparse
import asyncio
import json
import ssl
import sys
import uuid
from base64 import b64encode

import cbor2
import websockets

ATTR_VALUE = 11

OPCODE_READ = 4
OPCODE_WRITE = 5
OPCODE_SUBSCRIBE = 6
OPCODE_CLIENT_HELLO = 14

PACKET_REQUEST = 0
PACKET_SUCCESS = 1
PACKET_ERROR = 2
PACKET_EVENT = 8


def find_registers(model_path: str, group_name: str) -> dict[str, uuid.UUID]:
    """Map register name -> UUID for every @Skill.SignalFlow.Register under
    the first @Group named group_name (recursing into nested groups, since
    e.g. the K-Bus RTT probe's registers live under sub-groups of "RTT")."""
    with open(model_path) as f:
        model = json.load(f)

    registers: dict[str, uuid.UUID] = {}

    def walk(node, in_target_group):
        if isinstance(node, dict):
            if "@Group" in node:
                grp = node["@Group"]
                walk(grp.get("children", []), in_target_group or grp.get("name") == group_name)
                return
            if "@Skill.SignalFlow.Register" in node and in_target_group:
                reg = node["@Skill.SignalFlow.Register"]
                registers[reg["name"]] = uuid.UUID(reg["UUID"])
                return
            for v in node.values():
                walk(v, in_target_group)
        elif isinstance(node, list):
            for item in node:
                walk(item, in_target_group)

    walk(model.get("children", []), False)
    return registers


def untag(value):
    """Unwrap CBOR tag 121 (success)/122 (error) result wrappers."""
    if isinstance(value, cbor2.CBORTag):
        if value.tag == 121:
            return untag(value.value)
        if value.tag == 122:
            return RuntimeError(f"server error: {value.value}")
    return value


async def send_packet(ws, packet: list) -> list:
    """Send one packet as a single-element batch and return its reply
    (also a single-element batch, per the server's own framing)."""
    await ws.send(cbor2.dumps([packet]))
    replies = cbor2.loads(await ws.recv())
    return replies[0]


async def run(args: argparse.Namespace) -> None:
    registers = find_registers(args.model, args.group)
    if not registers:
        sys.exit(f"No registers found under @Group '{args.group}' in {args.model}")

    by_uuid = {v: k for k, v in registers.items()}
    print(f"Found {len(registers)} registers under '{args.group}': {', '.join(registers)}\n")

    ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ssl_ctx.check_hostname = False
    ssl_ctx.verify_mode = ssl.CERT_NONE  # self-signed cert on the test device

    auth = b64encode(f"{args.user}:{args.password}".encode()).decode()
    headers = {"Authorization": f"Basic {auth}"}

    url = f"wss://{args.host}:{args.port}/api/ws"
    print(f"Connecting to {url} ...")

    async with websockets.connect(url, additional_headers=headers, ssl=ssl_ctx) as ws:
        hello = await send_packet(ws, [PACKET_REQUEST, 0, OPCODE_CLIENT_HELLO, {0: 1, 1: 1}])
        if hello[0] != PACKET_SUCCESS:
            sys.exit(f"Client Hello failed: {hello}")
        print(f"Connected, protocol version {hello[2][0]}\n")

        # One-shot read of every register's current value.
        msg_id = 1
        for name, element_uuid in registers.items():
            reply = await send_packet(ws, [PACKET_REQUEST, msg_id, OPCODE_READ,
                                            {0: element_uuid, 1: [ATTR_VALUE]}])
            value = untag(reply[2][ATTR_VALUE]) if reply[0] == PACKET_SUCCESS else reply
            print(f"  {name:28s} = {value}")
            msg_id += 1

        # Subscribe to all of them at once and print live updates.
        print(f"\nSubscribing, watching for {args.duration}s of live updates "
              "(Ctrl+C to stop early) ...\n")
        subscription = [{0: u, 1: [ATTR_VALUE]} for u in registers.values()]
        ack = await send_packet(ws, [PACKET_REQUEST, msg_id, OPCODE_SUBSCRIBE,
                                      {0: subscription, 1: args.min_interval_ms}])
        if ack[0] != PACKET_SUCCESS:
            sys.exit(f"Subscribe failed: {ack}")

        try:
            async with asyncio.timeout(args.duration):
                async for raw in ws:
                    for packet in cbor2.loads(raw):
                        if packet[0] != PACKET_EVENT:
                            continue
                        event = packet[2]
                        name = by_uuid.get(event[1], str(event[1]))
                        for attr_id, value in event[2].items():
                            if attr_id == ATTR_VALUE:
                                print(f"  {name:28s} = {untag(value)}")
        except TimeoutError:
            pass


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP or hostname")
    p.add_argument("--port", type=int, default=8080, help="WebSocket API port (default: 8080)")
    p.add_argument("--user", default="xentara", help="Username (default: xentara)")
    p.add_argument("--password", required=True, help="Password for --user")
    p.add_argument("--model", required=True, help="Path to the deployed model.json")
    p.add_argument("--group", default="RTT", help="Top-level @Group to read registers from (default: RTT)")
    p.add_argument("--duration", type=float, default=10.0, help="Seconds to watch live updates (default: 10)")
    p.add_argument("--min-interval-ms", type=int, default=100, help="Max update rate to request, ms (default: 100)")
    return p.parse_args()


if __name__ == "__main__":
    asyncio.run(run(parse_args()))
