/*
 * GLFW support layer on top of SDL3 -- input code translation tables.
 *
 * GLFW identifies keys by "key token" (ASCII based for letters/digits, or
 * named values) that are positionally independent of the keyboard layout.
 * SDL3 uses SDL_Scancode (USB HID usage based), which is also positional.
 * So the two map 1:1.  We build both directions from a single table.
 *
 * SDL3 mouse button numbers differ from GLFW's (SDL: left=1... right=3,
 * GLFW: left=0, right=1, middle=2), so those need translation as well.
 *
 * The GLFW modifier bits and SDL_Keymod bits are laid out identically
 * (shift=1, ctrl=2, alt=4, super=8, caps=16, num=32), so mods pass through.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "internal.h"

typedef struct KeyMap
{
    int key;          /* GLFW key token (from glfw3.h) */
    SDL_Scancode scan;/* SDL scancode */
} KeyMap;

static const KeyMap sKeyMap[] =
{
    /* Letters: GLFW_KEY_A(=65) .. GLFW_KEY_Z(=90), SDL_SCANCODE_A(=4).. */
    { GLFW_KEY_A, SDL_SCANCODE_A }, { GLFW_KEY_B, SDL_SCANCODE_B },
    { GLFW_KEY_C, SDL_SCANCODE_C }, { GLFW_KEY_D, SDL_SCANCODE_D },
    { GLFW_KEY_E, SDL_SCANCODE_E }, { GLFW_KEY_F, SDL_SCANCODE_F },
    { GLFW_KEY_G, SDL_SCANCODE_G }, { GLFW_KEY_H, SDL_SCANCODE_H },
    { GLFW_KEY_I, SDL_SCANCODE_I }, { GLFW_KEY_J, SDL_SCANCODE_J },
    { GLFW_KEY_K, SDL_SCANCODE_K }, { GLFW_KEY_L, SDL_SCANCODE_L },
    { GLFW_KEY_M, SDL_SCANCODE_M }, { GLFW_KEY_N, SDL_SCANCODE_N },
    { GLFW_KEY_O, SDL_SCANCODE_O }, { GLFW_KEY_P, SDL_SCANCODE_P },
    { GLFW_KEY_Q, SDL_SCANCODE_Q }, { GLFW_KEY_R, SDL_SCANCODE_R },
    { GLFW_KEY_S, SDL_SCANCODE_S }, { GLFW_KEY_T, SDL_SCANCODE_T },
    { GLFW_KEY_U, SDL_SCANCODE_U }, { GLFW_KEY_V, SDL_SCANCODE_V },
    { GLFW_KEY_W, SDL_SCANCODE_W }, { GLFW_KEY_X, SDL_SCANCODE_X },
    { GLFW_KEY_Y, SDL_SCANCODE_Y }, { GLFW_KEY_Z, SDL_SCANCODE_Z },

    /* Digits */
    { GLFW_KEY_0, SDL_SCANCODE_0 }, { GLFW_KEY_1, SDL_SCANCODE_1 },
    { GLFW_KEY_2, SDL_SCANCODE_2 }, { GLFW_KEY_3, SDL_SCANCODE_3 },
    { GLFW_KEY_4, SDL_SCANCODE_4 }, { GLFW_KEY_5, SDL_SCANCODE_5 },
    { GLFW_KEY_6, SDL_SCANCODE_6 }, { GLFW_KEY_7, SDL_SCANCODE_7 },
    { GLFW_KEY_8, SDL_SCANCODE_8 }, { GLFW_KEY_9, SDL_SCANCODE_9 },

    /* Punctuation */
    { GLFW_KEY_SPACE,         SDL_SCANCODE_SPACE },
    { GLFW_KEY_APOSTROPHE,    SDL_SCANCODE_APOSTROPHE },
    { GLFW_KEY_COMMA,         SDL_SCANCODE_COMMA },
    { GLFW_KEY_MINUS,         SDL_SCANCODE_MINUS },
    { GLFW_KEY_PERIOD,        SDL_SCANCODE_PERIOD },
    { GLFW_KEY_SLASH,         SDL_SCANCODE_SLASH },
    { GLFW_KEY_SEMICOLON,     SDL_SCANCODE_SEMICOLON },
    { GLFW_KEY_EQUAL,         SDL_SCANCODE_EQUALS },
    { GLFW_KEY_LEFT_BRACKET,  SDL_SCANCODE_LEFTBRACKET },
    { GLFW_KEY_BACKSLASH,     SDL_SCANCODE_BACKSLASH },
    { GLFW_KEY_RIGHT_BRACKET, SDL_SCANCODE_RIGHTBRACKET },
    { GLFW_KEY_GRAVE_ACCENT,  SDL_SCANCODE_GRAVE },

    /* International */
    { GLFW_KEY_WORLD_1, SDL_SCANCODE_INTERNATIONAL1 },
    { GLFW_KEY_WORLD_2, SDL_SCANCODE_INTERNATIONAL2 },

    /* Navigation / editing */
    { GLFW_KEY_ESCAPE,    SDL_SCANCODE_ESCAPE },
    { GLFW_KEY_ENTER,     SDL_SCANCODE_RETURN },
    { GLFW_KEY_TAB,       SDL_SCANCODE_TAB },
    { GLFW_KEY_BACKSPACE, SDL_SCANCODE_BACKSPACE },
    { GLFW_KEY_INSERT,    SDL_SCANCODE_INSERT },
    { GLFW_KEY_DELETE,    SDL_SCANCODE_DELETE },
    { GLFW_KEY_RIGHT,     SDL_SCANCODE_RIGHT },
    { GLFW_KEY_LEFT,      SDL_SCANCODE_LEFT },
    { GLFW_KEY_DOWN,      SDL_SCANCODE_DOWN },
    { GLFW_KEY_UP,        SDL_SCANCODE_UP },
    { GLFW_KEY_PAGE_UP,   SDL_SCANCODE_PAGEUP },
    { GLFW_KEY_PAGE_DOWN, SDL_SCANCODE_PAGEDOWN },
    { GLFW_KEY_HOME,      SDL_SCANCODE_HOME },
    { GLFW_KEY_END,       SDL_SCANCODE_END },

    /* Lock keys */
    { GLFW_KEY_CAPS_LOCK,    SDL_SCANCODE_CAPSLOCK },
    { GLFW_KEY_SCROLL_LOCK,  SDL_SCANCODE_SCROLLLOCK },
    { GLFW_KEY_NUM_LOCK,     SDL_SCANCODE_NUMLOCKCLEAR },
    { GLFW_KEY_PRINT_SCREEN, SDL_SCANCODE_PRINTSCREEN },
    { GLFW_KEY_PAUSE,        SDL_SCANCODE_PAUSE },

    /* Function keys (GLFW has F25, SDL stops at F24: alias it) */
    { GLFW_KEY_F1,  SDL_SCANCODE_F1  }, { GLFW_KEY_F2,  SDL_SCANCODE_F2  },
    { GLFW_KEY_F3,  SDL_SCANCODE_F3  }, { GLFW_KEY_F4,  SDL_SCANCODE_F4  },
    { GLFW_KEY_F5,  SDL_SCANCODE_F5  }, { GLFW_KEY_F6,  SDL_SCANCODE_F6  },
    { GLFW_KEY_F7,  SDL_SCANCODE_F7  }, { GLFW_KEY_F8,  SDL_SCANCODE_F8  },
    { GLFW_KEY_F9,  SDL_SCANCODE_F9  }, { GLFW_KEY_F10, SDL_SCANCODE_F10 },
    { GLFW_KEY_F11, SDL_SCANCODE_F11 }, { GLFW_KEY_F12, SDL_SCANCODE_F12 },
    { GLFW_KEY_F13, SDL_SCANCODE_F13 }, { GLFW_KEY_F14, SDL_SCANCODE_F14 },
    { GLFW_KEY_F15, SDL_SCANCODE_F15 }, { GLFW_KEY_F16, SDL_SCANCODE_F16 },
    { GLFW_KEY_F17, SDL_SCANCODE_F17 }, { GLFW_KEY_F18, SDL_SCANCODE_F18 },
    { GLFW_KEY_F19, SDL_SCANCODE_F19 }, { GLFW_KEY_F20, SDL_SCANCODE_F20 },
    { GLFW_KEY_F21, SDL_SCANCODE_F21 }, { GLFW_KEY_F22, SDL_SCANCODE_F22 },
    { GLFW_KEY_F23, SDL_SCANCODE_F23 }, { GLFW_KEY_F24, SDL_SCANCODE_F24 },
    { GLFW_KEY_F25, SDL_SCANCODE_F24 }, /* no SDL equivalent */

    /* Keypad */
    { GLFW_KEY_KP_0, SDL_SCANCODE_KP_0 }, { GLFW_KEY_KP_1, SDL_SCANCODE_KP_1 },
    { GLFW_KEY_KP_2, SDL_SCANCODE_KP_2 }, { GLFW_KEY_KP_3, SDL_SCANCODE_KP_3 },
    { GLFW_KEY_KP_4, SDL_SCANCODE_KP_4 }, { GLFW_KEY_KP_5, SDL_SCANCODE_KP_5 },
    { GLFW_KEY_KP_6, SDL_SCANCODE_KP_6 }, { GLFW_KEY_KP_7, SDL_SCANCODE_KP_7 },
    { GLFW_KEY_KP_8, SDL_SCANCODE_KP_8 }, { GLFW_KEY_KP_9, SDL_SCANCODE_KP_9 },
    { GLFW_KEY_KP_DECIMAL, SDL_SCANCODE_KP_PERIOD },
    { GLFW_KEY_KP_DIVIDE,  SDL_SCANCODE_KP_DIVIDE },
    { GLFW_KEY_KP_MULTIPLY,SDL_SCANCODE_KP_MULTIPLY },
    { GLFW_KEY_KP_SUBTRACT,SDL_SCANCODE_KP_MINUS },
    { GLFW_KEY_KP_ADD,     SDL_SCANCODE_KP_PLUS },
    { GLFW_KEY_KP_ENTER,   SDL_SCANCODE_KP_ENTER },
    { GLFW_KEY_KP_EQUAL,   SDL_SCANCODE_KP_EQUALS },

    /* Modifier keys */
    { GLFW_KEY_LEFT_SHIFT,   SDL_SCANCODE_LSHIFT },
    { GLFW_KEY_LEFT_CONTROL, SDL_SCANCODE_LCTRL },
    { GLFW_KEY_LEFT_ALT,     SDL_SCANCODE_LALT },
    { GLFW_KEY_LEFT_SUPER,   SDL_SCANCODE_LGUI },
    { GLFW_KEY_RIGHT_SHIFT,  SDL_SCANCODE_RSHIFT },
    { GLFW_KEY_RIGHT_CONTROL,SDL_SCANCODE_RCTRL },
    { GLFW_KEY_RIGHT_ALT,    SDL_SCANCODE_RALT },
    { GLFW_KEY_RIGHT_SUPER,  SDL_SCANCODE_RGUI },
    { GLFW_KEY_MENU,         SDL_SCANCODE_MENU },
};

#define KEYMAP_COUNT (sizeof(sKeyMap) / sizeof(sKeyMap[0]))

/* Runtime lookup arrays, filled by _glfwInitKeyTables(). */
static SDL_Scancode sKeyToScan[GLFW_KEY_LAST + 1];
static int  sScanToKey[SDL_SCANCODE_COUNT];

void _glfwInitKeyTables(void)
{
    size_t i;

    for (i = 0; i < GLFW_KEY_LAST + 1; i++)
        sKeyToScan[i] = SDL_SCANCODE_UNKNOWN;
    for (i = 0; i < SDL_SCANCODE_COUNT; i++)
        sScanToKey[i] = GLFW_KEY_UNKNOWN;

    for (i = 0; i < KEYMAP_COUNT; i++)
    {
        const KeyMap *m = &sKeyMap[i];
        if (m->key >= 0 && m->key <= GLFW_KEY_LAST &&
            m->scan >= 0 && m->scan < SDL_SCANCODE_COUNT)
        {
            sKeyToScan[m->key] = m->scan;
            sScanToKey[m->scan] = m->key;
        }
    }
}

SDL_Scancode _glfwKeyToScan(int key)
{
    if (key < 0 || key > GLFW_KEY_LAST)
        return SDL_SCANCODE_UNKNOWN;
    return sKeyToScan[key];
}

int _glfwScanToKey(SDL_Scancode scan)
{
    if (scan < 0 || scan >= SDL_SCANCODE_COUNT)
        return GLFW_KEY_UNKNOWN;
    return sScanToKey[scan];
}

/* Modifier bits have identical bit-layout between GLFW and SDL3.
 * With GLFW_LOCK_KEY_MODS enabled the Caps Lock / Num Lock bits are
 * reported; otherwise GLFW strips them from callback "mods" values. */
int _glfwModsToGLFW(SDL_Keymod mods, bool lockKeyMods)
{
    const int mask = GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER;
    int result = (int)(mods & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI));
    if (lockKeyMods)
        result |= (int)(mods & (SDL_KMOD_CAPS | SDL_KMOD_NUM));
    (void) mask;
    return result;
}

/* SDL button ids: 1=left, 2=middle, 3=right, 4=x1, 5=x2, ... */
int _glfwSdlMouseButtonToGLFW(Uint8 b)
{
    switch (b)
    {
        case SDL_BUTTON_LEFT:   return GLFW_MOUSE_BUTTON_LEFT;
        case SDL_BUTTON_RIGHT:  return GLFW_MOUSE_BUTTON_RIGHT;
        case SDL_BUTTON_MIDDLE: return GLFW_MOUSE_BUTTON_MIDDLE;
        default:
            /* SDL button 4..8 -> GLFW 3..7 */
            if (b >= 4 && b <= 8)
                return (int)(b - 1);
            return -1;
    }
}

Uint8 _glfwGLFWMouseButtonToSDL(int button)
{
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
        return 0;
    return (Uint8)(button + 1);
}