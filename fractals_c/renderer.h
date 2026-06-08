#pragma once

#include <windows.h>
#include "mandelbrot.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the OpenGL renderer. Must be called from WM_CREATE with the window handle.
// Returns true on success, false on failure (error shown via MessageBox).
bool RendererInit(HWND hwnd, bool requestHighPrecision);

// Notify the renderer of a resize event (client area dimensions).
void RendererResize(int width, int height);

// Render a frame using the current view state. Called each WM_PAINT.
void RendererRender(const ViewState* view);

// Present the rendered OpenGL frame (SwapBuffers on the window DC).
void RendererPresent(void);

// Shut down the renderer and release OpenGL resources. Called on WM_DESTROY.
void RendererShutdown(void);

// Query if the renderer is currently using high-precision double shaders.
bool RendererIsHighPrecision(void);

#ifdef __cplusplus
}
#endif
