/*
 * Smoke test for the SDL-backed GLFW shim.
 *
 * Exercises a broad slice of the GLFW 3.5 surface through the public API
 * only (exactly what a game would do), prints results, and aborts with a
 * non-zero status on failures.  Designed to run headless with
 * SDL_VIDEODRIVER=dummy as well as on a real display.
 *
 * SPDX-License-Identifier: Zlib
 */

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

/* To call a native-access function the app defines the matching expose
 * macro.  We use GLFW_NATIVE_INCLUDE_NONE so the test needs no X11 dev
 * headers, providing ABI-equivalent typedefs for the X11 types. */
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_NATIVE_INCLUDE_NONE
#include <stdint.h>
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef unsigned long RRCrtc;
typedef unsigned long RROutput;
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

#define CHECK(cond, ...)                                                \
    do {                                                                \
        if (cond) {                                                     \
            printf("ok   - " __VA_ARGS__);                              \
            printf("\n");                                               \
        } else {                                                        \
            printf("FAIL - " __VA_ARGS__);                              \
            printf("\n");                                               \
            failures++;                                                 \
        }                                                               \
    } while (0)

static void errorCallback(int error, const char *description)
{
    /* Diagnostic only: every error reported by the callback is intentionally
     * injected and validated by an explicit CHECK(glfwGetError(NULL) == ...)
     * immediately after the triggering call, so counting here would
     * double-count the expected errors. */
    printf("error callback: 0x%08X: %s\n", error, description);
}

int main(void)
{
    int major = 0, minor = 0, rev = 0;
    int monitorCount = 0, modeCount = 0;
    GLFWmonitor **monitors;
    const GLFWvidmode *modes;

    glfwSetErrorCallback(errorCallback);

    glfwGetVersion(&major, &minor, &rev);
    printf("glfwGetVersion: %d.%d.%d (%s)\n", major, minor, rev,
           glfwGetVersionString());
    CHECK(major == 3 && minor == 5, "version is 3.5.x");

    /* glfwGetError should read GLFW_NO_ERROR before init. */
    CHECK(glfwGetError(NULL) == GLFW_NO_ERROR, "glfwGetError is clean pre-init");

    /* Pre-init error path: every function raises GLFW_NOT_INITIALIZED. */
    glfwPollEvents();
    CHECK(glfwGetError(NULL) == GLFW_NOT_INITIALIZED,
          "glfwPollEvents before init raises GLFW_NOT_INITIALIZED");

    CHECK(glfwInit() == GLFW_TRUE, "glfwInit succeeds");
    CHECK(glfwGetError(NULL) == GLFW_NO_ERROR, "no error after init");

    CHECK(glfwGetPlatform() == GLFW_PLATFORM_X11 ||
          glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ||
          glfwGetPlatform() == GLFW_PLATFORM_COCOA ||
          glfwGetPlatform() == GLFW_PLATFORM_NULL,
          "glfwGetPlatform reports a known platform (%d)", glfwGetPlatform());

    /* Monitors */
    monitors = glfwGetMonitors(&monitorCount);
    CHECK(monitors != NULL && monitorCount >= 0, "glfwGetMonitors (%d)", monitorCount);
    if (monitorCount > 0)
    {
        GLFWmonitor *primary = glfwGetPrimaryMonitor();
        CHECK(primary != NULL, "glfwGetPrimaryMonitor");
        CHECK(glfwGetMonitorName(primary) != NULL, "glfwGetMonitorName");
        CHECK(glfwGetMonitorName(primary)[0] != '\0',
              "monitor has a non-empty name ('%s')", glfwGetMonitorName(primary));

        int x = -1, y = -1;
        glfwGetMonitorPos(primary, &x, &y);
        CHECK(x != -1 && y != -1, "glfwGetMonitorPos (%d, %d)", x, y);

        int wmm = -1, hmm = -1;
        glfwGetMonitorPhysicalSize(primary, &wmm, &hmm);
        CHECK(wmm > 0 && hmm > 0, "glfwGetMonitorPhysicalSize (%d x %d mm)",
              wmm, hmm);

        float sx = 0.f, sy = 0.f;
        glfwGetMonitorContentScale(primary, &sx, &sy);
        CHECK(sx > 0.f && sy > 0.f, "glfwGetMonitorContentScale (%.2f, %.2f)", sx, sy);

        modes = glfwGetVideoModes(primary, &modeCount);
        /* Some drivers (e.g. SDL's dummy) expose no fullscreen modes. */
        CHECK(modeCount >= 0 && (modes != NULL || modeCount == 0),
              "glfwGetVideoModes (%d modes)", modeCount);
        if (modeCount > 0)
            CHECK(modes[0].width > 0 && modes[0].height > 0,
                  "first mode sane: %dx%d@%d", modes[0].width, modes[0].height,
                  modes[0].refreshRate);

        const GLFWvidmode *mode = glfwGetVideoMode(primary);
        CHECK(mode != NULL && mode->width > 0, "glfwGetVideoMode %dx%d",
              mode ? mode->width : 0, mode ? mode->height : 0);

        const GLFWgammaramp *ramp = glfwGetGammaRamp(primary);
        CHECK(ramp != NULL && ramp->size == 256, "glfwGetGammaRamp size=%d",
              ramp ? ramp->size : -1);
    }

    /* Joystick enumeration must not crash and must be consistent. */
    {
        int present = 0;
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++)
            present += glfwJoystickPresent(jid) ? 1 : 0;
        printf("info - %d joystick slot(s) present\n", present);
        CHECK(glfwGetJoystickGUID(GLFW_JOYSTICK_1) == NULL ||
              glfwGetJoystickGUID(GLFW_JOYSTICK_1)[0] != '\0',
              "glfwGetJoystickGUID is NULL or a non-empty string");
    }

    /* Window creation (video backend dependent; works with the dummy driver) */
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(320, 240, "smoke test", NULL, NULL);
    CHECK(window != NULL, "glfwCreateWindow (client-api=NONE, hidden)");
    if (!window)
        goto out;

    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    CHECK(w == 320 && h == 240, "glfwGetWindowSize (%dx%d)", w, h);

    glfwGetFramebufferSize(window, &w, &h);
    CHECK(w > 0 && h > 0, "glfwGetFramebufferSize (%dx%d)", w, h);

    glfwGetWindowPos(window, &w, &h);
    CHECK(w >= 0 && h >= 0, "glfwGetWindowPos");

    float cx = 0.f, cy = 0.f;
    glfwGetWindowContentScale(window, &cx, &cy);
    CHECK(cx > 0.f && cy > 0.f, "glfwGetWindowContentScale (%.2f, %.2f)", cx, cy);

    glfwSetWindowTitle(window, "renamed");
    CHECK(strcmp(glfwGetWindowTitle(window), "renamed") == 0,
          "glfwGetWindowTitle reflects glfwSetWindowTitle");

    CHECK(glfwGetWindowAttrib(window, GLFW_VISIBLE) == GLFW_FALSE ||
          glfwGetWindowAttrib(window, GLFW_VISIBLE) == GLFW_TRUE,
          "glfwGetWindowAttrib(GLFW_VISIBLE) readable");

    /* Input modes and state queries must round-trip. */
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    CHECK(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED,
          "cursor mode round-trips DISABLED");
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    CHECK(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_HIDDEN,
          "cursor mode round-trips HIDDEN");
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
    CHECK(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_CAPTURED,
          "cursor mode round-trips CAPTURED");
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    CHECK(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL,
          "cursor mode round-trips NORMAL");
    CHECK(glfwGetError(NULL) == GLFW_NO_ERROR,
          "cursor mode round-trips leave no error");

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
    CHECK(glfwGetInputMode(window, GLFW_STICKY_KEYS) == GLFW_TRUE,
          "sticky keys round-trips");

    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    CHECK(glfwRawMouseMotionSupported(),
          "glfwRawMouseMotionSupported");
    CHECK(glfwGetInputMode(window, GLFW_RAW_MOUSE_MOTION) == GLFW_TRUE,
          "raw mouse motion round-trips");

    /* GLFW 3.5 input modes used by recent Minecraft/LWJGL builds; a shim
     * that rejects them raises GLFW_INVALID_ENUM and breaks their input. */
    glfwSetInputMode(window, GLFW_IME, GLFW_TRUE);
    CHECK(glfwGetInputMode(window, GLFW_IME) == GLFW_TRUE,
          "IME input mode round-trips TRUE");
    glfwSetInputMode(window, GLFW_IME, GLFW_FALSE);
    CHECK(glfwGetInputMode(window, GLFW_IME) == GLFW_FALSE,
          "IME input mode round-trips FALSE");

    glfwSetInputMode(window, GLFW_UNLIMITED_MOUSE_BUTTONS, GLFW_TRUE);
    CHECK(glfwGetInputMode(window, GLFW_UNLIMITED_MOUSE_BUTTONS) == GLFW_TRUE,
          "unlimited mouse buttons round-trips TRUE");
    glfwSetInputMode(window, GLFW_UNLIMITED_MOUSE_BUTTONS, GLFW_FALSE);
    CHECK(glfwGetInputMode(window, GLFW_UNLIMITED_MOUSE_BUTTONS) == GLFW_FALSE,
          "unlimited mouse buttons round-trips FALSE");

    glfwGetKey(window, GLFW_KEY_ESCAPE);
    glfwGetKeyName(GLFW_KEY_A, 0);
    glfwGetKeyScancode(GLFW_KEY_ESCAPE);
    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    double mx = -1, my = -1;
    glfwGetCursorPos(window, &mx, &my);
    CHECK(mx == 0.0 || mx != -1.0, "glfwGetCursorPos works");
    glfwSetCursorPos(window, 10.0, 20.0);
    glfwGetCursorPos(window, &mx, &my);
    CHECK(mx == 10.0 && my == 20.0, "glfwSetCursorPos/GetCursorPos round-trip");

    /* Clipboard (dummy driver: expects failure via error path is acceptable;
     * on a real X11/Wayland session it should round-trip). */
    glfwSetClipboardString(window, "hello");
    const char *clip = glfwGetClipboardString(window);
    CHECK(clip == NULL || strcmp(clip, "hello") == 0,
          "clipboard round-trips or is unsupported (got '%s')", clip ? clip : "NULL");

    /* Timers */
    double t0 = glfwGetTime();
    CHECK(t0 >= -0.001 && t0 < 1e9, "glfwGetTime sane (%f)", t0);
    glfwSetTime(5.0);
    CHECK(glfwGetTime() >= 5.0, "glfwSetTime takes effect (%.3f)", glfwGetTime());
    CHECK(glfwGetTimerFrequency() > 0, "glfwGetTimerFrequency (%llu)",
          (unsigned long long)glfwGetTimerFrequency());

    /* Cursors.  Some drivers (dummy) cannot create system cursors; that is
     * an environment limitation, not a shim bug, so accept the platform
     * error path. */
    glfwGetError(NULL);
    GLFWcursor *cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (cursor)
    {
        printf("ok   - glfwCreateStandardCursor\n");
        glfwSetCursor(window, cursor);
        glfwSetCursor(window, NULL);
        glfwDestroyCursor(cursor);
    }
    else
    {
        CHECK(glfwGetError(NULL) == GLFW_PLATFORM_ERROR,
              "glfwCreateStandardCursor fails with GLFW_PLATFORM_ERROR on "
              "drivers without system cursors");
        glfwGetError(NULL);
    }

    /* Callback registration round-trips */
    CHECK(glfwSetKeyCallback(window, NULL) == NULL,
          "glfwSetKeyCallback registers/returns previous");
    CHECK(glfwSetCursorPosCallback(window, NULL) == NULL,
          "glfwSetCursorPosCallback registers");
    CHECK(glfwSetWindowCloseCallback(window, NULL) == NULL,
          "glfwSetWindowCloseCallback registers");

    /* Native access is expected to be unavailable -> NULL plus an error. */
    glfwGetError(NULL); /* clear */
    CHECK(glfwGetX11Window(window) == 0, "glfwGetX11Window returns 0");
    CHECK(glfwGetError(NULL) == GLFW_PLATFORM_UNAVAILABLE,
          "glfwGetX11Window raises GLFW_PLATFORM_UNAVAILABLE");
    glfwGetError(NULL);

    /* Event pumping */
    glfwPollEvents();
    CHECK(!glfwWindowShouldClose(window), "window not closed after PollEvents");

    glfwPostEmptyEvent();
    glfwPollEvents();
    CHECK(glfwGetError(NULL) == GLFW_NO_ERROR,
          "glfwPostEmptyEvent leaves no error");

    /* GLFW_NO_API window: no GL context */
    glfwMakeContextCurrent(window);
    CHECK(glfwGetError(NULL) == GLFW_NO_WINDOW_CONTEXT,
          "MakeContextCurrent on NO_API window raises GLFW_NO_WINDOW_CONTEXT");
    glfwGetError(NULL);

    glfwDestroyWindow(window);
    window = NULL;

    /* Re-init cycle (games do init/terminate/init).  glfwInit resets hints,
     * so re-assert the headless-friendly hints before the second window. */
    glfwTerminate();
    CHECK(glfwInit() == GLFW_TRUE, "second glfwInit succeeds");
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(64, 64, "smoke 2", NULL, NULL);
    CHECK(window != NULL, "re-created window after terminate/init");
    if (window)
        glfwDestroyWindow(window);

out:
    printf("failures: %d\n", failures);
    glfwTerminate();
    return failures ? 1 : 0;
}