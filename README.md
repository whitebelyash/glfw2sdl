# GLFW2SDL

A wrapper implementing GLFW3 on-top of SDL3. Mostly intended for running Minecraft.

GLFW platform support (Wayland, Android) lacks compared to SDL. This wrapper allows GLFW apps to run on-top of SDL3 without any changes on their side bringing some benefits of SDL3 like improved Wayland support.

# AI usage
This project was **fully** generated using a free LLM model. Hence this can (and will) contain a lot of obscure bugs and/or poor decisions.  
Poor code fixes are welcome, though I doubt anyone will even use this, not saying about fixing cringe stuff.

# Status

- [x] Boots Minecraft (1.16.5, 26.1.2)
- [x] Runs Vulkan apps

#### Untested
- [ ] Games other than Minecraft (e.g. Vintage Story)
- [ ] Android/iOS/macOS

#### TODO
- [ ] Windows/win32 support
- [ ] Implement missing stuff
- [ ] Add LICENSE

# Build

```bash
# Assuming you're in the project directory
cmake -B build -S .
cmake --build build
```
Then just point the application to the `libglfw.so` library you just built.
