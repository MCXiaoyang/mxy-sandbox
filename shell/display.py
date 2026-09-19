"""pygame rendering surface for the sandbox framebuffer."""

from __future__ import annotations

import pygame

WIDTH, HEIGHT = 640, 480


class Display:
    def __init__(self, title: str = "mxy-sandbox") -> None:
        pygame.init()
        pygame.display.set_caption(title)

        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        self.font = pygame.font.Font(None, 16)
        self.surface: pygame.Surface | None = None
        self.show_overlay = True

    def blit_frame(self, data: bytes) -> None:
        self.surface = pygame.image.frombuffer(data, (WIDTH, HEIGHT), "RGBA")
        self.screen.blit(self.surface, (0, 0))

    def draw_overlay(
        self,
        lines: list[str],
        color: tuple[int, int, int] = (255, 255, 0),
        bg: tuple[int, int, int, int] = (0, 0, 0, 170),
    ) -> None:
        if not lines:
            return

        pad = 6
        rendered = [self.font.render(t, True, color) for t in lines]
        width = max(s.get_width() for s in rendered) + pad * 2
        height = sum(s.get_height() for s in rendered) + pad * 2

        panel = pygame.Surface((width, height), pygame.SRCALPHA)
        panel.fill(bg)

        y = pad
        for s in rendered:
            panel.blit(s, (pad, y))
            y += s.get_height()

        self.screen.blit(panel, (8, 8))

    def flip(self) -> None:
        pygame.display.flip()

    def set_title(self, title: str) -> None:
        pygame.display.set_caption(title)

    @staticmethod
    def quit() -> None:
        pygame.quit()
