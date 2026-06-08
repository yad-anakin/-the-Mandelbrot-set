#include <windows.h>
#include <GL/gl.h>

// #include <GL/glext.h>  // Removed: not present on this system
#include "renderer.h"
#include "mandelbrot.h"



// Add missing standard headers for bool and size types
#include <stdbool.h>
#include <stdint.h>

// Define GLchar if not already defined (required for shader source strings)
#ifndef GLCHAR_DEFINED
typedef char GLchar;
#define GLCHAR_DEFINED
#endif



// Define missing GL constants for shader compilation/linking (OpenGL 1.1 headers lack them)
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif

// Define required OpenGL extension function pointer types
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *PFNGLUNIFORM1DPROC)(GLint location, GLdouble v0);
typedef void (APIENTRY *PFNGLUNIFORM2DPROC)(GLint location, GLdouble v0, GLdouble v1);



// Global pointers to OpenGL functions
static PFNGLCREATESHADERPROC        pglCreateShader = NULL;
static PFNGLSHADERSOURCEPROC       pglShaderSource = NULL;
static PFNGLCOMPILESHADERPROC      pglCompileShader = NULL;
static PFNGLGETSHADERIVPROC        pglGetShaderiv = NULL;
static PFNGLGETSHADERINFOLOGPROC   pglGetShaderInfoLog = NULL;
static PFNGLCREATEPROGRAMPROC      pglCreateProgram = NULL;
static PFNGLATTACHSHADERPROC       pglAttachShader = NULL;
static PFNGLLINKPROGRAMPROC        pglLinkProgram = NULL;
static PFNGLGETPROGRAMIVPROC       pglGetProgramiv = NULL;
static PFNGLGETPROGRAMINFOLOGPROC  pglGetProgramInfoLog = NULL;
static PFNGLUSEPROGRAMPROC         pglUseProgram = NULL;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = NULL;
static PFNGLUNIFORM1IPROC          pglUniform1i = NULL;
static PFNGLUNIFORM1FPROC          pglUniform1f = NULL;
static PFNGLUNIFORM2FPROC          pglUniform2f = NULL;
static PFNGLUNIFORM1DPROC          pglUniform1d = NULL;
static PFNGLUNIFORM2DPROC          pglUniform2d = NULL;

static HGLRC g_glRC = NULL;
static HWND g_hwnd = NULL;
static HDC g_hdc = NULL;
static bool g_useHighPrecision = false;
static GLuint g_shaderProgram = 0;

static int g_width = 0;
static int g_height = 0;

static const char* vertexShaderSrc = "#version 110\n"
    "varying vec2 vPos;\n"
    "void main() { vPos = gl_Vertex.xy; gl_Position = gl_Vertex; }";

static const char* vertexShaderSrcDouble = "#version 400 compatibility\n"
    "out vec2 vPos;\n"
    "void main() { vPos = gl_Vertex.xy; gl_Position = gl_Vertex; }";

static const char* fragmentShaderSrc = "#version 110\n"
    "varying vec2 vPos;\n"
    "uniform vec2 uCenter;\n"
    "uniform float uZoom;\n"
    "uniform int uMaxIter;\n"
    "uniform int uPaletteIndex;\n"
    "uniform int uSSAA;\n"
    "uniform vec2 uPixelSize;\n"
    "\n"
    "vec3 getPaletteColor(int paletteIdx, float t) {\n"
    "    t = t - floor(t);\n"
    "    if (t < 0.0) t += 1.0;\n"
    "    if (paletteIdx == 0) {\n"
    "        if (t < 0.15) return mix(vec3(0.0, 0.0, 0.0), vec3(0.118, 0.0, 0.471), (t - 0.0) / 0.15);\n"
    "        if (t < 0.42) return mix(vec3(0.118, 0.0, 0.471), vec3(0.863, 0.078, 0.196), (t - 0.15) / 0.27);\n"
    "        if (t < 0.75) return mix(vec3(0.863, 0.078, 0.196), vec3(1.0, 0.588, 0.0), (t - 0.42) / 0.33);\n"
    "        if (t < 0.90) return mix(vec3(1.0, 0.588, 0.0), vec3(1.0, 1.0, 0.784), (t - 0.75) / 0.15);\n"
    "        return mix(vec3(1.0, 1.0, 0.784), vec3(0.0, 0.0, 0.0), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (paletteIdx == 1) {\n"
    "        if (t < 0.20) return mix(vec3(0.0, 0.0, 0.039), vec3(0.0, 0.118, 0.314), (t - 0.0) / 0.20);\n"
    "        if (t < 0.45) return mix(vec3(0.0, 0.118, 0.314), vec3(0.0, 0.392, 0.706), (t - 0.20) / 0.25);\n"
    "        if (t < 0.70) return mix(vec3(0.0, 0.392, 0.706), vec3(0.196, 0.784, 1.0), (t - 0.45) / 0.25);\n"
    "        if (t < 0.90) return mix(vec3(0.196, 0.784, 1.0), vec3(0.784, 1.0, 1.0), (t - 0.70) / 0.20);\n"
    "        return mix(vec3(0.784, 1.0, 1.0), vec3(0.0, 0.0, 0.039), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (paletteIdx == 2) {\n"
    "        if (t < 0.25) return mix(vec3(0.0, 0.0, 0.0), vec3(0.706, 0.0, 0.0), (t - 0.0) / 0.25);\n"
    "        if (t < 0.55) return mix(vec3(0.706, 0.0, 0.0), vec3(1.0, 0.471, 0.0), (t - 0.25) / 0.30);\n"
    "        if (t < 0.85) return mix(vec3(1.0, 0.471, 0.0), vec3(1.0, 0.902, 0.196), (t - 0.55) / 0.30);\n"
    "        return mix(vec3(1.0, 0.902, 0.196), vec3(1.0, 1.0, 1.0), (t - 0.85) / 0.15);\n"
    "    }\n"
    "    if (paletteIdx == 3) {\n"
    "        if (t < 0.22) return mix(vec3(0.0, 0.0, 0.0), vec3(0.314, 0.0, 0.706), (t - 0.0) / 0.22);\n"
    "        if (t < 0.48) return mix(vec3(0.314, 0.0, 0.706), vec3(0.0, 1.0, 1.0), (t - 0.22) / 0.26);\n"
    "        if (t < 0.70) return mix(vec3(0.0, 1.0, 1.0), vec3(0.784, 0.0, 0.314), (t - 0.48) / 0.22);\n"
    "        if (t < 0.90) return mix(vec3(0.784, 0.0, 0.314), vec3(1.0, 0.784, 0.941), (t - 0.70) / 0.20);\n"
    "        return mix(vec3(1.0, 0.784, 0.941), vec3(0.0, 0.0, 0.0), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (t < 0.25) return mix(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), (t - 0.0) / 0.25);\n"
    "    if (t < 0.50) return mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), (t - 0.25) / 0.25);\n"
    "    if (t < 0.75) return mix(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), (t - 0.50) / 0.25);\n"
    "    return mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), (t - 0.75) / 0.25);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec3 sumCol = vec3(0.0);\n"
    "    for (int dx = 0; dx < 4; ++dx) {\n"
    "        if (dx >= uSSAA) break;\n"
    "        for (int dy = 0; dy < 4; ++dy) {\n"
    "            if (dy >= uSSAA) break;\n"
    "            vec2 offset = (vec2(float(dx) + 0.5, float(dy) + 0.5) / float(uSSAA) - vec2(0.5)) * uPixelSize;\n"
    "            vec2 c = uCenter + (vPos + offset) * uZoom;\n"
    "            vec2 z = vec2(0.0);\n"
    "            int i;\n"
    "            for(i = 0; i < 50000; ++i) {\n"
    "                if (i >= uMaxIter) break;\n"
    "                float x = (z.x * z.x - z.y * z.y) + c.x;\n"
    "                float y = (2.0 * z.x * z.y) + c.y;\n"
    "                if((x*x + y*y) > 65536.0) break;\n"
    "                z = vec2(x, y);\n"
    "            }\n"
    "            if (i >= uMaxIter) {\n"
    "                sumCol += vec3(0.0);\n"
    "            } else {\n"
    "                float log_zn = log(dot(z, z)) / 2.0;\n"
    "                float nu = log(log_zn) / 0.69314718056;\n"
    "                float smooth_iter = float(i) + 1.0 - nu;\n"
    "                float color_t = (uPaletteIndex == 4) ? (smooth_iter * 0.1) : (smooth_iter * 0.015);\n"
    "                sumCol += getPaletteColor(uPaletteIndex, color_t);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    gl_FragColor = vec4(sumCol / float(uSSAA * uSSAA), 1.0);\n"
    "}";

static const char* fragmentShaderSrcDouble = "#version 400 compatibility\n"
    "in vec2 vPos;\n"
    "uniform dvec2 uCenter;\n"
    "uniform double uZoom;\n"
    "uniform int uMaxIter;\n"
    "uniform int uPaletteIndex;\n"
    "uniform int uSSAA;\n"
    "uniform vec2 uPixelSize;\n"
    "\n"
    "vec3 getPaletteColor(int paletteIdx, float t) {\n"
    "    t = t - floor(t);\n"
    "    if (t < 0.0) t += 1.0;\n"
    "    if (paletteIdx == 0) {\n"
    "        if (t < 0.15) return mix(vec3(0.0, 0.0, 0.0), vec3(0.118, 0.0, 0.471), (t - 0.0) / 0.15);\n"
    "        if (t < 0.42) return mix(vec3(0.118, 0.0, 0.471), vec3(0.863, 0.078, 0.196), (t - 0.15) / 0.27);\n"
    "        if (t < 0.75) return mix(vec3(0.863, 0.078, 0.196), vec3(1.0, 0.588, 0.0), (t - 0.42) / 0.33);\n"
    "        if (t < 0.90) return mix(vec3(1.0, 0.588, 0.0), vec3(1.0, 1.0, 0.784), (t - 0.75) / 0.15);\n"
    "        return mix(vec3(1.0, 1.0, 0.784), vec3(0.0, 0.0, 0.0), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (paletteIdx == 1) {\n"
    "        if (t < 0.20) return mix(vec3(0.0, 0.0, 0.039), vec3(0.0, 0.118, 0.314), (t - 0.0) / 0.20);\n"
    "        if (t < 0.45) return mix(vec3(0.0, 0.118, 0.314), vec3(0.0, 0.392, 0.706), (t - 0.20) / 0.25);\n"
    "        if (t < 0.70) return mix(vec3(0.0, 0.392, 0.706), vec3(0.196, 0.784, 1.0), (t - 0.45) / 0.25);\n"
    "        if (t < 0.90) return mix(vec3(0.196, 0.784, 1.0), vec3(0.784, 1.0, 1.0), (t - 0.70) / 0.20);\n"
    "        return mix(vec3(0.784, 1.0, 1.0), vec3(0.0, 0.0, 0.039), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (paletteIdx == 2) {\n"
    "        if (t < 0.25) return mix(vec3(0.0, 0.0, 0.0), vec3(0.706, 0.0, 0.0), (t - 0.0) / 0.25);\n"
    "        if (t < 0.55) return mix(vec3(0.706, 0.0, 0.0), vec3(1.0, 0.471, 0.0), (t - 0.25) / 0.30);\n"
    "        if (t < 0.85) return mix(vec3(1.0, 0.471, 0.0), vec3(1.0, 0.902, 0.196), (t - 0.55) / 0.30);\n"
    "        return mix(vec3(1.0, 0.902, 0.196), vec3(1.0, 1.0, 1.0), (t - 0.85) / 0.15);\n"
    "    }\n"
    "    if (paletteIdx == 3) {\n"
    "        if (t < 0.22) return mix(vec3(0.0, 0.0, 0.0), vec3(0.314, 0.0, 0.706), (t - 0.0) / 0.22);\n"
    "        if (t < 0.48) return mix(vec3(0.314, 0.0, 0.706), vec3(0.0, 1.0, 1.0), (t - 0.22) / 0.26);\n"
    "        if (t < 0.70) return mix(vec3(0.0, 1.0, 1.0), vec3(0.784, 0.0, 0.314), (t - 0.48) / 0.22);\n"
    "        if (t < 0.90) return mix(vec3(0.784, 0.0, 0.314), vec3(1.0, 0.784, 0.941), (t - 0.70) / 0.20);\n"
    "        return mix(vec3(1.0, 0.784, 0.941), vec3(0.0, 0.0, 0.0), (t - 0.90) / 0.10);\n"
    "    }\n"
    "    if (t < 0.25) return mix(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), (t - 0.0) / 0.25);\n"
    "    if (t < 0.50) return mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), (t - 0.25) / 0.25);\n"
    "    if (t < 0.75) return mix(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), (t - 0.50) / 0.25);\n"
    "    return mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), (t - 0.75) / 0.25);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec3 sumCol = vec3(0.0);\n"
    "    for (int dx = 0; dx < 4; ++dx) {\n"
    "        if (dx >= uSSAA) break;\n"
    "        for (int dy = 0; dy < 4; ++dy) {\n"
    "            if (dy >= uSSAA) break;\n"
    "            vec2 offset = (vec2(float(dx) + 0.5, float(dy) + 0.5) / float(uSSAA) - vec2(0.5)) * uPixelSize;\n"
    "            dvec2 c = uCenter + dvec2(vPos + offset) * uZoom;\n"
    "            dvec2 z = dvec2(0.0);\n"
    "            int i;\n"
    "            for(i = 0; i < 50000; ++i) {\n"
    "                if (i >= uMaxIter) break;\n"
    "                double x = (z.x * z.x - z.y * z.y) + c.x;\n"
    "                double y = (2.0 * z.x * z.y) + c.y;\n"
    "                if((x*x + y*y) > 65536.0) break;\n"
    "                z = dvec2(x, y);\n"
    "            }\n"
    "            if (i >= uMaxIter) {\n"
    "                sumCol += vec3(0.0);\n"
    "            } else {\n"
    "                double log_zn = log(double(dot(z, z))) / 2.0;\n"
    "                double nu = log(log_zn) / 0.6931471805599453;\n"
    "                double smooth_iter = double(i) + 1.0 - nu;\n"
    "                float color_t = float((uPaletteIndex == 4) ? (smooth_iter * 0.1) : (smooth_iter * 0.015));\n"
    "                sumCol += getPaletteColor(uPaletteIndex, color_t);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    gl_FragColor = vec4(sumCol / float(uSSAA * uSSAA), 1.0);\n"
    "}";

static GLuint CompileShader(GLenum type, const char* source, bool showErrors) {
    if (!pglCreateShader) return 0;
    GLuint shader = pglCreateShader(type);
    if (!shader) return 0;
    pglShaderSource(shader, 1, &source, NULL);
    pglCompileShader(shader);
    GLint compiled = 0;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        if (showErrors) {
            char log[1024];
            pglGetShaderInfoLog(shader, 1024, NULL, log);
            MessageBoxA(NULL, log, "Shader Compile Error", MB_OK | MB_ICONERROR);
        }
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag, bool showErrors) {
    if (!vert || !frag || !pglCreateProgram) return 0;
    GLuint program = pglCreateProgram();
    if (!program) return 0;
    pglAttachShader(program, vert);
    pglAttachShader(program, frag);
    pglLinkProgram(program);
    GLint linked = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        if (showErrors) {
            char log[1024];
            pglGetProgramInfoLog(program, 1024, NULL, log);
            MessageBoxA(NULL, log, "Program Link Error", MB_OK | MB_ICONERROR);
        }
        return 0;
    }
    return program;
}

bool RendererInit(HWND hwnd, bool requestHighPrecision) {
    g_hwnd = hwnd;
    g_hdc = GetDC(hwnd);
    
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    
    int format = ChoosePixelFormat(g_hdc, &pfd);
    if (!format) {
        MessageBoxA(hwnd, "Failed to choose pixel format.", "Error", MB_OK);
        return false;
    }
    if (!SetPixelFormat(g_hdc, format, &pfd)) {
        MessageBoxA(hwnd, "Failed to set pixel format.", "Error", MB_OK);
        return false;
    }
    g_glRC = wglCreateContext(g_hdc);
    if (!g_glRC) {
        MessageBoxA(hwnd, "Failed to create GL context.", "Error", MB_OK);
        return false;
    }
    
    // Bind context before querying extension function pointers
    if (!wglMakeCurrent(g_hdc, g_glRC)) {
        MessageBoxA(hwnd, "Failed to make GL context current.", "Error", MB_OK);
        return false;
    }
    
    // Set a default clear color for the fractal background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Load required OpenGL extension functions
    pglCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    pglShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    pglCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    pglGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    pglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    pglCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    pglAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    pglLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    pglGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    pglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
    pglUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    pglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    pglUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    pglUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    pglUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
    pglUniform1d = (PFNGLUNIFORM1DPROC)wglGetProcAddress("glUniform1d");
    pglUniform2d = (PFNGLUNIFORM2DPROC)wglGetProcAddress("glUniform2d");
    
    if (!pglCreateShader || !pglShaderSource || !pglCompileShader || !pglCreateProgram || !pglUseProgram) {
        MessageBoxA(hwnd, "Your GPU/driver does not support basic OpenGL shaders.", "Error", MB_OK);
        return false;
    }
    
    g_useHighPrecision = false;
    
    if (requestHighPrecision && pglUniform1d && pglUniform2d) {
        // Attempt to compile high-precision (double) shaders
        GLuint vertDouble = CompileShader(GL_VERTEX_SHADER, vertexShaderSrcDouble, false);
        GLuint fragDouble = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrcDouble, false);
        if (vertDouble && fragDouble) {
            g_shaderProgram = LinkProgram(vertDouble, fragDouble, false);
            if (g_shaderProgram) {
                g_useHighPrecision = true;
            }
        }
    }
    
    // Fallback to single-precision shaders if double-precision failed or wasn't requested
    if (!g_useHighPrecision) {
        GLuint vertSingle = CompileShader(GL_VERTEX_SHADER, vertexShaderSrc, true);
        GLuint fragSingle = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc, true);
        g_shaderProgram = LinkProgram(vertSingle, fragSingle, true);
        if (!g_shaderProgram) {
            MessageBoxA(hwnd, "Failed to compile single-precision shaders.", "Error", MB_OK);
            return false;
        }
    }
    
    return true;
}

void RendererResize(int width, int height) {
    g_width = width;
    g_height = height;
    if (g_glRC && g_hdc) {
        wglMakeCurrent(g_hdc, g_glRC);
        glViewport(0, 0, width, height);
    }
}

void RendererRender(const ViewState* view) {
    if (!g_glRC || !g_hdc) return;
    wglMakeCurrent(g_hdc, g_glRC);
    glClear(GL_COLOR_BUFFER_BIT);
    pglUseProgram(g_shaderProgram);
    
    // Pixel size in vPos space
    float pixelSizeX = (g_width > 0) ? (2.0f / g_width) : 0.0f;
    float pixelSizeY = (g_height > 0) ? (2.0f / g_height) : 0.0f;
    
    GLint locPixelSize = pglGetUniformLocation(g_shaderProgram, "uPixelSize");
    pglUniform2f(locPixelSize, pixelSizeX, pixelSizeY);
    
    GLint locSSAA = pglGetUniformLocation(g_shaderProgram, "uSSAA");
    pglUniform1i(locSSAA, view->ssaa);
    
    GLint locPalette = pglGetUniformLocation(g_shaderProgram, "uPaletteIndex");
    pglUniform1i(locPalette, view->paletteIndex);
    
    if (g_useHighPrecision) {
        GLint locCenter = pglGetUniformLocation(g_shaderProgram, "uCenter");
        pglUniform2d(locCenter, view->centerX, view->centerY);
        GLint locZoom = pglGetUniformLocation(g_shaderProgram, "uZoom");
        pglUniform1d(locZoom, 2.0 / view->zoom);
        GLint locIter = pglGetUniformLocation(g_shaderProgram, "uMaxIter");
        pglUniform1i(locIter, view->maxIterations);
    } else {
        GLint locCenter = pglGetUniformLocation(g_shaderProgram, "uCenter");
        pglUniform2f(locCenter, (GLfloat)view->centerX, (GLfloat)view->centerY);
        GLint locZoom = pglGetUniformLocation(g_shaderProgram, "uZoom");
        pglUniform1f(locZoom, (GLfloat)(2.0 / view->zoom));
        GLint locIter = pglGetUniformLocation(g_shaderProgram, "uMaxIter");
        pglUniform1i(locIter, view->maxIterations);
    }
    
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f(-1.0f,  1.0f);
    glVertex2f( 1.0f,  1.0f);
    glEnd();
}

void RendererPresent(void) {
    if (g_glRC && g_hdc) {
        SwapBuffers(g_hdc);
    }
}

void RendererShutdown(void) {
    if (g_glRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_glRC);
        g_glRC = NULL;
    }
    if (g_hwnd && g_hdc) {
        ReleaseDC(g_hwnd, g_hdc);
        g_hdc = NULL;
        g_hwnd = NULL;
    }
}

bool RendererIsHighPrecision(void) {
    return g_useHighPrecision;
}
