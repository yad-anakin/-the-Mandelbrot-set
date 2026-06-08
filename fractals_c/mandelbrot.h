#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PALETTES 5

// Coordinates of the view
typedef struct {
    double centerX;
    double centerY;
    double zoom;
    int maxIterations; // now unlimited, can be large
    int paletteIndex;
    int showHUD;
    bool useHighPrecision; // true when using GPU high precision or MPFR
    int ssaa;              // Supersampling anti-aliasing factor (1, 2, or 4)
    // Note: No explicit caps; user can set any value
} ViewState;

// Color structure (BGRA)
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} Color;

// Initialize view state
void InitViewState(ViewState* view);

// Render the Mandelbrot set into the pixel buffer
// buffer: pointer to width * height uint32_t pixels (BGRA format)
// width, height: dimensions of the buffer
// numThreads: number of CPU threads to use (0 for automatic based on system cores)
// returns: elapsed time in milliseconds for the rendering
double RenderMandelbrot(const ViewState* view, uint32_t* buffer, int width, int height, int numThreads);

// Get the name of the current palette
const char* GetPaletteName(int paletteIndex);

// Sample a palette at continuous position t (used by CPU and MPFR paths).
Color SamplePaletteColor(int paletteIndex, double t);

#endif // MANDELBROT_H
