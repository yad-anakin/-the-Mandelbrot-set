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


C / MPFR Mandelbrot Explorer (High‑Precision Engine)
====================================================

This repository also contains a **native Windows C application** that uses
OpenGL and optional **MPFR/GMP arbitrary‑precision arithmetic** to explore the
Mandelbrot set at extremely deep zoom levels.

Project folder:

    fractals_c\

The C engine provides three rendering paths:

- **GPU shader (single/double precision)** — very fast preview.
- **CPU double renderer** — high‑quality reference matching the GPU geometry.
- **MPFR perturbation renderer** — arbitrary‑precision deep zoom using
  reference orbits and perturbation theory.


Building the C/MPFR Explorer (Windows, MSVC)
-------------------------------------------

Requirements:

- Visual Studio 2022/2025/2026 with C++ build tools ("MSVC" toolchain).
- [vcpkg](https://github.com/microsoft/vcpkg) installed.
- MPFR + GMP installed via vcpkg for the `x64-windows` triplet.

Example vcpkg commands (run once):

    vcpkg install mpfr:x64-windows gmp:x64-windows

The repository already contains a convenience script that:

- Sets up the Visual Studio build environment.
- Compiles the C sources with `USE_MPFR` defined.
- Links against `mpfr.lib` and `gmp.lib` from vcpkg.

To build the MPFR‑enabled executable:

1. Open a normal Windows command prompt.
2. Change directory to the C project root:

       cd C:\Users\...\-the-Mandelbrot-set\fractals_c

3. Run the build script:

       build_mpfr.bat

   On success it produces:

       main.exe

4. If `main.exe` fails to start with a message like

       mpfr-6.dll was not found

   copy these runtime DLLs from your vcpkg installation into the
   `fractals_c` folder next to `main.exe`:

   - `mpfr-6.dll`
   - `gmp-10.dll`

   Typical location (adjust user name / path as needed):

       C:\Users\<you>\vcpkg\installed\x64-windows\bin


Running the C Mandelbrot Explorer
---------------------------------

From `fractals_c` double‑click:

    main.exe

You should see a window titled:

    Mandelbrot Set Explorer (Double Precision, Multithreaded)

The app renders directly into an OpenGL back buffer and draws a HUD using
the Win32 GDI API.


Keyboard and Mouse Controls (C Engine)
--------------------------------------

Mouse:

- **Drag**              – Pan.
- **Mouse Wheel**       – Smooth zoom around the cursor.

Keyboard:

- **1 / 2 / 3**         – Quality levels (Standard / High / Maximum).
- **SPACE**             – High‑quality CPU view (hybrid: CPU double or MPFR).
- **A**                 – Toggle *Auto MPFR* (hybrid high‑quality on zoom).
- **P**                 – Toggle GPU double‑precision shader (if supported).
- **C**                 – Cycle color palettes.
- **R**                 – Reset view to the default.
- **S**                 – Save current view as a BMP (GPU/CPU image).
- **I**                 – MPFR snapshot: high‑precision deep‑zoom BMP.
- **H**                 – Show/hide HUD overlay.
- **ESC**               – Quit.


Quality Levels and Iterations
-----------------------------

`Quality` controls supersampling (SSAA) and influences how many iterations are
used at a given zoom:

- **Standard**  – 1x SSAA, moderate iterations, fast.
- **High**      – 4x SSAA, more iterations.
- **Maximum**   – 16x SSAA, many iterations (GPU‑heavy).

The engine also adjusts `Iterations` dynamically based on `Zoom` and quality
so that deep zooms automatically get higher iteration counts.


Precision Modes (GPU vs CPU vs MPFR)
------------------------------------

The HUD shows a `Precision:` line indicating which numeric engine produced
the current image:

- `Single (GPU)`  – Default OpenGL shader, fast preview.
- `Double (GPU)`  – Double‑precision shader if the driver supports it.
- `CPU (Double)`  – High‑quality CPU renderer using 64‑bit doubles.
- `MPFR (CPU)`    – Arbitrary‑precision MPFR perturbation engine.

`P` toggles the GPU between single and double shaders (when supported). This
only affects the live GPU preview path.


Hybrid High‑Quality Rendering (SPACE / Auto MPFR)
-------------------------------------------------

Pressing **SPACE** (or zooming with **Auto MPFR: ON**) triggers a high‑quality
render into an internal CPU pixel buffer and then displays that buffer
directly in the window.

The high‑quality path is **hybrid**:

- At **moderate zooms** (below a threshold, ~`1e6`):
  - Uses the CPU double renderer (`CPU (Double)` in the HUD).
  - Geometry matches the GPU image exactly, but with higher iterations and
    optional supersampling.

- At **very deep zooms** (beyond the threshold):
  - Uses the MPFR perturbation engine (`MPFR (CPU)` in the HUD).
  - A high‑precision reference orbit is computed at the current center using
    MPFR, and nearby pixels are evaluated by perturbation in double precision.
  - This allows zoom factors far beyond what 64‑bit floats can represent.

`Auto MPFR` controls whether the hybrid high‑quality render is triggered
automatically on each mouse‑wheel zoom step.


MPFR Snapshot (Key I)
---------------------

The `I` key always triggers a **full MPFR snapshot** regardless of zoom:

- Uses aggressive iteration counts (up to ~250k) based on the current zoom.
- Renders the view into the CPU buffer using `RenderMandelbrotMPFR`.
- Saves a BMP file named like:

      mpfr_zoom_5.66e+07.bmp

  in the `fractals_c` directory.

This is intended for very deep, "poster‑quality" snapshots where render time
is less important than mathematical accuracy.

