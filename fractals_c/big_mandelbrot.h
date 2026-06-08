#pragma once

// High-precision Mandelbrot renderer using MPFR + perturbation.
//
// Design:
//  - We compute a high-precision reference orbit Z_n at the current
//    view center c0 using MPFR (arbitrary precision).
//  - For each pixel, we iterate only the *difference* delta z around
//    that orbit in double precision (perturbation theory):
//        delta_{n+1} = 2*Z_n*delta_n + delta_n^2 + delta_c
//    and test escape of Z_n + delta_n.
//  - This gives extremely deep zoom while keeping per‑pixel cost close
//    to a normal double Mandelbrot.
//
// To enable MPFR you must:
//  - Install MPFR + GMP dev libraries on your system
//  - Build with USE_MPFR defined and link against mpfr + gmp.
//
// When USE_MPFR is *not* defined, these functions fall back to cheap
// stubs so the project still builds and runs without MPFR.

#include <stdint.h>
#include "mandelbrot.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_MPFR

// Render a high‑precision Mandelbrot frame into an RGBA buffer.
//
//  view        : current view state (center / zoom / iterations / palette)
//  buffer      : width * height pixels, ARGB/BGRA 0xAARRGGBB as in mandelbrot.c
//  width/height: dimensions of the buffer
//  numThreads  : 0 = auto (one per core)
//
// Returns wall‑clock render time in milliseconds.
double RenderMandelbrotMPFR(const ViewState* view,
                            uint32_t* buffer,
                            int width,
                            int height,
                            int numThreads);

// Indicates whether the MPFR engine is compiled in (always 1 when
// USE_MPFR is defined and this module is linked).
int BigMandelbrotAvailable(void);

#else  // !USE_MPFR

// Stub implementations when MPFR is not available. These let the rest
// of the program compile and run; the MPFR path simply does nothing.
static inline double RenderMandelbrotMPFR(const ViewState* view,
                                          uint32_t* buffer,
                                          int width,
                                          int height,
                                          int numThreads)
{
    (void)view; (void)buffer; (void)width; (void)height; (void)numThreads;
    return 0.0;
}

static inline int BigMandelbrotAvailable(void) { return 0; }

#endif // USE_MPFR

#ifdef __cplusplus
}
#endif
