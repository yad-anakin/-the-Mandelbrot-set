Fractal Explorer
================
Controls
--------
Click           : zoom in (4x faster zoom on click)
Shift+Click     : zoom out
Drag            : pan
Mouse wheel     : smooth zoom
+ / -           : increase / decrease iterations
R               : reset view
S               : save PNG snapshot

Fractals available
------------------
- Mandelbrot
- Julia (two presets)
- Burning Ship
- Tricorn (Mandelbar)

Notes on "infinity"
-------------------
Browser JavaScript uses 64-bit floats, so deep zoom is mathematically
limited to about 1e15x. Past that, pixels become blocky — a warning will
appear. Real "infinite" zoom requires arbitrary-precision math
(perturbation theory + reference orbits), which is outside what a single
HTML file can practically do. Until that limit, you can explore for
hours and find new structures everywhere.

Tip: press + a few times when deep-zoomed to reveal more detail
(higher iteration count = slower render but more accuracy).
