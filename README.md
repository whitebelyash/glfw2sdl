# SDL-backed GLFW 3.5 compatibility shim

A from-scratch reimplementation of the [GLFW 3.5](https://www.glfw.org/) public
API on top of [SDL3](https://wiki.libsdl.org/SDL3/), written in C11 for
Linux, macOS and Android (POSIX — no OS-specific code in the shim).

The shim does **not** embed real GLFW source. It vendors the genuine GLFW 3.5.0
public headers (`include/GLFW/glfw3.h`, `glfw3native.h`, zlib licence) so that
games built against real GLFW link unchanged. The implementation itself calls
SDL3 exclusively — every function is a live translation, there is no GLFW code
being compiled.

The goal is a drop-in `libglfw.so` for games such as Minecraft (LWJGL3),
which load `libglfw.so` and expect the real GLFW 3.5 ABI and semantics.

## Building

Requires SDL3 (>= 3.4), CMake >= 3.16 and a C11 compiler.

```sh
cmake -S . -B build
cmake --build build -j
```

Produces `build/libglfw.so` (with `3.5` / `3.5.0` soname symlinks) and a
headless conformance smoke test (`glfw_smoke_test`). SDL3 is found via CMake
config (`SDL3::SDL3`) and falls back to `pkg-config sdl3`. Install with
`cmake --install build`.

To compile against the shim just use the vendored headers:

```sh
cc game.c -Iinclude -Lbuild -lglfw -lSDL3
```

Run the smoke test headless or on a real desktop:

```sh
SDL_VIDEODRIVER=dummy ./build/glfw_smoke_test   # zero failures, exit 0
./build/glfw_smoke_test                         # on X11/Wayland
```

## Design

* Single C11 backend. Mirrors GLFW's module layout
  (`window`, `input`, `monitor`, `context`, `vulkan`, `keyboard/joystick`,
  `error`, `init`, `events`, `native`) with one global state struct `_glfw`
  in `src/internal.h`; a `_GLFWwindow` handle wraps an `SDL_Window`.
* GLFW concepts with no exact SDL3 counterpart are emulated from SDL
  primitives (see [Mapping notes](#mapping-notes)).

## Mapping notes

### Units and naming
* GLFW *scancodes* are the corresponding `SDL_Scancode` values, so
  `glfwGetKeyScancode` / `glfwGetKeyName` / `glfwGetKey` are direct SDL calls.
  Key tokens keep their full GLFW value space; undefined tokens map to
  `SDL_SCANCODE_UNKNOWN` and are translated to/from on the fly.
* Cursor position is tracked virtually from SDL mouse events, so
  `GLFW_CURSOR_DISABLED` (relative motion) and `glfwSetCursorPos` behave
  correctly even where SDL has no raw absolute-set API.
* Gamma ramps are stored and returned by the shim (SDL3 has no gamma API);
  `glfwSetGamma` computes a ramp and applies it to a stored copy. There is no
  hardware ramp application.

### Joysticks and gamepads
* GLFW joystick slots 1–16 (GLFW_JOYSTICK_1..LAST) are remapped onto the
  live ordering of `SDL_GetJoysticks`, so `glfwJoystickPresent` and the
  per-slot queries are consistent with real GLFW on the primary joystick.
* `glfwJoystickIsGamepad` uses SDL's built-in gamepad mapping database
  (`SDL_IsGamepad`); `glfwGetGamepadState` wraps SDL's gamepad state.
* GUIDs are synthesized deterministically from SDL joystick GUIDs.

### Windows and video
* `GLFW_NO_API` windows are created with `SDL_WINDOW_VULKAN` so that
  `glfwCreateWindowSurface` works on Vulkan-capable drivers; if that fails the
  flag is retried without it, so input-only windows still open on drivers
  without Vulkan.
* `glfwGetMonitorPhysicalSize` has no SDL3 equivalent (checked 3.4.16 and
  3.5.0); it is estimated from monitor bounds + content scale at a nominal
  96 DPI (25.4/96 mm per pixel).
* `GLFW_MOUSE_PASSTHROUGH` has no SDL3 equivalent: it is stored on the window
  but not applied (SDL3 has no `SDL_SetWindowMousePassthrough`).
* The shim is POSIX-only (no Windows code paths). `glfwGetPlatform` reports
  `GLFW_PLATFORM_X11` / `GLFW_PLATFORM_WAYLAND` / `GLFW_PLATFORM_COCOA`
  depending on the SDL video driver, or `GLFW_PLATFORM_NULL` headlessly
  (e.g. `SDL_VIDEODRIVER=dummy`). Windows are always created hidden when
  `GLFW_VISIBLE` is false; the dummy driver always reports 1 monitor and
  one default video mode.

### Contexts
* Context hints (`GLFW_CONTEXT_VERSION_*`, `GLFW_OPENGL_*`, `GLFW_CONTEXT_*`)
  are stored and translated to SDL GL attribute requests; `glfwMakeContextCurrent`
  maps to `SDL_GL_MakeCurrent`. SDL attributes only accept GLFW values that
  SDL understands; everything is applied at window creation.
* `glfwGetWindowOpacity` returns a `float` (GLFW 3.5 signature) and is
  stored/returned; there is no compositor round-trip.
* `glfwRawMouseMotionSupported` returns `GLFW_TRUE`; raw motion is requested
  via SDL relative mouse mode where available.
* Clipboard, timers (`SDL_GetPerformanceCounter`), `glfwPostEmptyEvent`
  (injects `SDL_EVENT_USER`) all map 1:1.

### Stubs and limitations
* **Native window-system access** (`glfwGetX11Window`, `glfwGetWaylandWindow`,
  `glfwGetWin32Window`, `glfwGetCocoaWindow`, …) returns NULL / 0 and raises
  `GLFW_PLATFORM_UNAVAILABLE` after the `GLFW_NOT_INITIALIZED` check. Platform
  typedefs in `glfw3native.h` are ABI-compatible stand-ins (pointers / 64-bit
  ints) so every symbol still exports and links. Define all
  `GLFW_EXPOSE_NATIVE_*` macros — every native symbol is exported.
* **IME** (`glfwSetPreeditCallback` etc.) stores the group and callbacks;
  SDL3 exposes no preedit/candidate events, so callbacks never fire.
* **`glfwInitAllocator`** accepts and stores a custom allocator, but libc/SDL
  still do the actual allocation (SDL3 has no allocator hook).
* **`glfwInitVulkanLoader`** is accepted and stored; `glfwGetInstanceProcAddress`
  raises `GLFW_API_UNAVAILABLE` and returns NULL because SDL cannot resolve
  instance-level Vulkan proc addresses. `glfwVulkanSupported` and
  `glfwCreateWindowSurface` go through SDL's Vulkan backend.
* **`glfwSetGamma`** applies only to the stored ramp (see gamma note above).

### Version compatibility
Verified against SDL3 3.4.16 (system) and 3.5.0 (`/home/whbex/src/MojoCopy`).
Every SDL3 symbol the shim calls has identical names in both versions, so no
version-conditional code is required. The shim itself reports GLFW 3.5.0 and
resets hints exactly like real GLFW 3.5.0 (`glfwInit` calls
`glfwDefaultWindowHints()`; confirmed against the vendored reference sources).

## Layout

```
include/GLFW/   real GLFW 3.5.0 public headers (zlib, vendored)
src/            shim implementation, one C file per GLFW module
tests/          headless GLFW-conformance smoke test
CMakeLists.txt  shared lib "glfw" (libglfw.so, SOVERSION 3.5) + smoke test
```

## Licence

Zlib — see [LICENSE.md](LICENSE.md). The vendored GLFW headers retain their
own zlib copyright notice.