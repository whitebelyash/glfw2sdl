/*
 * Coexistence regression test: a foreign SDL3 consumer (Controlify-style)
 * installs a global event filter that drops everything except joystick
 * add/remove on the SAME SDL3 instance the shim uses.  The shim must
 * restore its pass-through filter and keep delivering game input.
 *
 * This exercises the Minecraft/Controlify scenario: controlify loads its
 * own SDL3, but when its calls land on the shared SDL3 instance (library
 * soname merging), its event filter would starve the game's keyboard and
 * mouse.  Requires linking SDL3 directly so the test can impersonate the
 * second consumer on the same instance.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <GLFW/glfw3.h>
#include <SDL3/SDL.h>

#include <stdio.h>

static int failures = 0;
static int keyPressCount = 0;
static int keyCodes[8];
static int keyCodeCount = 0;

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

static void keyCallback(GLFWwindow *window, int key, int scancode,
                        int action, int mods)
{
    (void) window; (void) scancode; (void) mods;
    if (action == GLFW_PRESS)
    {
        keyPressCount++;
        if (keyCodeCount < (int)(sizeof(keyCodes) / sizeof(keyCodes[0])))
            keyCodes[keyCodeCount++] = key;
    }
}

static bool keyPressed(int needle)
{
    for (int i = 0; i < keyCodeCount; i++)
    {
        if (keyCodes[i] == needle)
            return true;
    }
    return false;
}

/* Controlify's SDLControllerManager filters exactly this way. */
static bool SDLCALL foreignDroppingFilter(void *userdata, SDL_Event *event)
{
    (void) userdata;
    return event->type == SDL_EVENT_JOYSTICK_ADDED ||
           event->type == SDL_EVENT_JOYSTICK_REMOVED;
}

static void pushKey(SDL_WindowID windowID, SDL_Keycode keycode,
                    SDL_Scancode scancode)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = windowID;
    event.key.key = keycode;
    event.key.scancode = scancode;
    event.key.down = true;
    event.key.repeat = false;
    if (!SDL_PushEvent(&event))
        printf("SDL_PushEvent failed: %s\n", SDL_GetError());
}

int main(void)
{
    if (!glfwInit())
    {
        printf("glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(64, 64, "coexist", NULL, NULL);
    if (!window)
    {
        printf("glfwCreateWindow failed\n");
        return 1;
    }
    glfwSetKeyCallback(window, keyCallback);

    /* Find the shim's SDL window so pushed events carry its windowID. */
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    if (!windows || count < 1)
    {
        printf("no SDL windows\n");
        return 1;
    }
    SDL_WindowID winID = SDL_GetWindowID(windows[0]);
    SDL_free(windows);

    /* --- Scenario A: foreign filter installed AFTER glfwInit ---------- */
    /* (the realistic order: Controlify initializes after the game
     *  creates its window and starts pumping). */
    glfwGetError(NULL);
    SDL_SetEventFilter(foreignDroppingFilter, NULL); /* Controlify */

    /* Each frame, our pump re-asserts the pass-through filter first, so
     * from the next poll on, keyboard events are no longer dropped. */
    glfwPollEvents();

    pushKey(winID, SDLK_A, SDL_SCANCODE_A);
    glfwPollEvents();
    CHECK(keyPressCount == 1 && keyPressed(GLFW_KEY_A),
          "key event delivered after foreign filter replaced");
    CHECK(glfwGetError(NULL) == GLFW_NO_ERROR,
          "no GLFW error during coexistence");

    /* The restored pass-through filter must hold for subsequent frames. */
    pushKey(winID, SDLK_B, SDL_SCANCODE_B);
    pushKey(winID, SDLK_C, SDL_SCANCODE_C);
    glfwPollEvents();
    CHECK(keyPressCount == 3 &&
          keyPressed(GLFW_KEY_B) && keyPressed(GLFW_KEY_C),
          "later key events still delivered");

    /* A new foreign filter replacement is also re-defeated on the next
     * pump (game keeps working even if Controlify re-asserts). */
    SDL_SetEventFilter(foreignDroppingFilter, NULL);
    glfwPollEvents();
    pushKey(winID, SDLK_D, SDL_SCANCODE_D);
    glfwPollEvents();
    CHECK(keyPressCount == 4 && keyPressed(GLFW_KEY_D),
          "filter re-replaced by foreign consumer is re-defeated");

    /* Joystick/gamepad events are translated for GLFW's joystick API but
     * re-posted so a second SDL consumer still receives them. */
    {
        SDL_Event je;
        SDL_zero(je);
        je.type = SDL_EVENT_JOYSTICK_ADDED;
        je.jdevice.which = 999; /* synthetic id; must not reach SDL state */
        if (SDL_PushEvent(&je))
        {
            glfwPollEvents(); /* shim drains and should re-post it */
            SDL_Event back;
            SDL_zero(back);
            const bool reQueued =
                SDL_PollEvent(&back) && back.type == SDL_EVENT_JOYSTICK_ADDED;
            CHECK(reQueued, "joystick event left on the shared queue");
            if (reQueued)
                SDL_zero(back); /* consumed for the test */
        }
        else
        {
            printf("skip - SDL rejected a synthetic joystick event\n");
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    printf("failures: %d\n", failures);
    return failures ? 1 : 0;
}