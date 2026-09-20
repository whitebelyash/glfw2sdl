/*
 * GLFW support layer on top of SDL3 -- internal shared state.
 *
 * This file is part of the "glfw on SDL3" compatibility shim.  It is an
 * independent implementation that translates the GLFW 3.5 public API onto
 * SDL3; it does not contain any GLFW or SDL runtime code.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef GLFW_SDL_INTERNAL_H
#define GLFW_SDL_INTERNAL_H

#include <GLFW/glfw3.h>
#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Sticky input sentinel (matches GLFW semantics: a sticky press is    */
/* reported once by the next state query, then cleared).               */
/* ------------------------------------------------------------------ */
#define _GLFW_STICK_PRESS 2

/* ------------------------------------------------------------------ */
/* Monitor                                                             */
/* ------------------------------------------------------------------ */
typedef struct _GLFWmonitor
{
    struct _GLFWmonitor *next;
    SDL_DisplayID displayID;
    char name[256];
    void *userPointer;

    /* Cached video modes (owned by us, SDL_free array after rebuild) */
    SDL_DisplayMode **sdlModes;   /* raw SDL mode pointers/ownership */
    int sdlModeCount;
    GLFWvidmode *modes;           /* GLFW-converted modes */
    int modeCount;
    GLFWvidmode currentMode;      /* storage for glfwGetVideoMode */

    /* Gamma state.  SDL3 has no public gamma-ramp API, so we keep the */
    /* ramp ourselves; glfwGetGammaRamp() returns it, glfwSetGamma*()  */
    /* stores it (applied best-effort where the platform allows).      */
    unsigned short rampRed[256], rampGreen[256], rampBlue[256];
    GLFWgammaramp ramp;
    bool rampValid;
} _GLFWmonitor;

/* ------------------------------------------------------------------ */
/* Window                                                              */
/* ------------------------------------------------------------------ */
typedef struct _GLFWwindow
{
    struct _GLFWwindow *next;
    SDL_Window *sdlWindow;
    SDL_GLContext context;
    void *userPointer;
    char title[1024];                 /* current window title (glfwGetWindowTitle) */

    bool shouldClose;

    /* Creation-time / runtime hint snapshot */
    int clientAPI;            /* GLFW_OPENGL_API / GLFW_OPENGL_ES_API / GLFW_NO_API */
    int contextCreationAPI;   /* GLFW_NATIVE_CONTEXT_API / GLFW_EGL_CONTEXT_API */
    int contextVersionMajor, contextVersionMinor, contextRevision;
    int openGLForwardCompat, openGLDebugContext, openGLProfile;
    int contextRobustness, contextReleaseBehavior, contextNoError;
    int doublebuffer, stereo, srgbCapable, samples, frameBufferBits;
    int redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits;
    int accumRedBits, accumGreenBits, accumBlueBits, accumAlphaBits;
    int auxBuffers, refreshRate;
    int transparentFramebuffer;
    int resizable, visible, decorated, focused, floating;
    int autoIconify, focusOnShow, scaleToMonitor, mousePassthrough;
    int centerCursor;

    /* Pre-fullscreen geometry (restored by glfwSetWindowMonitor(NULL)) */
    int prevX, prevY, prevW, prevH;

    /* Input state */
    char keys[GLFW_KEY_LAST + 1];               /* GLFW_PRESS/RELEASE/_GLFW_STICK_PRESS */
    int  mouseButtons[GLFW_MOUSE_BUTTON_LAST + 1];
    double virtualX, virtualY;                  /* position reported to the app */
    int cursorMode;                             /* GLFW_CURSOR_* */
    bool relativeMode;                          /* cursor disabled -> relative deltas */
    bool stickyKeys, stickyMouseButtons;
    bool lockKeyMods, rawMouseMotion;
    SDL_Cursor *sdlCursor;                      /* custom cursor or NULL (default) */
    bool cursorEntered;
    bool fullscreen;
    bool maximized, iconified;

    /* Callbacks */
    GLFWwindowposfun          posCb;
    GLFWwindowsizefun         sizeCb;
    GLFWwindowclosefun        closeCb;
    GLFWwindowrefreshfun      refreshCb;
    GLFWwindowfocusfun        focusCb;
    GLFWwindowiconifyfun      iconifyCb;
    GLFWwindowmaximizefun     maximizeCb;
    GLFWframebuffersizefun    fbSizeCb;
    GLFWwindowcontentscalefun scaleCb;
    GLFWmousebuttonfun        mouseButtonCb;
    GLFWcursorposfun          cursorPosCb;
    GLFWcursorenterfun        cursorEnterCb;
    GLFWscrollfun             scrollCb;
    GLFWkeyfun                keyCb;
    GLFWcharfun               charCb;
    GLFWcharmodsfun           charModsCb;
    GLFWdropfun               dropCb;

    /* Drop -- paths accumulated between DROP_BEGIN and DROP_COMPLETE */
    char **dropPaths;
    int dropPathCount, dropPathCapacity;

    /* IME state (stored only; SDL3 does not expose preedit events) */
    GLFWpreeditfun            preeditCb;
    GLFWimestatusfun          imeStatusCb;
    GLFWpreeditcandidatefun   preeditCandidateCb;
    int preeditX, preeditY, preeditW, preeditH;
} _GLFWwindow;

/* ------------------------------------------------------------------ */
/* Cursor                                                              */
/* ------------------------------------------------------------------ */
typedef struct _GLFWcursor
{
    struct _GLFWcursor *next;
    SDL_Cursor *sdlCursor;
    bool standard; /* created from a standard cursor shape */
} _GLFWcursor;

/* ------------------------------------------------------------------ */
/* Joystick slot                                                       */
/* ------------------------------------------------------------------ */
typedef struct _GLFWjoystick
{
    SDL_JoystickID instanceID;  /* 0 == not connected */
    SDL_Joystick *sdlJoystick;  /* lazily opened */
    SDL_Gamepad  *sdlGamepad;   /* lazily opened when the device is a gamepad */
    bool isGamepad;
    char guidString[33];
    void *userPointer;

    /* Persistent result buffers (valid until the next query, like GLFW). */
    float *axes;
    int axesSize;
    unsigned char *buttons;
    int buttonsSize;
    unsigned char *hats;
    int hatsSize;
} _GLFWjoystick;

/* ------------------------------------------------------------------ */
/* Global state                                                        */
/* ------------------------------------------------------------------ */
typedef struct _GLFWglobal
{
    bool initialized;
    int refcount;

    _GLFWwindow  *windowList;
    _GLFWmonitor *monitorList;
    GLFWmonitor **monitorArray;   /* mirrors monitorList for glfwGetMonitors */
    int monitorCount;
    _GLFWcursor  *cursorList;

    /* Hints given before glfwInit */
    int requestedPlatform;        /* GLFW_ANY_PLATFORM / GLFW_PLATFORM_* */
    int resolvedPlatform;         /* GLFW_PLATFORM_* after init */
    bool joystickHatButtons;      /* GLFW_JOYSTICK_HAT_BUTTONS (default true) */

    GLFWjoystickfun joystickCallback;
    GLFWmonitorfun   monitorCallback;

    /* Init-time configuration accepted for GLFW compatibility.
     * The shim always allocates through libc (documented in README). */
    bool allocatorSet;
    GLFWallocator allocator;

    /* Custom Vulkan loader supplied via glfwInitVulkanLoader (stored as a
     * plain address; SDL owns the loader, so this is compatibility only). */
    void *vkLoader;

    _GLFWjoystick joysticks[GLFW_JOYSTICK_LAST + 1];

    /* Time */
    double timeOffset;

    /* Clipboard: GLFW owns the returned string, so keep our own copy. */
    char *clipboard;
    size_t clipboardLen, clipboardCap;

    /* Vulkan */
    bool vkLoaded;
    const char *vkText[8];
    Uint32 vkTextCount;
} _GLFWglobal;

extern _GLFWglobal _glfw;

/* ------------------------------------------------------------------ */
/* shared helpers                                                      */
/* ------------------------------------------------------------------ */

/* error.c */
void _glfwInputError(int error, const char *format, ...) __attribute__((format(printf, 2, 3)));

/* keys.c -- bidirectional key mapping */
void _glfwInitKeyTables(void);
SDL_Scancode _glfwKeyToScan(int key);      /* GLFW token -> SDL scancode            */
int _glfwScanToKey(SDL_Scancode scan);     /* SDL scancode  -> GLFW token           */
int _glfwModsToGLFW(SDL_Keymod mods, bool lockKeyMods);
int _glfwSdlMouseButtonToGLFW(Uint8 b);    /* SDL button -> GLFW button index       */
Uint8 _glfwGLFWMouseButtonToSDL(int b);    /* GLFW button index -> SDL button       */

/* window.c */
_GLFWwindow *_glfwFindWindow(SDL_Window *sdlWindow);
_GLFWwindow *_glfwFindWindowByID(SDL_WindowID id);
void _glfwDestroyWindow(_GLFWwindow *window);
bool _glfwCreateContext(_GLFWwindow *window, _GLFWwindow *share);
void _glfwApplyContextAttributes(_GLFWwindow *window);

/* monitor.c */
void _glfwRefreshMonitors(void);
void _glfwDestroyMonitors(void);
_GLFWmonitor *_glfwFindMonitor(SDL_DisplayID id);
void _glfwUpdateMonitorModes(_GLFWmonitor *monitor);

/* input.c (joystick slot management) */
void _glfwRefreshJoysticks(void);
SDL_JoystickID _glfwJoystickSlotToSDL(int jid);   /* 0 if not present      */
_GLFWjoystick *_glfwSlot(int jid);
void _glfwApplyCursor(_GLFWwindow *window);
/* events.c */
void _glfwPumpEvents(void);
bool _glfwAcceptsEvents(void); /* platform uses poll/wait */

/* C11 thread-local error state lives in error.c */

#endif /* GLFW_SDL_INTERNAL_H */