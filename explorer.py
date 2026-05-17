"""
Fractal Explorer (desktop, fast)
================================
Vectorized NumPy Mandelbrot/Julia/Burning Ship renderer with a Pygame window.

Controls
--------
Left click       : zoom in (centered on cursor)
Right click      : zoom out
Mouse drag       : pan
Mouse wheel      : smooth zoom
+ / -            : more / fewer iterations
[ / ]            : decrease / increase resolution scale (lower = faster)
1..5             : switch fractal (1 Mandelbrot, 2 Julia A, 3 Julia B, 4 Burning Ship, 5 Tricorn)
P                : cycle palette
S                : save PNG snapshot to this folder
R                : reset view
Esc              : quit
"""
import os
import sys
import time
import math
import numpy as np

try:
    import pygame
except ImportError:
    print("Pygame is required. Install with:  pip install pygame numpy")
    sys.exit(1)

# ---------------- Config ----------------
WIN_W, WIN_H = 1100, 750
HERE = os.path.dirname(os.path.abspath(__file__))

# ---------------- Fractal kernels (vectorized) ----------------
def mandelbrot(c, max_iter):
    z = np.zeros_like(c)
    out = np.full(c.shape, max_iter, dtype=np.float64)
    mask = np.ones(c.shape, dtype=bool)
    # cardioid / period-2 bulb shortcut
    x = c.real; y = c.imag
    q = (x - 0.25) ** 2 + y * y
    inside = (q * (q + (x - 0.25)) <= 0.25 * y * y) | ((x + 1) ** 2 + y * y <= 0.0625)
    mask &= ~inside
    for i in range(max_iter):
        zm = z[mask]
        zm = zm * zm + c[mask]
        z[mask] = zm
        escaped = np.abs(zm) > 256
        if escaped.any():
            idx = np.where(mask)[0] if mask.ndim == 1 else None
            # smooth iter on escaped pixels
            absz = np.abs(zm[escaped])
            nu = np.log(np.log(absz) / math.log(2)) / math.log(2)
            sub = mask.copy()
            sub[mask] = escaped
            out[sub] = i + 1 - nu
            mask[sub] = False
        if not mask.any():
            break
    return out

def julia(c_grid, cx, cy, max_iter):
    z = c_grid.copy()
    cc = complex(cx, cy)
    out = np.full(z.shape, max_iter, dtype=np.float64)
    mask = np.ones(z.shape, dtype=bool)
    for i in range(max_iter):
        zm = z[mask]
        zm = zm * zm + cc
        z[mask] = zm
        escaped = np.abs(zm) > 256
        if escaped.any():
            absz = np.abs(zm[escaped])
            nu = np.log(np.log(absz) / math.log(2)) / math.log(2)
            sub = mask.copy()
            sub[mask] = escaped
            out[sub] = i + 1 - nu
            mask[sub] = False
        if not mask.any():
            break
    return out

def burningship(c, max_iter):
    x = np.zeros(c.shape); y = np.zeros(c.shape)
    out = np.full(c.shape, max_iter, dtype=np.float64)
    mask = np.ones(c.shape, dtype=bool)
    cr = c.real; ci = c.imag
    for i in range(max_iter):
        xm = x[mask]; ym = y[mask]
        x2 = xm * xm; y2 = ym * ym
        ny = np.abs(2 * xm * ym) + ci[mask]
        nx = x2 - y2 + cr[mask]
        x[mask] = nx; y[mask] = ny
        escaped = (x2 + y2) > 256
        if escaped.any():
            absz = np.sqrt(x2[escaped] + y2[escaped])
            nu = np.log(np.log(absz) / math.log(2)) / math.log(2)
            sub = mask.copy()
            sub[mask] = escaped
            out[sub] = i + 1 - nu
            mask[sub] = False
        if not mask.any():
            break
    return out

def tricorn(c, max_iter):
    z = np.zeros_like(c)
    out = np.full(c.shape, max_iter, dtype=np.float64)
    mask = np.ones(c.shape, dtype=bool)
    for i in range(max_iter):
        zm = z[mask]
        zm = np.conj(zm) ** 2 + c[mask]
        z[mask] = zm
        escaped = np.abs(zm) > 256
        if escaped.any():
            absz = np.abs(zm[escaped])
            nu = np.log(np.log(absz) / math.log(2)) / math.log(2)
            sub = mask.copy()
            sub[mask] = escaped
            out[sub] = i + 1 - nu
            mask[sub] = False
        if not mask.any():
            break
    return out

# ---------------- Palettes ----------------
def make_palette(kind, n=4096):
    t = np.linspace(0, 1, n)
    if kind == 0:  # Electric
        r = 9 * (1 - t) * t ** 3
        g = 15 * (1 - t) ** 2 * t ** 2
        b = 8.5 * (1 - t) ** 3 * t
        rgb = np.stack([r, g, b], axis=1)
        rgb = np.clip(rgb / rgb.max() * 1.4, 0, 1)
    elif kind == 1:  # Fire
        rgb = np.stack([np.clip(t ** 0.4, 0, 1), t ** 1.5, t ** 3.5], axis=1)
    elif kind == 2:  # Ice
        rgb = np.stack([t ** 3, t ** 1.4, np.clip(t ** 0.5, 0, 1)], axis=1)
    elif kind == 3:  # Rainbow
        h = (t * 5) % 1
        s = 0.85
        v = np.where(t < 0.02, 0, 1.0)
        i = (h * 6).astype(int)
        f = h * 6 - i
        p = v * (1 - s); q = v * (1 - f * s); u = v * (1 - (1 - f) * s)
        r = np.choose(i % 6, [v, q, p, p, u, v])
        g = np.choose(i % 6, [u, v, v, q, p, p])
        b = np.choose(i % 6, [p, p, u, v, v, q])
        rgb = np.stack([r, g, b], axis=1)
    else:  # Grayscale
        rgb = np.stack([t, t, t], axis=1)
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)

PALETTE_NAMES = ["Electric", "Fire", "Ice", "Rainbow", "Gray"]

# ---------------- Renderer ----------------
class Explorer:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WIN_W, WIN_H), pygame.RESIZABLE)
        pygame.display.set_caption("Fractal Explorer")
        self.font = pygame.font.SysFont("consolas", 14)
        self.clock = pygame.time.Clock()

        self.cx = -0.5
        self.cy = 0.0
        self.scale = 3.5  # width in complex plane
        self.base_iter = 256
        self.iter_boost = 0
        self.fractal = "mandelbrot"
        self.julia_c = (-0.8, 0.156)
        self.palette_idx = 0
        self.palette = make_palette(0)
        self.res_scale = 2  # 1=full, 2=half, 4=quarter (faster)
        self.pan_drag = None
        self.dirty = True
        self.last_render_ms = 0
        self.surface = None

    def reset(self):
        if self.fractal.startswith("julia"):
            self.cx, self.cy, self.scale = 0.0, 0.0, 3.5
        elif self.fractal == "burningship":
            self.cx, self.cy, self.scale = -0.5, -0.5, 3.5
        else:
            self.cx, self.cy, self.scale = -0.5, 0.0, 3.5
        self.iter_boost = 0
        self.dirty = True

    def screen_to_world(self, px, py, w, h):
        aspect = h / w
        ratio = self.scale / w
        wx = self.cx - self.scale / 2 + px * ratio
        wy = self.cy - (self.scale * aspect) / 2 + py * ratio
        return wx, wy

    def current_max_iter(self):
        # iter grows with zoom (and user boost)
        n = int((self.base_iter + 80 * math.log2(3.5 / self.scale + 1)) * (1.5 ** self.iter_boost))
        return max(64, n)

    def render(self):
        t0 = time.perf_counter()
        w, h = self.screen.get_size()
        rs = self.res_scale
        rw, rh = max(8, w // rs), max(8, h // rs)
        aspect = rh / rw
        max_iter = self.current_max_iter()

        xs = np.linspace(self.cx - self.scale / 2,
                         self.cx + self.scale / 2, rw)
        ys = np.linspace(self.cy - self.scale * aspect / 2,
                         self.cy + self.scale * aspect / 2, rh)
        X, Y = np.meshgrid(xs, ys)
        c = X + 1j * Y

        if self.fractal == "mandelbrot":
            it = mandelbrot(c, max_iter)
        elif self.fractal == "julia":
            it = julia(c, -0.8, 0.156, max_iter)
        elif self.fractal == "julia2":
            it = julia(c, -0.4, 0.6, max_iter)
        elif self.fractal == "burningship":
            it = burningship(c, max_iter)
        elif self.fractal == "tricorn":
            it = tricorn(c, max_iter)
        else:
            it = mandelbrot(c, max_iter)

        # Map to colors. Inside set -> black.
        inside = it >= max_iter
        t = np.sqrt(np.clip(it, 0, max_iter) / max_iter)  # smooth, perceptual
        idx = np.clip((t * (len(self.palette) - 1)).astype(int), 0, len(self.palette) - 1)
        rgb = self.palette[idx]
        rgb[inside] = 0

        # rgb is (rh, rw, 3). Pygame surfarray wants (w, h, 3).
        arr = np.transpose(rgb, (1, 0, 2)).copy()
        small = pygame.surfarray.make_surface(arr)
        self.surface = pygame.transform.scale(small, (w, h))
        self.last_render_ms = (time.perf_counter() - t0) * 1000
        self.dirty = False

    def draw_hud(self):
        w, h = self.screen.get_size()
        lines = [
            f"Fractal : {self.fractal}",
            f"Center  : {self.cx:.15f}, {self.cy:.15f}",
            f"Zoom    : {3.5 / self.scale:.3e}x   Scale {self.scale:.3e}",
            f"Iter    : {self.current_max_iter()}   (+/- to adjust, boost={self.iter_boost})",
            f"Palette : {PALETTE_NAMES[self.palette_idx]}   (P)",
            f"ResScale: 1/{self.res_scale}   ([ / ])   Render: {self.last_render_ms:.0f} ms",
            f"Save: S   Reset: R   1-5: fractal",
        ]
        if self.scale < 1e-13:
            lines.append("WARNING: float64 precision limit — pixelation expected")
        bg = pygame.Surface((460, 18 * len(lines) + 10), pygame.SRCALPHA)
        bg.fill((0, 0, 0, 160))
        self.screen.blit(bg, (8, 8))
        for i, ln in enumerate(lines):
            self.screen.blit(self.font.render(ln, True, (240, 240, 240)), (16, 12 + i * 18))

    def save_png(self):
        if self.surface is None:
            return
        fname = os.path.join(HERE, f"fractal_{int(time.time())}.png")
        pygame.image.save(self.surface, fname)
        print("Saved:", fname)

    def zoom_at(self, px, py, factor):
        w, h = self.screen.get_size()
        wx, wy = self.screen_to_world(px, py, w, h)
        self.cx = wx + (self.cx - wx) * factor
        self.cy = wy + (self.cy - wy) * factor
        self.scale *= factor
        self.dirty = True

    def run(self):
        self.render()
        running = True
        while running:
            for ev in pygame.event.get():
                if ev.type == pygame.QUIT:
                    running = False
                elif ev.type == pygame.VIDEORESIZE:
                    self.screen = pygame.display.set_mode(ev.size, pygame.RESIZABLE)
                    self.dirty = True
                elif ev.type == pygame.KEYDOWN:
                    k = ev.key
                    if k == pygame.K_ESCAPE: running = False
                    elif k == pygame.K_r: self.reset()
                    elif k in (pygame.K_PLUS, pygame.K_EQUALS, pygame.K_KP_PLUS):
                        self.iter_boost += 1; self.dirty = True
                    elif k in (pygame.K_MINUS, pygame.K_KP_MINUS):
                        self.iter_boost -= 1; self.dirty = True
                    elif k == pygame.K_LEFTBRACKET:
                        self.res_scale = min(8, self.res_scale * 2); self.dirty = True
                    elif k == pygame.K_RIGHTBRACKET:
                        self.res_scale = max(1, self.res_scale // 2); self.dirty = True
                    elif k == pygame.K_p:
                        self.palette_idx = (self.palette_idx + 1) % len(PALETTE_NAMES)
                        self.palette = make_palette(self.palette_idx); self.dirty = True
                    elif k == pygame.K_s: self.save_png()
                    elif k == pygame.K_1: self.fractal = "mandelbrot"; self.reset()
                    elif k == pygame.K_2: self.fractal = "julia"; self.reset()
                    elif k == pygame.K_3: self.fractal = "julia2"; self.reset()
                    elif k == pygame.K_4: self.fractal = "burningship"; self.reset()
                    elif k == pygame.K_5: self.fractal = "tricorn"; self.reset()
                elif ev.type == pygame.MOUSEBUTTONDOWN:
                    if ev.button == 1:  # left = zoom in
                        self.zoom_at(*ev.pos, 0.4)
                    elif ev.button == 3:  # right = zoom out
                        self.zoom_at(*ev.pos, 2.5)
                    elif ev.button == 2:  # middle drag start
                        self.pan_drag = (ev.pos, self.cx, self.cy)
                    elif ev.button == 4:  # wheel up
                        self.zoom_at(*ev.pos, 1 / 1.25)
                    elif ev.button == 5:  # wheel down
                        self.zoom_at(*ev.pos, 1.25)
                elif ev.type == pygame.MOUSEBUTTONUP:
                    if ev.button == 2: self.pan_drag = None
                elif ev.type == pygame.MOUSEMOTION:
                    if ev.buttons[1] and self.pan_drag:  # middle drag
                        (sx, sy), c0x, c0y = self.pan_drag
                        w, _ = self.screen.get_size()
                        ratio = self.scale / w
                        self.cx = c0x - (ev.pos[0] - sx) * ratio
                        self.cy = c0y - (ev.pos[1] - sy) * ratio
                        self.dirty = True

            if self.dirty:
                self.render()

            if self.surface is not None:
                self.screen.blit(self.surface, (0, 0))
            self.draw_hud()
            pygame.display.flip()
            self.clock.tick(60)
        pygame.quit()


if __name__ == "__main__":
    Explorer().run()
