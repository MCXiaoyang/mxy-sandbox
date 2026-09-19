"""mxy-sandbox shell entry point."""

from __future__ import annotations

import sys
import time

import pygame

from client import SandboxClient
from display import Display, WIDTH, HEIGHT
from input import translate_events


HELP_LINES = [
    "F1  toggle overlay",
    "R   restart demo programs",
    "SPC clear framebuffer",
    "L   list virtual filesystem",
    "P   list processes",
]


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9000

    client = SandboxClient(host, port)
    try:
        client.connect()
    except OSError as exc:
        print(f"[shell] cannot connect to {host}:{port}: {exc}")
        return 1

    print(f"[shell] connected to {host}:{port}")

    display = Display()
    clock = pygame.time.Clock()

    frames = 0
    fps = 0.0
    fps_t0 = time.time()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_F1:
                    display.show_overlay = not display.show_overlay
                elif event.key == pygame.K_r:
                    client.send_cmd("restart")
                elif event.key == pygame.K_SPACE:
                    client.send_cmd("clear")
                elif event.key == pygame.K_l:
                    client.send_cmd("ls", "/")
                elif event.key == pygame.K_p:
                    client.send_cmd("ps")

            for msg in translate_events([event]):
                client.send_json(msg)

        data = client.recv_frame()
        if data is None:
            print("[shell] core disconnected")
            break

        display.blit_frame(data)

        frames += 1
        now = time.time()
        if now - fps_t0 >= 0.5:
            fps = frames / (now - fps_t0)
            frames = 0
            fps_t0 = now

        if display.show_overlay:
            display.draw_overlay(
                [f"FPS {fps:5.1f}", f"{WIDTH}x{HEIGHT} RGBA"]
                + HELP_LINES
            )

        display.flip()
        display.set_title(f"mxy-sandbox - {fps:5.1f} fps")

        clock.tick(120)

    client.send_quit()
    client.close()
    Display.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
