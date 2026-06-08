#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdlib.h>
#include "renderer.h"
#include "big_mandelbrot.h"

// Global View State and dimensions
static ViewState g_View;
static uint32_t* g_PixelBuffer = NULL;
static int g_BufferWidth = 0;
static int g_BufferHeight = 0;
static double g_IterScale = 1.0; // Global iteration scaling factor
static bool g_HighPrecision = true; // Whether to use double‑precision shaders
static double g_LastRenderTimeMs = 0.0;
static bool g_IsHighQualityRendered = false;
static bool g_MPFRAvailable = false; // true when MPFR engine compiled in
static bool g_MPFRFrameValid = false; // true when a CPU MPFR frame is available for display
static bool g_AutoMPFR = false; // when true, zooming auto-triggers MPFR high-quality render
static bool g_LastHQWasMPFR = false; // true if the current CPU frame was rendered with MPFR

// Threshold above which we switch from CPU double renderer to MPFR perturbation
static const double MPFR_ZOOM_THRESHOLD = 1.0e6;

// Rendering quality levels (0=Standard,1=High,2=Maximum)
static const double QUALITY_SCALES[] = { 1.0, 2.0, 5.0 };
static int g_QualityLevel = 0; // default Standard

static int ComputeIterations(double zoom, int qualityLevel) {
    int base = (qualityLevel == 0) ? 250 : ((qualityLevel == 1) ? 1000 : 5000);
    double logZoom = log2(zoom);
    if (logZoom < 0.0) logZoom = 0.0;
    int extra = (int)(logZoom * 100.0);
    int total = base + extra;
    if (total > 30000) total = 30000;
    return total;
}

static void UpdateQuality(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    g_QualityLevel = level;
    g_IterScale = QUALITY_SCALES[g_QualityLevel];
    g_View.ssaa = (g_QualityLevel == 0) ? 1 : ((g_QualityLevel == 1) ? 2 : 4);
    g_View.maxIterations = ComputeIterations(g_View.zoom, g_QualityLevel);
    g_IsHighQualityRendered = (g_QualityLevel == 0);
    g_MPFRFrameValid = false;
}

static void InvalidateHighQuality(void) {
    g_MPFRFrameValid = false;
    g_IsHighQualityRendered = (g_QualityLevel == 0);
}

static void RenderMPFRSnapshot(HWND hwnd) {
    if (!g_MPFRAvailable) {
        MessageBoxA(hwnd,
            "MPFR engine is not available in this build.\n\n"
            "Rebuild with USE_MPFR defined and link against mpfr + gmp to enable it.",
            "MPFR not enabled",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!g_PixelBuffer || g_BufferWidth <= 0 || g_BufferHeight <= 0) {
        MessageBoxA(hwnd,
            "No pixel buffer allocated yet. Resize the window and try again.",
            "MPFR snapshot",
            MB_OK | MB_ICONWARNING);
        return;
    }

    // Push iterations much higher than the normal GPU preview for deep zoom.
    // Always base this on the maximum quality level rather than current GPU quality.
    int base = ComputeIterations(g_View.zoom, 2);
    int scaled = base * 12; // heavy multiplier for crisp boundaries
    if (scaled < 10000) scaled = 10000;
    if (scaled > 250000) scaled = 250000;
    g_View.maxIterations = scaled;

    double ms = RenderMandelbrotMPFR(&g_View, g_PixelBuffer,
                                     g_BufferWidth, g_BufferHeight,
                                     0);
    g_LastRenderTimeMs = ms;

    char path[MAX_PATH];
    sprintf(path, "mpfr_zoom_%.2e.bmp", g_View.zoom);
    if (SaveBMP(path, g_PixelBuffer, g_BufferWidth, g_BufferHeight)) {
        char msg[256];
        sprintf(msg, "MPFR render finished in %.2f ms\nSaved as %s", ms, path);
        MessageBoxA(hwnd, msg, "MPFR snapshot", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(hwnd,
            "MPFR render finished but failed to save BMP.",
            "MPFR snapshot",
            MB_OK | MB_ICONERROR);
    }
}
static void RenderMPFRHighQuality(HWND hwnd) {
    if (!g_MPFRAvailable) {
        MessageBoxA(hwnd,
            "MPFR engine is not available in this build.\n\n"
            "Rebuild with USE_MPFR defined and link against mpfr + gmp to enable it.",
            "MPFR not enabled",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!g_PixelBuffer || g_BufferWidth <= 0 || g_BufferHeight <= 0) {
        MessageBoxA(hwnd,
            "No pixel buffer allocated yet. Resize the window and try again.",
            "MPFR high-quality view",
            MB_OK | MB_ICONWARNING);
        return;
    }

    // Use the same aggressive iteration scaling as the MPFR snapshot so
    // deep zooms have crisp, detailed boundaries instead of blobby shapes.
    int base = ComputeIterations(g_View.zoom, 2);
    int scaled = base * 12;
    if (scaled < 10000) scaled = 10000;
    if (scaled > 250000) scaled = 250000;
    g_View.maxIterations = scaled;

    double ms = RenderMandelbrotMPFR(&g_View, g_PixelBuffer,
                                     g_BufferWidth, g_BufferHeight,
                                     0);
    if (ms <= 0.0) {
        MessageBoxA(hwnd,
            "MPFR render failed.",
            "MPFR high-quality view",
            MB_OK | MB_ICONERROR);
        return;
    }

    g_LastRenderTimeMs = ms;
    g_MPFRFrameValid = true;
    g_IsHighQualityRendered = true;
    g_LastHQWasMPFR = true;

    InvalidateRect(hwnd, NULL, FALSE);
}

// High-quality CPU render using the standard double-precision Mandelbrot engine.
// This matches the GPU geometry but can push iterations higher for cleaner detail.
static void RenderCPUHighQuality(HWND hwnd) {
    if (!g_PixelBuffer || g_BufferWidth <= 0 || g_BufferHeight <= 0) {
        MessageBoxA(hwnd,
            "No pixel buffer allocated yet. Resize the window and try again.",
            "CPU high-quality view",
            MB_OK | MB_ICONWARNING);
        return;
    }

    int base = ComputeIterations(g_View.zoom, 2);
    int scaled = base * 6;
    if (scaled < 8000) scaled = 8000;
    if (scaled > 200000) scaled = 200000;
    g_View.maxIterations = scaled;

    double ms = RenderMandelbrot(&g_View, g_PixelBuffer,
                                 g_BufferWidth, g_BufferHeight,
                                 0);
    if (ms <= 0.0) {
        MessageBoxA(hwnd,
            "CPU high-quality render failed.",
            "CPU high-quality view",
            MB_OK | MB_ICONERROR);
        return;
    }

    g_LastRenderTimeMs = ms;
    g_MPFRFrameValid = true;
    g_IsHighQualityRendered = true;
    g_LastHQWasMPFR = false;

    InvalidateRect(hwnd, NULL, FALSE);
}

// Hybrid high-quality path: use CPU double renderer at moderate zooms,
// and MPFR perturbation only at very deep zooms where double precision fails.
static void RenderHighQualityView(HWND hwnd) {
    if (g_View.zoom < MPFR_ZOOM_THRESHOLD) {
        RenderCPUHighQuality(hwnd);
    } else {
        RenderMPFRHighQuality(hwnd);
    }
}


// Mouse tracking state
static BOOL g_IsDragging = FALSE;
static POINT g_DragStartMouse;
static double g_DragStartCenterX;
static double g_DragStartCenterY;

// Save current screen to a BMP file
BOOL SaveBMP(const char* filename, const uint32_t* buffer, int width, int height) {
    #pragma pack(push, 1)
    struct {
        uint16_t type;
        uint32_t size;
        uint16_t reserved1;
        uint16_t reserved2;
        uint32_t offBits;
    } fileHeader;

    struct {
        uint32_t size;
        int32_t  width;
        int32_t  height;
        uint16_t planes;
        uint16_t bitCount;
        uint32_t compression;
        uint32_t sizeImage;
        int32_t  xPelsPerMeter;
        int32_t  yPelsPerMeter;
        uint32_t clrUsed;
        uint32_t clrImportant;
    } infoHeader;
    #pragma pack(pop)

    int imageSize = width * height * 4;
    fileHeader.type = 0x4D42; // "BM"
    fileHeader.size = sizeof(fileHeader) + sizeof(infoHeader) + imageSize;
    fileHeader.reserved1 = 0;
    fileHeader.reserved2 = 0;
    fileHeader.offBits = sizeof(fileHeader) + sizeof(infoHeader);

    infoHeader.size = sizeof(infoHeader);
    infoHeader.width = width;
    // Use negative height for top-down bitmap
    infoHeader.height = -height;
    infoHeader.planes = 1;
    infoHeader.bitCount = 32;
    infoHeader.compression = 0;
    infoHeader.sizeImage = imageSize;
    infoHeader.xPelsPerMeter = 3780;
    infoHeader.yPelsPerMeter = 3780;
    infoHeader.clrUsed = 0;
    infoHeader.clrImportant = 0;

    FILE* f = fopen(filename, "wb");
    if (!f) return FALSE;

    fwrite(&fileHeader, sizeof(fileHeader), 1, f);
    fwrite(&infoHeader, sizeof(infoHeader), 1, f);
    fwrite(buffer, imageSize, 1, f);
    fclose(f);

    return TRUE;
}

static void DrawHUD(HDC hdc, int width, int height, const ViewState* view, int qualityLevel, double renderTimeMs, bool actualHighPrecision) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    
    HFONT hFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    HBRUSH hBrush = CreateSolidBrush(RGB(20, 20, 20));
    RECT rect = { 10, 10, 360, 240 };
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);
    
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rect.left, rect.top, NULL);
    LineTo(hdc, rect.right, rect.top);
    LineTo(hdc, rect.right, rect.bottom);
    LineTo(hdc, rect.left, rect.bottom);
    LineTo(hdc, rect.left, rect.top);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    
    char buf[128];
    int y = 20;
    int x = 25;
    
    #define HUD_WRITE(format, ...) { \
        sprintf(buf, format, __VA_ARGS__); \
        TextOutA(hdc, x, y, buf, (int)strlen(buf)); \
        y += 20; \
    }
    
    SetTextColor(hdc, RGB(0, 255, 255));
    HUD_WRITE("Mandelbrot Explorer v2.0", 0);
    
    SetTextColor(hdc, RGB(255, 255, 255));
    y += 5;
    HUD_WRITE("Center X:   %.15f", view->centerX);
    HUD_WRITE("Center Y:   %.15f", view->centerY);
    HUD_WRITE("Zoom:       %.2e", view->zoom);
    HUD_WRITE("Iterations: %d", view->maxIterations);
    if (g_MPFRFrameValid) {
        HUD_WRITE("Precision:  %s", g_LastHQWasMPFR ? "MPFR (CPU)" : "CPU (Double)");
    } else {
        HUD_WRITE("Precision:  %s", actualHighPrecision ? "Double (GPU)" : "Single (GPU)");
    }
    
    const char* qualityStr = (qualityLevel == 0) ? "Standard (1x SSAA)" : ((qualityLevel == 1) ? "High (4x SSAA)" : "Maximum (16x SSAA)");
    HUD_WRITE("Quality:    %s", qualityStr);
    HUD_WRITE("Palette:    %s", GetPaletteName(view->paletteIndex));
    HUD_WRITE("Auto MPFR:  %s", g_AutoMPFR ? "ON" : "OFF");
    
    if (qualityLevel > 0) {
        if (g_MPFRFrameValid) {
            SetTextColor(hdc, RGB(0, 255, 0));
            HUD_WRITE("Status:     Rendered (High Quality)", 0);
        } else {
            SetTextColor(hdc, RGB(255, 255, 0));
            HUD_WRITE("Status:     Preview (SPACE: High-Quality)", 0);
        }
    } else {
        SetTextColor(hdc, RGB(0, 255, 0));
        HUD_WRITE("Status:     Rendered (Standard)", 0);
    }
    
    SetTextColor(hdc, RGB(0, 255, 255));
    HUD_WRITE("Render Time: %.2f ms", renderTimeMs);
    
    RECT helpRect = { 10, height - 150, 360, height - 10 };
    HBRUSH hHelpBrush = CreateSolidBrush(RGB(20, 20, 20));
    FillRect(hdc, &helpRect, hHelpBrush);
    DeleteObject(hHelpBrush);
    
    hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, helpRect.left, helpRect.top, NULL);
    LineTo(hdc, helpRect.right, helpRect.top);
    LineTo(hdc, helpRect.right, helpRect.bottom);
    LineTo(hdc, helpRect.left, helpRect.bottom);
    LineTo(hdc, helpRect.left, helpRect.top);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    
    y = height - 140;
    SetTextColor(hdc, RGB(255, 255, 0));
    HUD_WRITE("Controls & Shortcuts:", 0);
    SetTextColor(hdc, RGB(255, 255, 255));
    HUD_WRITE("Drag Mouse      : Pan", 0);
    HUD_WRITE("Mouse Wheel     : Zoom", 0);
    HUD_WRITE("SPACE          : High-Quality view (CPU/MPFR)", 0);
    HUD_WRITE("Keys [1, 2, 3]  : Quality (Std/High/Max)", 0);
    HUD_WRITE("Key [P]         : Toggle GPU Double Precision", 0);
    HUD_WRITE("Key [C]         : Cycle Colors | [R]: Reset", 0);
    HUD_WRITE("Key [A]         : Toggle Auto MPFR on Zoom", 0);
    HUD_WRITE("Key [I]         : MPFR snapshot (%s)", g_MPFRAvailable ? "ON" : "OFF");
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    #undef HUD_WRITE
}

void InitViewState(ViewState* view) {
    view->centerX = -0.7;
    view->centerY = 0.0;
    view->zoom = 1.0;
    view->useHighPrecision = g_HighPrecision;
    view->paletteIndex = 0;
    view->showHUD = 1;
    view->ssaa = (g_QualityLevel == 0) ? 1 : ((g_QualityLevel == 1) ? 2 : 4);
    view->maxIterations = ComputeIterations(view->zoom, g_QualityLevel);
    InvalidateHighQuality();
}


// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            if (!RendererInit(hwnd, g_HighPrecision)) {
                MessageBoxA(hwnd, "Failed to initialize OpenGL renderer", "Error", MB_OK | MB_ICONERROR);
                PostQuitMessage(0);
                return 0;
            }
            InitViewState(&g_View);
            g_MPFRAvailable = BigMandelbrotAvailable() ? true : false;
            return 0;
        case WM_SIZE: {
            int newWidth = LOWORD(lParam);
            int newHeight = HIWORD(lParam);
            if (newWidth > 0 && newHeight > 0) {
                g_BufferWidth = newWidth;
                g_BufferHeight = newHeight;
                if (g_PixelBuffer) {
                    free(g_PixelBuffer);
                }
                g_PixelBuffer = (uint32_t*)malloc(sizeof(uint32_t) * g_BufferWidth * g_BufferHeight);
                RendererResize(g_BufferWidth, g_BufferHeight);
                InvalidateHighQuality();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_PixelBuffer && g_BufferWidth > 0 && g_BufferHeight > 0) {
                if (g_MPFRFrameValid) {
                    BITMAPINFO bmi;
                    ZeroMemory(&bmi, sizeof(bmi));
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = g_BufferWidth;
                    bmi.bmiHeader.biHeight = -g_BufferHeight; // top-down
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;

                    StretchDIBits(hdc,
                                  0, 0, g_BufferWidth, g_BufferHeight,
                                  0, 0, g_BufferWidth, g_BufferHeight,
                                  g_PixelBuffer,
                                  &bmi,
                                  DIB_RGB_COLORS,
                                  SRCCOPY);
                } else {
                    LARGE_INTEGER frequency, start, end;
                    QueryPerformanceFrequency(&frequency);
                    QueryPerformanceCounter(&start);

                    RendererRender(&g_View);
                    RendererPresent();

                    QueryPerformanceCounter(&end);
                    g_LastRenderTimeMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)frequency.QuadPart;
                }

                if (g_View.showHUD) {
                    DrawHUD(hdc, g_BufferWidth, g_BufferHeight, &g_View, g_QualityLevel, g_LastRenderTimeMs, RendererIsHighPrecision());
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            g_IsDragging = TRUE;
            g_DragStartMouse.x = LOWORD(lParam);
            g_DragStartMouse.y = HIWORD(lParam);
            g_DragStartCenterX = g_View.centerX;
            g_DragStartCenterY = g_View.centerY;
            SetCapture(hwnd);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_IsDragging) {
                g_IsDragging = FALSE;
                ReleaseCapture();
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_IsDragging) {
                int mouseX = LOWORD(lParam);
                int mouseY = HIWORD(lParam);
                int dx = mouseX - g_DragStartMouse.x;
                int dy = mouseY - g_DragStartMouse.y;
                double minDim = (g_BufferWidth < g_BufferHeight) ? g_BufferWidth : g_BufferHeight;
                double scale = 3.0 / (minDim * g_View.zoom);
                g_View.centerX = g_DragStartCenterX - dx * scale;
                g_View.centerY = g_DragStartCenterY + dy * scale;
                InvalidateHighQuality();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT screenMouse;
            screenMouse.x = LOWORD(lParam);
            screenMouse.y = HIWORD(lParam);
            ScreenToClient(hwnd, &screenMouse);
            double minDim = (g_BufferWidth < g_BufferHeight) ? g_BufferWidth : g_BufferHeight;
            double oldScale = 3.0 / (minDim * g_View.zoom);
            double mouseCr = g_View.centerX + (screenMouse.x - g_BufferWidth / 2.0) * oldScale;
            double mouseCi = g_View.centerY - (screenMouse.y - g_BufferHeight / 2.0) * oldScale;
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            double zoomMultiplier = (delta > 0) ? 1.25 : 0.8;
            g_View.zoom *= zoomMultiplier;
            if (g_View.zoom < 0.1) g_View.zoom = 0.1;
            // Recalculate max iterations for smooth detail during deep zoom
            g_View.maxIterations = (int)(200 * pow(g_View.zoom, 0.5) * g_IterScale);
            if (g_View.maxIterations < 5) g_View.maxIterations = 5;
            double newScale = 3.0 / (minDim * g_View.zoom);
            g_View.centerX = mouseCr - (screenMouse.x - g_BufferWidth / 2.0) * newScale;
            g_View.centerY = mouseCi + (screenMouse.y - g_BufferHeight / 2.0) * newScale;
            InvalidateHighQuality();
            InvalidateRect(hwnd, NULL, FALSE);
            if (g_AutoMPFR) {
                RenderHighQualityView(hwnd);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    break;
                case 'R': case 'r':
                    InitViewState(&g_View);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'C': case 'c':
                    g_View.paletteIndex = (g_View.paletteIndex + 1) % MAX_PALETTES;
                    InvalidateHighQuality();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'A': case 'a':
                    g_AutoMPFR = !g_AutoMPFR;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'H': case 'h':
                    g_View.showHUD = !g_View.showHUD;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case 'S': case 's': {
                    char path[MAX_PATH];
                    sprintf(path, "mandelbrot_zoom_%.2e.bmp", g_View.zoom);
                    if (SaveBMP(path, g_PixelBuffer, g_BufferWidth, g_BufferHeight)) {
                        char msg[256];
                        sprintf(msg, "Successfully saved view as %s", path);
                        MessageBoxA(hwnd, msg, "Image Saved", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxA(hwnd, "Failed to save screenshot.", "Error", MB_OK | MB_ICONERROR);
                    }
                    break;
                }
                case 'P': case 'p':
                    // Toggle high‑precision (double) shader usage
                    g_HighPrecision = !g_HighPrecision;
                    // Re‑initialize renderer to reload appropriate shader
                    RendererShutdown();
                    RendererInit(hwnd, g_HighPrecision);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_OEM_4: case 189: case VK_SUBTRACT:
                    // Decrease iteration scaling factor (lower quality)
                    UpdateQuality(g_QualityLevel - 1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_OEM_6: case 187: case VK_ADD:
                    // Increase iteration scaling factor (higher quality)
                    UpdateQuality(g_QualityLevel + 1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case '1':
                    // Standard quality
                    UpdateQuality(0);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case '2':
                    // High quality
                    UpdateQuality(1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case '3':
                    // Maximum quality (deep zoom, many iterations)
                    UpdateQuality(2);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case VK_SPACE:
                    // High-quality view in-window (CPU double or MPFR depending on zoom)
                    RenderHighQualityView(hwnd);
                    break;
                case 'I':
                case 'i':
                    // High‑precision MPFR snapshot (CPU, very deep zoom)
                    RenderMPFRSnapshot(hwnd);
                    break;
            }
            return 0;
        }
        case WM_DESTROY:
            if (g_PixelBuffer) {
                free(g_PixelBuffer);
                g_PixelBuffer = NULL;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "MandelbrotWindowClass";

    WNDCLASSA wc = {0};
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "Mandelbrot Set Explorer (Double Precision, Multithreaded)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
