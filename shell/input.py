"""Translates pygame events into sandbox protocol messages."""

from __future__ import annotations

import pygame


def translate_events(events: list[pygame.event.Event]) -> list[dict]:
    out: list[dict] = []

    for e in events:
        if e.type == pygame.QUIT:
            out.append({"type": "quit"})

        elif e.type == pygame.KEYDOWN:
            out.append({"type": "key", "code": e.key, "pressed": True})

        elif e.type == pygame.KEYUP:
            out.append({"type": "key", "code": e.key, "pressed": False})

        elif e.type == pygame.MOUSEBUTTONDOWN:
            x, y = e.pos
            out.append({
                "type": "mouse", "x": x, "y": y,
                "button": e.button, "pressed": True,
            })

        elif e.type == pygame.MOUSEBUTTONUP:
            x, y = e.pos
            out.append({
                "type": "mouse", "x": x, "y": y,
                "button": e.button, "pressed": False,
            })

        elif e.type == pygame.MOUSEMOTION:
            x, y = e.pos
            out.append({"type": "mouse_move", "x": x, "y": y})

    return out
