"""TCP client for the mxy-sandbox core."""

from __future__ import annotations

import json
import socket
import struct

FRAME_MAGIC = 0x46524D31
WIDTH, HEIGHT = 640, 480
FRAME_BYTES = WIDTH * HEIGHT * 4


class SandboxClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9000) -> None:
        self.host = host
        self.port = port
        self.sock: socket.socket | None = None

    # -- connection ---------------------------------------------------------

    def connect(self, timeout: float = 5.0) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout)
        self.sock.settimeout(None)
        try:
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        except OSError:
            pass

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    @property
    def connected(self) -> bool:
        return self.sock is not None

    # -- framing ------------------------------------------------------------

    def _recv_exact(self, n: int) -> bytes | None:
        assert self.sock is not None
        buf = bytearray(n)
        view = memoryview(buf)
        got = 0
        while got < n:
            chunk = self.sock.recv_into(view[got:], n - got)
            if chunk == 0:
                return None
            got += chunk
        return bytes(buf)

    def recv_frame(self) -> bytes | None:
        """Read one frame. Returns raw RGBA bytes, or None on disconnect."""
        header = self._recv_exact(4)
        if header is None:
            return None
        magic = struct.unpack("<I", header)[0]
        if magic != FRAME_MAGIC:
            raise RuntimeError(f"bad frame magic 0x{magic:08X}")
        return self._recv_exact(FRAME_BYTES)

    # -- sending ------------------------------------------------------------

    def send_json(self, obj: dict) -> None:
        if self.sock is None:
            return
        line = json.dumps(obj, separators=(",", ":")) + "\n"
        self.sock.sendall(line.encode("utf-8"))

    def send_key(self, code: int, pressed: bool) -> None:
        self.send_json({"type": "key", "code": code, "pressed": pressed})

    def send_mouse(self, x: int, y: int, button: int, pressed: bool) -> None:
        self.send_json({
            "type": "mouse", "x": x, "y": y,
            "button": button, "pressed": pressed,
        })

    def send_mouse_move(self, x: int, y: int) -> None:
        self.send_json({"type": "mouse_move", "x": x, "y": y})

    def send_cmd(self, name: str, arg: str = "", argi: int = 0) -> None:
        self.send_json({"type": "cmd", "name": name, "arg": arg, "argi": argi})

    def send_quit(self) -> None:
        self.send_json({"type": "quit"})
