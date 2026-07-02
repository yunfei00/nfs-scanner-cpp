#!/usr/bin/env python3
"""
SCPI spectrum analyzer simulator over TCP (port 5025).

Device modes (--device):
  zna67   R&S ZNA67  — MMEM CSV + CALC1:DATA? SDATA fallback
  fsw     R&S FSW    — MMEM CSV export path
  n9020a  Keysight N9020A — TRAC:DATA? TRACE1

Options:
  --delay-ms          per-command processing delay
  --random-fail-rate  probability [0..1] of empty/failed responses
  --disconnect-rate   probability [0..1] of forced disconnect after a query
"""

from __future__ import annotations

import argparse
import math
import random
import re
import socket
import socketserver
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

DEVICE_IDN = {
    "zna67": "Rohde & Schwarz,ZNA67,1333.0001k02/102233,4.50.18",
    "fsw": "Rohde & Schwarz,FSW,1312.9000K26/123456,2.30.18",
    "n9020a": "Keysight Technologies,N9020A,MY12345678,A.26.08",
}

TRACE_NAME = "Trc1_S21"


@dataclass
class InstrumentState:
    device: str = "zna67"
    start_hz: float = 1e9
    stop_hz: float = 3e9
    points: int = 201
    rbw_hz: float = 1000.0
    vbw_hz: float = 1000.0
    trace_mode: str = "WRIT"
    continuous: bool = False
    opc_done: bool = True
    error_queue: List[str] = field(default_factory=list)
    stored_files: Dict[str, str] = field(default_factory=dict)
    sweep_count: int = 0
    delay_ms: float = 5.0
    random_fail_rate: float = 0.0
    disconnect_rate: float = 0.0
    lock: threading.Lock = field(default_factory=threading.Lock)


def strip_quotes(text: str) -> str:
    text = text.strip()
    if len(text) >= 2 and text[0] in "\"'" and text[-1] == text[0]:
        return text[1:-1]
    return text


def freq_axis(start: float, stop: float, points: int) -> List[float]:
    if points < 2:
        points = 2
    step = (stop - start) / (points - 1)
    return [start + i * step for i in range(points)]


def synth_amplitude_db(freq_hz: float, sweep_idx: int) -> float:
    # Deterministic pseudo-spectrum for repeatable tests.
    x = freq_hz / 1e9 + sweep_idx * 0.01
    return -40.0 + 10.0 * math.sin(x * 3.1) + 5.0 * math.cos(x * 7.3)


def build_zna_csv(state: InstrumentState) -> str:
    lines = [f"freq;re:{TRACE_NAME};im:{TRACE_NAME}"]
    for i, f in enumerate(freq_axis(state.start_hz, state.stop_hz, state.points)):
        db = synth_amplitude_db(f, state.sweep_count)
        mag = 10 ** (db / 20.0)
        phase = (i / max(state.points, 1)) * math.pi
        re = mag * math.cos(phase)
        im = mag * math.sin(phase)
        lines.append(f"{f:.0f};{re:.6f};{im:.6f}")
    return "\r\n".join(lines) + "\r\n"


def build_fsw_csv(state: InstrumentState) -> str:
    lines = ["freq,value"]
    for f in freq_axis(state.start_hz, state.stop_hz, state.points):
        db = synth_amplitude_db(f, state.sweep_count)
        lines.append(f"{f:.0f},{db:.3f}")
    return "\r\n".join(lines) + "\r\n"


def build_n9020a_trace(state: InstrumentState) -> str:
    values = []
    for f in freq_axis(state.start_hz, state.stop_hz, state.points):
        db = synth_amplitude_db(f, state.sweep_count)
        values.append(f"{db:.3f}")
    return ",".join(values)


def build_sdata_block(state: InstrumentState) -> str:
    nums: List[str] = []
    for i, f in enumerate(freq_axis(state.start_hz, state.stop_hz, state.points)):
        db = synth_amplitude_db(f, state.sweep_count)
        mag = 10 ** (db / 20.0)
        phase = (i / max(state.points, 1)) * math.pi
        nums.append(f"{mag * math.cos(phase):.6f}")
        nums.append(f"{mag * math.sin(phase):.6f}")
    return ",".join(nums)


def scpi_block_payload(text: str) -> bytes:
    data = text.encode("utf-8")
    header = f"#{len(str(len(data)))}{len(data)}".encode("ascii")
    return header + data


class ScpiSession:
    def __init__(self, state: InstrumentState) -> None:
        self.state = state

    def maybe_fail(self) -> bool:
        return random.random() < self.state.random_fail_rate

    def maybe_disconnect(self, conn: socket.socket) -> bool:
        if random.random() < self.state.disconnect_rate:
            try:
                conn.shutdown(socket.SHUT_RDWR)
                conn.close()
            except OSError:
                pass
            return True
        return False

    def delay(self) -> None:
        if self.state.delay_ms > 0:
            time.sleep(self.state.delay_ms / 1000.0)

    def enqueue_error(self, code: int, message: str) -> None:
        self.state.error_queue.append(f"{code},\"{message}\"")

    def pop_error(self) -> str:
        if self.state.error_queue:
            return self.state.error_queue.pop(0)
        return '0,"No error"'

    def apply_set(self, cmd_upper: str, args: str) -> None:
        s = self.state
        num_match = re.search(r"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)", args)
        num = float(num_match.group(1)) if num_match else None

        if cmd_upper in ("*CLS",):
            s.error_queue.clear()
            return

        if "FREQ:STAR" in cmd_upper or "FREQUENCY:START" in cmd_upper or "SENS1:FREQ:STAR" in cmd_upper:
            if num is not None:
                s.start_hz = num
            return
        if "FREQ:STOP" in cmd_upper or "FREQUENCY:STOP" in cmd_upper or "SENS1:FREQ:STOP" in cmd_upper:
            if num is not None:
                s.stop_hz = num
            return
        if "SWE:POIN" in cmd_upper or "SWEEP:POINTS" in cmd_upper or "SENS1:SWE:POIN" in cmd_upper:
            if num is not None:
                s.points = max(2, int(num))
            return
        if "BAND:RES" in cmd_upper or "BANDWIDTH:RESOLUTION" in cmd_upper or "SENS1:BAND" in cmd_upper:
            if num is not None:
                s.rbw_hz = num
            return
        if "BAND:VID" in cmd_upper or "BANDWIDTH:VIDEO" in cmd_upper:
            if num is not None:
                s.vbw_hz = num
            return
        if "INIT:CONT" in cmd_upper or "INIT1:CONT" in cmd_upper:
            s.continuous = "ON" in args.upper()
            return
        if "TRAC" in cmd_upper and "TYPE" in cmd_upper:
            s.trace_mode = args.strip().upper() or "WRIT"
            return
        if "FORM ASC" in cmd_upper or cmd_upper == "FORM ASC":
            return
        if "ABOR" in cmd_upper:
            s.opc_done = True
            return
        if cmd_upper.startswith("MMEM:DEL"):
            path = strip_quotes(args.split(",")[0].strip())
            s.stored_files.pop(path, None)
            s.opc_done = True
            return
        if cmd_upper.startswith("MMEM:STOR"):
            path = strip_quotes(re.split(r",", args, maxsplit=1)[-1].strip())
            if s.device == "zna67":
                s.stored_files[path] = build_zna_csv(s)
            else:
                s.stored_files[path] = build_fsw_csv(s)
            s.sweep_count += 1
            s.opc_done = True
            return
        if cmd_upper.startswith("INIT1:IMM") or cmd_upper.startswith("INIT:IMM"):
            s.sweep_count += 1
            s.opc_done = True
            return
        if "DISP:TRAC1:MODE" in cmd_upper:
            return

    def handle_query(self, cmd: str) -> Optional[bytes]:
        s = self.state
        upper = cmd.upper().strip()
        self.delay()

        if self.maybe_fail():
            return b""

        if upper == "*IDN?":
            return (DEVICE_IDN.get(s.device, DEVICE_IDN["zna67"]) + "\n").encode("utf-8")

        if upper in ("*OPC?", "SYST:ERR?"):
            if upper == "*OPC?":
                return b"1\n"
            return (self.pop_error() + "\n").encode("utf-8")

        if upper.startswith("MMEM:DATA?"):
            path = strip_quotes(cmd.split("?", 1)[-1].strip())
            content = s.stored_files.get(path)
            if content is None:
                self.enqueue_error(-200, "Execution error")
                return b"\n"
            return scpi_block_payload(content) + b"\n"

        if upper.startswith("CALC1:DATA? SDATA") or upper == "CALC1:DATA? SDATA":
            return (build_sdata_block(s) + "\n").encode("utf-8")

        if "TRAC:DATA? TRACE1" in upper or upper.endswith("TRAC:DATA? TRACE1"):
            return (build_n9020a_trace(s) + "\n").encode("utf-8")

        if upper.startswith("FREQ:STAR?"):
            return f"{s.start_hz:.0f}\n".encode("utf-8")
        if upper.startswith("FREQ:STOP?"):
            return f"{s.stop_hz:.0f}\n".encode("utf-8")
        if upper.startswith("SWE:POIN?"):
            return f"{s.points}\n".encode("utf-8")

        return b"\n"

    def handle_command(self, cmd: str) -> Optional[bytes]:
        stripped = cmd.strip()
        if not stripped:
            return None
        if stripped.endswith("?"):
            return self.handle_query(stripped)
        self.apply_set(stripped.upper(), stripped)
        return None


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


class ScpiHandler(socketserver.BaseRequestHandler):
    shared_state: InstrumentState = InstrumentState()

    def handle(self) -> None:
        conn: socket.socket = self.request  # type: ignore[assignment]
        conn.settimeout(1.0)
        session = ScpiSession(self.shared_state)
        buf = ""

        while True:
            try:
                chunk = conn.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break

            buf += chunk.decode("utf-8", errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                line = line.strip("\r")
                if not line.strip():
                    continue
                response = session.handle_command(line)
                if response is not None:
                    try:
                        conn.sendall(response)
                    except OSError:
                        return
                    if session.maybe_disconnect(conn):
                        return


def main() -> None:
    parser = argparse.ArgumentParser(description="SCPI spectrum analyzer TCP simulator")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5025)
    parser.add_argument(
        "--device",
        choices=["zna67", "fsw", "n9020a"],
        default="zna67",
        help="simulated instrument personality",
    )
    parser.add_argument("--delay-ms", type=float, default=5.0, help="per-command delay")
    parser.add_argument(
        "--random-fail-rate",
        type=float,
        default=0.0,
        help="probability of empty query response (0..1)",
    )
    parser.add_argument(
        "--disconnect-rate",
        type=float,
        default=0.0,
        help="probability of forced disconnect after a query (0..1)",
    )
    args = parser.parse_args()

    ScpiHandler.shared_state = InstrumentState(
        device=args.device,
        delay_ms=args.delay_ms,
        random_fail_rate=max(0.0, min(1.0, args.random_fail_rate)),
        disconnect_rate=max(0.0, min(1.0, args.disconnect_rate)),
    )

    with ThreadedTCPServer((args.host, args.port), ScpiHandler) as server:
        print(
            f"SCPI simulator ({args.device}) on {args.host}:{args.port} "
            f"delay={args.delay_ms}ms fail={args.random_fail_rate} disc={args.disconnect_rate}"
        )
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
