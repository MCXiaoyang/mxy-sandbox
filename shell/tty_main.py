"""mxy-sandbox TTY client with proper line editing."""

from __future__ import annotations

import json
import socket
import struct
import sys
import threading
import time

try:
    import msvcrt
    WINDOWS = True
except ImportError:
    WINDOWS = False

FRAME_MAGIC = 0x46524D31
TEXT_MAGIC  = 0x4C545854
FRAME_BYTES = 640 * 480 * 4
PROMPT = "> "

stdout_lock = threading.Lock()


class LineEditor:
    """单行编辑器：自己处理回显，避免和 reader 线程抢 stdout。"""

    def __init__(self, prompt: str = PROMPT) -> None:
        self.prompt = prompt
        self.buffer: list[str] = []

    def read_line(self) -> str:
        self.buffer = []
        with stdout_lock:
            sys.stdout.write(self.prompt)
            sys.stdout.flush()

        if WINDOWS:
            return self._read_windows()
        return self._read_unix()

    def _read_windows(self) -> str:
        while True:
            ch = msvcrt.getwch()
            if ch == "\r":
                with stdout_lock:
                    sys.stdout.write("\n")
                    sys.stdout.flush()
                return "".join(self.buffer)
            elif ch == "\x08":
                if self.buffer:
                    self.buffer.pop()
                    with stdout_lock:
                        sys.stdout.write("\b \b")
                        sys.stdout.flush()
            elif ch == "\x03":
                raise KeyboardInterrupt
            elif ch == "\x1b":
                for _ in range(2):
                    try:
                        msvcrt.getwch()
                    except Exception:
                        break
            elif ch and ch.isprintable():
                self.buffer.append(ch)
                with stdout_lock:
                    sys.stdout.write(ch)
                    sys.stdout.flush()

    def _read_unix(self) -> str:
        import termios, tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            while True:
                ch = sys.stdin.read(1)
                if ch in ("\r", "\n"):
                    with stdout_lock:
                        sys.stdout.write("\n")
                        sys.stdout.flush()
                    return "".join(self.buffer)
                elif ch in ("\x7f", "\x08"):
                    if self.buffer:
                        self.buffer.pop()
                        with stdout_lock:
                            sys.stdout.write("\b \b")
                            sys.stdout.flush()
                elif ch == "\x03":
                    raise KeyboardInterrupt
                elif ch == "\x1b":
                    sys.stdin.read(2)
                elif ch and ch.isprintable():
                    self.buffer.append(ch)
                    with stdout_lock:
                        sys.stdout.write(ch)
                        sys.stdout.flush()
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)


class SandboxTTY:
    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.create_connection((host, port))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.running = True
        self.editor = LineEditor()

    def close(self) -> None:
        self.running = False
        try:
            self.sock.sendall(b'{"type":"quit"}\n')
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass

    def send_cmd(self, name: str, arg: str = "", argi: int = 0) -> None:
        if not name or not name.strip():
            return
        line = json.dumps(
            {"type": "cmd", "name": name, "arg": arg, "argi": argi},
            separators=(",", ":"),
        ) + "\n"
        try:
            self.sock.sendall(line.encode())
        except OSError:
            self.running = False

    def _print_external(self, text: str) -> None:
        """从 reader 线程打印：清掉当前行 -> 打印 -> 重绘提示符 + 用户已输入内容。"""
        with stdout_lock:
            sys.stdout.write("\r" + " " * 100 + "\r")
            sys.stdout.write(text)
            if not text.endswith("\n"):
                sys.stdout.write("\n")
            sys.stdout.write(self.editor.prompt + "".join(self.editor.buffer))
            sys.stdout.flush()

    def reader_loop(self) -> None:
        buf = bytearray()
        self.sock.settimeout(0.5)
        while self.running:
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            buf.extend(chunk)

            while True:
                if len(buf) < 4:
                    break
                magic = struct.unpack("<I", bytes(buf[:4]))[0]

                if magic == FRAME_MAGIC:
                    if len(buf) < 4 + FRAME_BYTES:
                        break
                    del buf[: 4 + FRAME_BYTES]
                    continue

                if magic == TEXT_MAGIC:
                    if len(buf) < 8:
                        break
                    n = struct.unpack("<I", bytes(buf[4:8]))[0]
                    if len(buf) < 8 + n:
                        break
                    text = bytes(buf[8:8 + n]).decode("utf-8", errors="replace")
                    del buf[: 8 + n]
                    self._print_external(text)
                    continue

                self._print_external(f"[bad magic 0x{magic:08X}]\n")
                self.running = False
                return

        self.running = False
        self._print_external("\n[core disconnected]\n")

    def run(self) -> int:
        t = threading.Thread(target=self.reader_loop, daemon=True)
        t.start()
        time.sleep(0.4)

        try:
            while self.running:
                try:
                    line = self.editor.read_line()
                except (EOFError, KeyboardInterrupt):
                    print()
                    break

                line = line.strip()
                if not line:
                    continue

                parts = line.split(maxsplit=1)
                cmd = parts[0]
                arg = parts[1] if len(parts) > 1 else ""

                if cmd in ("quit", "exit"):
                    break

                if cmd == "kill":
                    try:
                        argi = int(arg)
                    except ValueError:
                        self._print_external("usage: kill <pid>\n")
                        continue
                    self.send_cmd("kill", argi=argi)
                else:
                    self.send_cmd(cmd, arg=arg)
        finally:
            self.close()

        return 0


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9000

    try:
        tty = SandboxTTY(host, port)
    except OSError as exc:
        print(f"[tty] cannot connect to {host}:{port}: {exc}")
        return 1

    print(f"[tty] connected to {host}:{port}")
    print("[tty] type 'help' for commands, 'quit' to exit")
    print()
    return tty.run()


if __name__ == "__main__":
    sys.exit(main())