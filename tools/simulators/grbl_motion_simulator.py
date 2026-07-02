#!/usr/bin/env python3
"""
GRBL-like motion controller simulator over TCP.

NFSScanner connects via serial (QSerialPort). On Windows, bridge this TCP port
to a virtual COM pair (see tools/simulators/README.md).

Supported commands (matching SerialMotionController / GrblCommandBuilder):
  $I   version query
  $H   home
  $X   unlock (clear alarm)
  ?    status query (MPos)
  G1   absolute move (G1X..Y..Z..F..)
  G91G1 jog delta
  !    feed hold
  ~    cycle start / resume
  0x18 soft reset

Coordinate limits (same as SerialMotionController::validatePosition):
  X: 0 ~ 200 mm
  Y: -300 ~ 0 mm
  Z: 0 ~ 10 mm
"""

from __future__ import annotations

import argparse
import math
import re
import socket
import socketserver
import threading
import time
from dataclasses import dataclass, field
from typing import Optional, Tuple

SOFT_RESET = b"\x18"

LIMITS = {
    "x": (0.0, 200.0),
    "y": (-300.0, 0.0),
    "z": (0.0, 10.0),
}


@dataclass
class GrblState:
    mode: str = "Idle"  # Idle | Run | Hold | Alarm | Home
    x: float = 100.0
    y: float = -150.0
    z: float = 5.0
    feed: float = 0.0
    alarm: bool = False
    line_buffer: bytearray = field(default_factory=bytearray)
    move_lock: threading.Lock = field(default_factory=threading.Lock)

    def status_line(self) -> str:
        return (
            f"<{self.mode}|MPos:{self.x:.3f},{self.y:.3f},{self.z:.3f}|"
            f"FS:{self.feed:.0f},0>"
        )

    def in_bounds(self, x: float, y: float, z: float) -> bool:
        return (
            LIMITS["x"][0] <= x <= LIMITS["x"][1]
            and LIMITS["y"][0] <= y <= LIMITS["y"][1]
            and LIMITS["z"][0] <= z <= LIMITS["z"][1]
        )


def parse_g1(line: str, state: GrblState) -> Tuple[Optional[Tuple[float, float, float, float]], Optional[str]]:
    """Parse G1 move. Returns ((x,y,z,feed), error) or (None, error)."""
    upper = line.upper().replace(" ", "")
    relative = "G91" in upper
    if "G1" not in upper:
        return None, "unsupported command"

    def extract(axis: str) -> Optional[float]:
        m = re.search(rf"{axis}([-+]?\d*\.?\d+)", upper)
        return float(m.group(1)) if m else None

    fx = extract("X")
    fy = extract("Y")
    fz = extract("Z")
    ff = extract("F")

    target_x = state.x + fx if relative and fx is not None else (fx if fx is not None else state.x)
    target_y = state.y + fy if relative and fy is not None else (fy if fy is not None else state.y)
    target_z = state.z + fz if relative and fz is not None else (fz if fz is not None else state.z)

    feed = ff if ff is not None and ff > 0 else 1000.0
    if not state.in_bounds(target_x, target_y, target_z):
        return None, "error:15"
    return (target_x, target_y, target_z, feed), None


class GrblSession:
    def __init__(self, state: GrblState, move_delay_ms: float) -> None:
        self.state = state
        self.move_delay_ms = move_delay_ms

    def send_line(self, conn: socket.socket, text: str) -> None:
        conn.sendall((text + "\n").encode("utf-8"))

    def simulate_move(self, conn: socket.socket, target: Tuple[float, float, float, float]) -> None:
        x, y, z, feed = target
        with self.state.move_lock:
            if self.state.alarm:
                self.send_line(conn, "error:8")
                return
            self.state.mode = "Run"
            self.state.feed = feed
            self.send_line(conn, "ok")
            # Simulate motion time proportional to distance.
            dist = math.sqrt(
                (x - self.state.x) ** 2
                + (y - self.state.y) ** 2
                + (z - self.state.z) ** 2
            )
            delay = max(self.move_delay_ms / 1000.0, dist / max(feed, 1.0) * 60.0)
            time.sleep(min(delay, 3.0))
            self.state.x, self.state.y, self.state.z = x, y, z
            self.state.mode = "Idle"
            self.state.feed = 0.0

    def handle_line(self, conn: socket.socket, line: str) -> None:
        cmd = line.strip()
        if not cmd:
            return

        if cmd == "?":
            self.send_line(conn, self.state.status_line())
            return

        if cmd == "$I":
            self.send_line(
                conn,
                "[VER:1.1f.20220608:NFSScanner-GRBL-Sim]\r\n"
                "[OPT:PH,SD]\r\n"
                "[GRBL:NFSScanner Motion Simulator]\r\n"
                "ok",
            )
            return

        if cmd == "$":
            self.send_line(conn, "$0=10")
            self.send_line(conn, "ok")
            return

        if cmd == "$X":
            self.state.alarm = False
            self.state.mode = "Idle"
            self.send_line(conn, "ok")
            return

        if cmd == "$H":
            if self.state.alarm:
                self.send_line(conn, "error:8")
                return
            with self.state.move_lock:
                self.state.mode = "Home"
                self.send_line(conn, "ok")
                time.sleep(max(self.move_delay_ms / 1000.0, 0.5))
                self.state.x, self.state.y, self.state.z = 0.0, 0.0, 0.0
                self.state.mode = "Idle"
            return

        if cmd == "!":
            if self.state.mode == "Run":
                self.state.mode = "Hold"
            self.send_line(conn, "ok")
            return

        if cmd == "~":
            if self.state.mode == "Hold":
                self.state.mode = "Idle"
            self.send_line(conn, "ok")
            return

        if cmd.upper().startswith("G") and "G1" in cmd.upper():
            parsed, err = parse_g1(cmd, self.state)
            if err:
                self.send_line(conn, err)
                return
            assert parsed is not None
            threading.Thread(
                target=self.simulate_move, args=(conn, parsed), daemon=True
            ).start()
            return

        self.send_line(conn, "error:20")


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


class GrblHandler(socketserver.BaseRequestHandler):
    shared_state: GrblState = GrblState()
    move_delay_ms: float = 50.0

    def handle(self) -> None:
        session = GrblSession(self.shared_state, self.move_delay_ms)
        conn: socket.socket = self.request  # type: ignore[assignment]
        conn.settimeout(0.5)
        buf = bytearray()
        session.send_line(conn, "Grbl 1.1f ['$' for help]")

        while True:
            try:
                chunk = conn.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break

            i = 0
            while i < len(chunk):
                b = chunk[i]
                if b == SOFT_RESET[0]:
                    with session.state.move_lock:
                        session.state.mode = "Idle"
                        session.state.feed = 0.0
                        session.state.line_buffer.clear()
                    i += 1
                    continue
                buf.append(b)
                i += 1

            while True:
                nl = buf.find(b"\n")
                if nl < 0:
                    break
                line = buf[:nl].decode("utf-8", errors="replace").strip("\r")
                del buf[: nl + 1]
                session.handle_line(conn, line)


def main() -> None:
    parser = argparse.ArgumentParser(description="GRBL-like TCP motion simulator")
    parser.add_argument("--host", default="0.0.0.0", help="bind address")
    parser.add_argument("--port", type=int, default=7001, help="TCP port (default 7001)")
    parser.add_argument(
        "--move-delay-ms",
        type=float,
        default=50.0,
        help="minimum per-move delay in milliseconds",
    )
    args = parser.parse_args()

    GrblHandler.move_delay_ms = args.move_delay_ms
    with ThreadedTCPServer((args.host, args.port), GrblHandler) as server:
        print(f"GRBL simulator listening on {args.host}:{args.port}")
        print("Limits: X[0,200] Y[-300,0] Z[0,10]")
        print("Press Ctrl+C to stop.")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
