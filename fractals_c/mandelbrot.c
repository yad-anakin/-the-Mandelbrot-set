#include "mandelbrot.h"
#include <windows.h>
#include <math.h>
#include <stdlib.h>

// Palette control points
typedef struct {
    double pos;
    Color color;
} PalettePoint;

typedef struct {
    const char* name;
    int count;
    PalettePoint points[8];
} Palette;

// Palette definitions (BGRA format)
static const Palette Palettes[MAX_PALETTES] = {
    // 0. Sunset Glow (Warm, rich colors)
    {
        "Sunset Glow", 6,
        {
            {0.0,  {0,   0,   0,   255}},     // Black
            {0.15, {120, 0,   30,  255}},     // Deep Indigo/Purple
            {0.42, {50,  20,  220, 255}},     // Hot Pink / Red
            {0.75, {0,   150, 255, 255}},     // Orange/Yellow
            {0.90, {200, 255, 255, 255}},     // Light Yellow/White
            {1.0,  {0,   0,   0,   255}}      // Wrap back to Black
        }
    },
    // 1. Deep Space (Cool, electric blues and cyans)
    {
        "Deep Space", 6,
        {
            {0.0,  {10,  0,   0,   255}},     // Near Black
            {0.20, {80,  30,  0,   255}},     // Dark Blue
            {0.45, {180, 100, 0,   255}},     // Electric Blue
            {0.70, {255, 200, 50,  255}},     // Neon Cyan
            {0.90, {255, 255, 200, 255}},     // Soft Cyan/White
            {1.0,  {10,  0,   0,   255}}      // Wrap back
        }
    },
    // 2. Fire (Classic fire gradient)
    {
        "Fire & Ash", 5,
        {
            {0.0,  {0,   0,   0,   255}},     // Black
            {0.25, {0,   0,   180, 255}},     // Dark Red
            {0.55, {0,   120, 255, 255}},     // Orange
            {0.85, {50,  230, 255, 255}},     // Yellow
            {1.0,  {255, 255, 255, 255}}      // White
        }
    },
    // 3. Psychedelic Neon (Vibrant green/magenta)
    {
        "Psychedelic Neon", 6,
        {
            {0.0,  {0,   0,   0,   255}},     // Black
            {0.22, {180, 0,   80,  255}},     // Dark Green
            {0.48, {255, 255, 0,   255}},     // Bright Cyan
            {0.70, {80,  0,   200, 255}},     // Hot Magenta/Pink
            {0.90, {240, 200, 255, 255}},     // White/Pink
            {1.0,  {0,   0,   0,   255}}      // Wrap back
        }
    },
    // 4. Zebra Stripe (High-contrast mathematical view)
    {
        "Zebra Stripes", 5,
        {
            {0.0,  {0,   0,   0,   255}},     // Black
            {0.25, {255, 255, 255, 255}},     // White
            {0.50, {0,   0,   0,   255}},     // Black
            {0.75, {255, 255, 255, 255}},     // White
            {1.0,  {0,   0,   0,   255}}      // Black
        }
    }
};

const char* GetPaletteName(int paletteIndex) {
    if (paletteIndex < 0 || paletteIndex >= MAX_PALETTES) {
        return "Unknown";
    }
    return Palettes[paletteIndex].name;
}

// Helper to interpolate colors
static inline Color InterpolateColor(const Palette* p, double t) {
    // Loop the palette position to prevent harsh cutoffs
    t = fmod(t, 1.0);
    if (t < 0.0) t += 1.0;

    for (int i = 0; i < p->count - 1; i++) {
        if (t >= p->points[i].pos && t <= p->points[i+1].pos) {
            double local_t = (t - p->points[i].pos) / (p->points[i+1].pos - p->points[i].pos);
            Color c1 = p->points[i].color;
            Color c2 = p->points[i+1].color;
            
            Color res;
            res.b = (uint8_t)(c1.b + local_t * (c2.b - c1.b));
            res.g = (uint8_t)(c1.g + local_t * (c2.g - c1.g));
            res.r = (uint8_t)(c1.r + local_t * (c2.r - c1.r));
            res.a = 255;
            return res;
        }
    }
    return p->points[p->count - 1].color;
}

// Public palette sampler used by both the standard CPU renderer and the
// MPFR perturbation engine.
Color SamplePaletteColor(int paletteIndex, double t) {
    if (paletteIndex < 0) paletteIndex = 0;
    if (paletteIndex >= MAX_PALETTES) paletteIndex = MAX_PALETTES - 1;
    const Palette* p = &Palettes[paletteIndex];
    return InterpolateColor(p, t);
}

// Thread data structure
typedef struct {
    const ViewState* view;
    uint32_t* buffer;
    int width;
    int height;
    int startY;
    int endY;
    double scale;
} ThreadData;

// Thread worker function
DWORD WINAPI RenderMandelbrotSlice(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    const ViewState* view = data->view;
    uint32_t* buffer = data->buffer;
    int w = data->width;
    int h = data->height;
    double scale = data->scale;
    int paletteIdx = view->paletteIndex;
    int maxIter = view->maxIterations;
    const Palette* activePalette = &Palettes[paletteIdx];
    
    // Log constants for smooth coloring
    double log2_val = log(2.0);

    for (int y = data->startY; y < data->endY; ++y) {
        // Map screen y to imaginary coordinate
        // Subtract screen center offset, scale, and invert y (complex plane goes up, screen goes down)
        double ci = view->centerY - (y - h / 2.0) * scale;
        
        uint32_t* rowBuffer = buffer + (y * w);

        for (int x = 0; x < w; ++x) {
            // Map screen x to real coordinate
            double cr = view->centerX + (x - w / 2.0) * scale;

            // Mandelbrot recurrence: z_next = z^2 + c
            double zr = cr;
            double zi = ci;
            int iter = 0;

            // Escape radius squared. R = 256 -> R^2 = 65536. 
            // Large radius is critical for smooth coloring!
            double zr2 = zr * zr;
            double zi2 = zi * zi;
            while (zr2 + zi2 <= 65536.0 && iter < maxIter) {
                double temp = zr2 - zi2 + cr;
                zi = 2.0 * zr * zi + ci;
                zr = temp;
                zr2 = zr * zr;
                zi2 = zi * zi;
                iter++;
            }

            if (iter == maxIter) {
                // Point is in the set
                rowBuffer[x] = 0xFF000000; // Black (ARGB format with Alpha=255)
            } else {
                // Smooth coloring:
                // nu = log(log(|z|)) / log(2)
                // |z| = sqrt(zr^2 + zi^2) -> log(|z|) = 0.5 * log(zr^2 + zi^2)
                // nu = log(0.5 * log(zr^2 + zi^2)) / log(2)
                double log_zn = log(zr2 + zi2) / 2.0;
                double nu = log(log_zn) / log2_val;
                double smooth_iter = (double)iter + 1.0 - nu;

                // Map smooth iteration to palette index
                // Apply scaling depending on palette preference to stretch colors
                double color_t;
                if (paletteIdx == 4) {
                    // Zebra stripe is high frequency
                    color_t = smooth_iter * 0.1;
                } else {
                    // Sunset, space, fire are smoother
                    color_t = smooth_iter * 0.015;
                }

                Color c = InterpolateColor(activePalette, color_t);
                
                // Pack BGRA into uint32_t
                rowBuffer[x] = (255 << 24) | (c.r << 16) | (c.g << 8) | c.b;
            }
        }
    }
    return 0;
}

double RenderMandelbrot(const ViewState* view, uint32_t* buffer, int width, int height, int numThreads) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    // Calculate aspect-ratio-aware scaling factor
    // Using min of width/height ensures square scaling (no distortion on resize)
    double minDim = (width < height) ? width : height;
    double scale = 3.0 / (minDim * view->zoom);

    // Auto-detect core count if thread count is 0
    if (numThreads <= 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numThreads = sysinfo.dwNumberOfProcessors;
    }
    if (numThreads < 1) numThreads = 1;
    if (numThreads > 64) numThreads = 64; // Win32 limit for single WaitForMultipleObjects without complexity

    HANDLE* threads = (HANDLE*)malloc(sizeof(HANDLE) * numThreads);
    ThreadData* threadData = (ThreadData*)malloc(sizeof(ThreadData) * numThreads);

    int rowsPerThread = height / numThreads;
    int remainingRows = height % numThreads;

    int currentY = 0;
    for (int i = 0; i < numThreads; ++i) {
        threadData[i].view = view;
        threadData[i].buffer = buffer;
        threadData[i].width = width;
        threadData[i].height = height;
        threadData[i].scale = scale;
        threadData[i].startY = currentY;
        
        currentY += rowsPerThread;
        if (i == numThreads - 1) {
            currentY += remainingRows; // Last thread handles remainder
        }
        threadData[i].endY = currentY;

        threads[i] = CreateThread(
            NULL,                       // Default security attributes
            0,                          // Default stack size
            RenderMandelbrotSlice,      // Thread function
            &threadData[i],             // Parameter
            0,                          // Default creation flags
            NULL                        // Receives thread identifier
        );
    }

    // Wait for all rendering threads to complete
    WaitForMultipleObjects(numThreads, threads, TRUE, INFINITE);

    // Clean up handles
    for (int i = 0; i < numThreads; ++i) {
        CloseHandle(threads[i]);
    }
    free(threads);
    free(threadData);

    QueryPerformanceCounter(&end);
    double elapsedMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)frequency.QuadPart;
    return elapsedMs;
}
