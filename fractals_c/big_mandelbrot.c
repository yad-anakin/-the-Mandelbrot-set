#include "big_mandelbrot.h"

#ifdef USE_MPFR

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <mpfr.h>

// We reuse the Color type and palette utilities from mandelbrot.c via
// this function, which is implemented there.
Color SamplePaletteColor(int paletteIndex, double t);

// ------------ Reference orbit (MPFR) ------------

typedef struct {
    double *x;
    double *y;
    int     len;      // number of valid entries in x/y
} RefOrbit;

static void RefOrbit_Free(RefOrbit *orb) {
    if (!orb) return;
    free(orb->x);
    free(orb->y);
    orb->x = orb->y = NULL;
    orb->len = 0;
}

// Compute high‑precision reference orbit at c0 = (centerX, centerY).
// Uses MPFR with precision chosen from zoom level.
static int RefOrbit_Compute(const ViewState *view, RefOrbit *out) {
    if (!out) return 0;

    int maxIter = view->maxIterations;
    if (maxIter < 8) maxIter = 8;

    // Choose precision: start from ~40 decimal digits and add a bit as zoom grows.
    double zoom = view->zoom;
    if (zoom < 1.0) zoom = 1.0;
    double log10z = log10(zoom);
    int digits = 40 + (int)(log10z * 4.0);  // +4 digits per decade of zoom
    if (digits < 40) digits = 40;
    if (digits > 200) digits = 200;
    int precBits = (int)(digits * 3.32192809489) + 32; // bits per decimal + margin

    mpfr_t zx, zy, cr, ci, tmp, zr2, zi2;
    mpfr_inits2(precBits, zx, zy, cr, ci, tmp, zr2, zi2, (mpfr_ptr)0);

    mpfr_set_d(cr, view->centerX, MPFR_RNDN);
    mpfr_set_d(ci, view->centerY, MPFR_RNDN);
    mpfr_set_d(zx, 0.0, MPFR_RNDN);
    mpfr_set_d(zy, 0.0, MPFR_RNDN);

    double *xs = (double *)malloc(sizeof(double) * (maxIter + 1));
    double *ys = (double *)malloc(sizeof(double) * (maxIter + 1));
    if (!xs || !ys) {
        free(xs); free(ys);
        mpfr_clears(zx, zy, cr, ci, tmp, zr2, zi2, (mpfr_ptr)0);
        return 0;
    }

    xs[0] = 0.0;
    ys[0] = 0.0;
    int n;
    int len = maxIter + 1;

    const double escape2 = 1e10; // large radius squared for smooth coloring

    for (n = 1; n <= maxIter; ++n) {
        // zx2 = zx*zx
        mpfr_mul(zr2, zx, zx, MPFR_RNDN);
        // zy2 = zy*zy
        mpfr_mul(zi2, zy, zy, MPFR_RNDN);
        // tmp = 2*zx*zy
        mpfr_mul(tmp, zx, zy, MPFR_RNDN);
        mpfr_mul_ui(tmp, tmp, 2u, MPFR_RNDN);

        // new zx = zx2 - zy2 + cr
        mpfr_sub(zx, zr2, zi2, MPFR_RNDN);
        mpfr_add(zx, zx, cr, MPFR_RNDN);

        // new zy = tmp + ci
        mpfr_add(zy, tmp, ci, MPFR_RNDN);

        // |z|^2 = zx^2 + zy^2
        mpfr_mul(zr2, zx, zx, MPFR_RNDN);
        mpfr_mul(zi2, zy, zy, MPFR_RNDN);
        mpfr_add(tmp, zr2, zi2, MPFR_RNDN);

        xs[n] = mpfr_get_d(zx, MPFR_RNDN);
        ys[n] = mpfr_get_d(zy, MPFR_RNDN);

        double mag2 = xs[n] * xs[n] + ys[n] * ys[n];
        if (mag2 > escape2) {
            len = n + 1; // we filled indices 0..n
            break;
        }
    }

    mpfr_clears(zx, zy, cr, ci, tmp, zr2, zi2, (mpfr_ptr)0);

    out->x = xs;
    out->y = ys;
    out->len = len;
    return 1;
}

// ------------ Perturbation iteration (per pixel, double) ------------

static double PerturbIteratePixel(double dcx, double dcy,
                                  const double *refX, const double *refY,
                                  int refLen, int maxIter)
{
    double dzx = 0.0, dzy = 0.0;
    const double log2_val = log(2.0);

    // we have refX[0..refLen-1]
    int limit = (refLen - 1 < maxIter) ? (refLen - 1) : maxIter;
    if (limit < 1) return maxIter;

    for (int n = 0; n < limit; ++n) {
        const double Zx = refX[n];
        const double Zy = refY[n];

        // delta_{n+1} = 2*Z_n*delta_n + delta_n^2 + delta_c
        double a = 2.0 * (Zx * dzx - Zy * dzy) + (dzx * dzx - dzy * dzy) + dcx;
        double b = 2.0 * (Zx * dzy + Zy * dzx) + 2.0 * dzx * dzy + dcy;
        dzx = a; dzy = b;

        // full value at n+1
        const double fx = refX[n+1] + dzx;
        const double fy = refY[n+1] + dzy;
        const double sq = fx*fx + fy*fy;
        if (sq > 65536.0) {
            double log_zn = log(sq) * 0.5;
            double nu = log(log_zn / log(2.0)) / log2_val;
            return (double)(n+1) + 1.0 - nu;
        }
    }

    // If reference escaped early, treat as escaped as well
    if (refLen - 1 < maxIter) {
        const double fx = refX[refLen-1] + dzx;
        const double fy = refY[refLen-1] + dzy;
        const double sq = fx*fx + fy*fy;
        double log_zn = log((sq > 1.0) ? sq : 1.0) * 0.5;
        double nu = log(log_zn / log(2.0)) / log2_val;
        return (double)(refLen-1) + 1.0 - nu;
    }

    return (double)maxIter;
}

// ------------ Thread worker ------------

typedef struct {
    const ViewState *view;
    uint32_t       *buffer;
    int             width;
    int             height;
    int             startY;
    int             endY;
    double          scale;
    const double   *refX;
    const double   *refY;
    int             refLen;
    int             maxIter;
} ThreadDataMPFR;

static DWORD WINAPI RenderSliceMPFR(LPVOID param) {
    ThreadDataMPFR *t = (ThreadDataMPFR *)param;

    const int w = t->width;
    const int h = t->height;
    const double scale = t->scale;
    const int maxIter = t->maxIter;
    const int paletteIdx = t->view->paletteIndex;

    for (int y = t->startY; y < t->endY; ++y) {
        uint32_t *row = t->buffer + (size_t)y * (size_t)w;
        double dy = (double)(y - h/2) * scale;  // imaginary offset (screen y goes down)

        for (int x = 0; x < w; ++x) {
            double dx = (double)(x - w/2) * scale;  // real offset

            double it = PerturbIteratePixel(dx, -dy,  // minus because ci mapping is inverted
                                            t->refX, t->refY,
                                            t->refLen, maxIter);

            if (it >= maxIter) {
                row[x] = 0xFF000000; // inside set: black
            } else {
                double tval = (paletteIdx == 4)
                    ? it * 0.1
                    : it * 0.015;
                Color c = SamplePaletteColor(paletteIdx, tval);
                row[x] = (255u << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
            }
        }
    }
    return 0;
}

// ------------ Public entry point ------------

double RenderMandelbrotMPFR(const ViewState* view,
                            uint32_t* buffer,
                            int width,
                            int height,
                            int numThreads)
{
    if (!view || !buffer || width <= 0 || height <= 0) return 0.0;

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    // Precompute MPFR reference orbit at the current center.
    RefOrbit orb = {0};
    if (!RefOrbit_Compute(view, &orb) || orb.len < 2) {
        RefOrbit_Free(&orb);
        return 0.0;
    }

    // Pixel scale (match mandelbrot.c mapping)
    double minDim = (width < height) ? (double)width : (double)height;
    double scale = 3.0 / (minDim * view->zoom);

    if (numThreads <= 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        numThreads = (int)si.dwNumberOfProcessors;
    }
    if (numThreads < 1) numThreads = 1;
    if (numThreads > 64) numThreads = 64;

    HANDLE *threads = (HANDLE *)malloc(sizeof(HANDLE) * numThreads);
    ThreadDataMPFR *td = (ThreadDataMPFR *)malloc(sizeof(ThreadDataMPFR) * numThreads);
    if (!threads || !td) {
        free(threads); free(td);
        RefOrbit_Free(&orb);
        return 0.0;
    }

    int rowsPer = height / numThreads;
    int rem = height % numThreads;
    int y = 0;

    for (int i = 0; i < numThreads; ++i) {
        td[i].view = view;
        td[i].buffer = buffer;
        td[i].width = width;
        td[i].height = height;
        td[i].scale = scale;
        td[i].refX = orb.x;
        td[i].refY = orb.y;
        td[i].refLen = orb.len;
        td[i].maxIter = view->maxIterations;
        td[i].startY = y;
        y += rowsPer;
        if (i == numThreads - 1) y += rem;
        td[i].endY = y;

        threads[i] = CreateThread(NULL, 0, RenderSliceMPFR, &td[i], 0, NULL);
    }

    WaitForMultipleObjects(numThreads, threads, TRUE, INFINITE);
    for (int i = 0; i < numThreads; ++i) {
        CloseHandle(threads[i]);
    }
    free(threads);
    free(td);
    RefOrbit_Free(&orb);

    QueryPerformanceCounter(&t1);
    double elapsedMs = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
    return elapsedMs;
}

int BigMandelbrotAvailable(void) {
    return 1;
}

#endif // USE_MPFR
