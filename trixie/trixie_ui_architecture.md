# Trixie UI Architecture Guide
*A note to the next LLM (and human) reading this: this document was built from a real conversation about the codebase. Read it top to bottom — each concept builds on the previous one.*

---

## The Philosophy: Blender-style, Roll Your Own

Trixie takes the same approach Blender took: **no third-party widget toolkit**. No Qt, no Dear ImGui, no web view. The team owns and maintains their own UI system built on a vendored fork of NanoVG (a lightweight vector graphics library running on OpenGL).

This means more work upfront, but total control — including the ability to extend NanoVG itself (e.g. adding blur effects) since the source is in-tree.

The design philosophy borrows directly from Blender's internals:

- **Drawing is immediate** — nothing is "dirty", nothing is cached in a tree. Every frame, everything redraws from scratch.
- **Layout is arithmetic** — screen space is subdivided by pure math every frame. No layout engine, no flexbox, no heap allocations.
- **State is retained only where it must be** — transient drag/hover state lives in structs, not in a widget tree.

---

## Core Primitive: The Box

```cpp
struct Box { float x, y, w, h; };
```

A `Box` is just a rectangle. It has no behavior, no parent pointer, no children. It is the atom of the layout system.

### Carving

"Carving" is the act of subdividing a `Box` by slicing pieces off its edges. The project already has this in `box.h`:

```cpp
Box box_split_top(Box* b, float size);     // cuts from the top
Box box_split_bottom(Box* b, float size);  // cuts from the bottom
Box box_split_left(Box* b, float size);    // cuts from the left
Box box_split_right(Box* b, float size);   // cuts from the right
```

Each call **mutates the source box** (shrinking it) and **returns the carved piece**. You start with a box representing the whole screen and carve it up into regions:

```cpp
Box screen      = { 0, 0, win_width, win_height };
Box toolbar     = box_split_top(&screen, 32.0f);   // 32px toolbar
Box roll        = screen;                           // everything remaining
```

This is the equivalent of CSS `display: flex` — except you are the browser engine. The order of cuts determines the layout.

### Cutting a Corner

A corner is just two cuts in sequence:

```cpp
Box top_bar = box_split_top(&screen, 32.0f);
Box corner  = box_split_left(&top_bar, 80.0f); // top-left 80×32 region
// top_bar is now the remainder of the top strip
```

### Absolute Positioning

Not everything needs to be carved. Tooltips, context menus, drag handles — anything that floats above the normal layout — just gets a manually constructed `Box`:

```cpp
Box tooltip = { mouse_x + 12, mouse_y - 8, 120.0f, 24.0f };
```

This is the equivalent of CSS `position: absolute`. Carving and absolute positioning coexist freely.

---

## Panels: Boxes with Cameras

A `Panel` is a `Box` that also owns a `Camera` — scroll position and zoom state:

```cpp
struct Panel {
    float x, y, w, h;   // screen position
    Camera camera;       // scroll_x, scroll_y, zoom, lane_height, pixels_per_beat
};
```

Every independently scrollable surface is a `Panel`. The piano roll canvas is a Panel. The piano strip is (conceptually) a Panel. The mixer rack will be a Panel.

The camera transforms between **world space** (beats, pitches, ticks) and **screen space** (pixels). The coordinate helpers in `piano_roll.cpp` do this:

```cpp
float beat_to_x(float beat, const Camera& cam); // world → screen
float pitch_to_y(int pitch, const Camera& cam);
```

---

## The Widget Model: Composition by Function Call

There is no widget base class, no vtable, no heap-allocated widget tree. The hierarchy is expressed through **the call stack itself**.

A "parent widget" is a function that carves its box and calls child functions with the carved pieces:

```cpp
void draw_piano_roll(NVGcontext* nvg, PianoRollView& view, Song& song) {
    Box canvas_box  = view.box;
    Box hscroll_box = box_split_bottom(&canvas_box, 14.0f);
    Box vscroll_box = box_split_right(&canvas_box, 14.0f);
    Box strip_box   = box_split_left(&canvas_box, PIANO_STRIP_WIDTH);

    draw_roll_canvas(nvg, canvas_box, view, song);
    draw_piano_strip(nvg, strip_box, view.camera);
    draw_scrollbar(nvg, hscroll_box, &view.camera.scroll_x, ...);
    draw_scrollbar(nvg, vscroll_box, &view.camera.scroll_y, ...);
}
```

**The "children" are just local variables and function calls.** No heap. No tree. The parent owns the layout; the children just receive a box and draw into it.

### Where State Lives

Since there is no persistent widget tree, transient state (is this scrollbar dragging? is this note being hovered?) must live in a **named struct that outlives a single frame**.

Rule: **state lives in the nearest named ancestor that survives across frames.**

```cpp
struct PianoRollView {
    Panel  panel;
    Camera camera;

    // canvas interaction state
    bool lmb_placing        = false;
    bool lmb_dragging_note  = false;
    bool mmb_held           = false;
    int  drag_track         = -1;
    int  drag_note_idx      = -1;

    // scrollbar drag state
    bool hscroll_dragging   = false;
    bool vscroll_dragging   = false;

    // child views
    AutomationView automation;
    bool automation_open    = false;
    float automation_ratio  = 0.25f;
};
```

The nested `AutomationView` struct *is* the child widget. It has no parent pointer. It just holds its own state. You call its functions with the box you carved for it.

---

## Z-Order: Draw Order Is Render Order

NanoVG composites in the order you draw. There is no z-index property. To draw something on top, draw it after everything else.

```cpp
// back to front
draw_piano_roll(nvg, view, song);
draw_parameter_editor(nvg, param_editor);  // visually on top
draw_overlays(nvg);                         // tooltips, ghosts, cursors — always last
```

For the ghost note preview and playback cursor, the codebase already does this correctly — they are drawn after `draw_piano_roll` returns.

---

## Event Handling Pipeline

### The Problem: Draw Order ≠ Input Order (by default)

Just because something is drawn on top doesn't mean it automatically receives input first. You have to be explicit about input priority — but the rule is simple:

**Input is dispatched in reverse draw order. Whatever is visually on top gets first dibs.**

If it consumes the event, nothing below it sees it.

### The `event_consumed` Pattern

For simple cases (one overlay, one base panel):

```cpp
bool event_consumed = false;

if (!event_consumed && param_editor.open && box_contains(param_editor.box, e.x, e.y)) {
    param_editor_handle_event(param_editor, e);
    event_consumed = true;
}

if (!event_consumed && box_contains(piano_roll_panel.box, e.x, e.y)) {
    piano_roll_handle_event(view, e, song, journal);
}
```

### The Layer Stack (Scalable Version)

When you have many overlapping interactive surfaces, hardcoding the priority order becomes fragile. The solution is a **layer stack** — a list of active interactive layers, processed front-to-back for input, back-to-front for drawing:

```cpp
struct Layer {
    Box  box;
    bool modal = false; // if true, blocks all input to layers below
    std::function<void(NVGcontext*)>   draw;
    std::function<bool(InputEvent&)>   handle_event; // returns true = consumed
};

std::vector<Layer> layer_stack;
```

Main loop:

```cpp
// draw: back to front (index 0 is bottom)
for (auto& layer : layer_stack)
    layer.draw(nvg);

// input: front to back (reverse iteration)
for (auto it = layer_stack.rbegin(); it != layer_stack.rend(); ++it) {
    if (it->handle_event(e)) break;   // consumed — stop
    if (it->modal)           break;   // modal — nothing below sees input
}
```

Opening a popup:

```cpp
layer_stack.push_back({
    .box   = compute_popup_box(),
    .modal = true,
    .draw  = [&](NVGcontext* nvg){ draw_param_editor(nvg, param_editor); },
    .handle_event = [&](InputEvent& e) -> bool {
        return param_editor_handle_event(param_editor, e);
    }
});
```

Closing it:

```cpp
layer_stack.pop_back();
```

**Why this matters for reusability:** the piano roll doesn't know popups exist. The popup doesn't know the piano roll exists. The dispatch loop doesn't know what either of them are. Adding a new overlay never requires touching existing code.

### The Modal Case

If a popup should block *all* interaction behind it (a settings dialog, a "are you sure?" prompt), set `modal = true` and the loop stops after processing that layer regardless of whether the event was consumed.

### Input Coordinate Localization

When an event reaches a panel's handler, localize its coordinates to panel-local space at the dispatch boundary:

```cpp
float lx = e.x - panel.x;
float ly = e.y - panel.y;
// everything below works in local coords — the panel doesn't know where it sits on screen
```

This is already done correctly in the existing `render.cpp` for the piano roll.

---

## Reusable Widgets: Scrollbars and Splitters

### Scrollbar

A scrollbar is a `Box` + a pointer to a scroll value it modifies directly:

```cpp
struct Scrollbar {
    Box    box;
    bool   is_vertical;
    float  content_size;  // total scrollable extent in pixels
    float  view_size;     // visible size in pixels
    float* scroll_value;  // points to camera.scroll_x or camera.scroll_y
    bool   dragging    = false;
    float  drag_offset = 0;

    void layout(Box b)              { box = b; }
    void draw(NVGcontext* nvg);
    bool handle_event(InputEvent& e); // returns true if consumed
};
```

Pass it `&camera.scroll_x` and it owns itself. No callbacks. The camera is updated directly.

### Splitter (Resizable Divider)

A splitter is a thin interactive `Box` between two regions that modifies a ratio:

```cpp
struct Splitter {
    Box    box;
    bool   is_vertical;  // true = vertical line (left/right split)
    float* ratio;        // 0..1, drives box_split_left/right in the layout pass
    bool   dragging = false;

    void layout(Box b)              { box = b; }
    bool handle_event(InputEvent& e);
    void draw(NVGcontext* nvg);     // thin highlight line, cursor change
};
```

In the layout pass, use `*ratio` to compute the split point. Dragging just modifies `*ratio`. Because layout reruns every frame, the panels snap to the new ratio automatically — no invalidation needed.

---

## Overlay Queue

For things that must always draw on top of everything (tooltips, drag ghosts, context menus), maintain an overlay queue flushed at the end of each frame:

```cpp
std::vector<std::function<void(NVGcontext*)>> overlays;

// during the frame, any code can enqueue an overlay:
overlays.push_back([=](NVGcontext* nvg) {
    draw_tooltip(nvg, tooltip_box, "Note: C4");
});

// at the end of the frame, after all panels draw:
for (auto& fn : overlays) fn(nvg);
overlays.clear();
```

The ghost note preview (`piano_roll_draw_ghost`) is already doing this pattern informally. The queue just formalizes it so any subsystem can participate without knowing about the others.

---

## Transparency in NanoVG

Alpha is a first-class parameter in every color:

```cpp
nvgFillColor(nvg, nvgRGBAf(0.05f, 0.05f, 0.08f, 0.75f)); // 75% opaque
```

Since NanoVG composites in draw order against the framebuffer, transparent widgets naturally show whatever was drawn before them. Draw order is z-order is compositing order — all the same thing.

For a frosted/blurred background (macOS-style popups), NanoVG alone cannot do it — it has no access to what's already in the framebuffer. Since Trixie vendors NanoVG, this is theoretically extensible (blit framebuffer → blur texture → draw as background), but for a DAW, a dark semi-transparent fill is almost always sufficient and costs nothing.

---

## The CSS Translation Table

For those coming from web/CSS backgrounds:

| HTML/CSS concept       | Trixie equivalent                              |
|------------------------|------------------------------------------------|
| `<div>` nesting        | function call hierarchy                        |
| `display: flex`        | `box_split_*` calls                            |
| `overflow: scroll`     | `Panel` + `Camera`                             |
| `position: absolute`   | manually constructed `Box`, drawn out-of-order |
| `z-index`              | draw order (later = higher)                    |
| DOM tree               | struct nesting (`PianoRollView` owns `AutomationView`) |
| Event bubbling         | manual `box_contains` + `event_consumed` flag  |
| `pointer-events: none` | simply don't add it to the layer stack         |
| Modal overlay          | `Layer` with `modal = true` in the layer stack |

---

## What Currently Exists vs. What Still Needs Building

### Already solid — don't touch
- `box.h` — the split functions are correct and complete
- `panel.h` — the right abstraction for a scrollable surface  
- `piano_roll.cpp` draw functions — clean, well-separated passes
- The per-frame layout recomputation pattern

### Refactor soon
- Pull the loose input state variables out of `render_thread_run` into a `PianoRollView` struct
- `PIANO_STRIP_WIDTH` is currently baked into coordinate math — the piano strip should become its own sub-panel with its own clip region and scissor rect

### Build next
- `Scrollbar` widget (reusable, takes `float*`)
- `Splitter` widget (reusable, takes `float* ratio`)
- `Layer` stack + dispatch loop (replaces hardcoded input priority)
- Overlay queue (formalizes the ghost note pattern for all subsystems)

---

---

## UI Scaling

Scaling support is much easier to bake in early than to retrofit later. The whole system flows from one number.

### The Core Concept: One Scale Factor

A single `ui_scale` float drives everything. `1.0` on a normal monitor, `2.0` on a 4K display at 200% OS scaling, `0.8` if the user wants a denser UI. Every size constant gets multiplied by it:

```cpp
float toolbar_height = 32.0f * ui_scale;
float lane_height    = 14.0f * ui_scale;
float handle_width   = 8.0f  * ui_scale;
```

### Two Separate Concerns

GLFW already gives you `pixel_ratio` — the render thread computes it every frame:

```cpp
glfwGetFramebufferSize(window.handle, &fb_width, &fb_height);  // physical pixels
glfwGetWindowSize(window.handle, &win_width, &win_height);     // logical pixels
float pixel_ratio = (float)fb_width / (float)win_width;        // HiDPI scale
```

This is passed to `nvgBeginFrame` and handles rendering sharpness on HiDPI displays. But it's not the same as UI scale:

```
pixel_ratio  — rendering sharpness       (already handled)
ui_scale     — how big UI elements are   (needs to be added)
```

You want both, and `ui_scale` should be user-controllable independently of the OS setting.

### UIContext: Clean Propagation

Rather than threading `ui_scale` into every function signature, put it in a small context struct:

```cpp
struct UIContext {
    float scale       = 1.0f;
    float pixel_ratio = 1.0f;

    // wrap any design-space constant with this
    float px(float logical_size) const {
        return logical_size * scale;
    }
};
```

Layout then reads naturally:

```cpp
Box toolbar = box_split_top(&screen, ui.px(32.0f));
Box strip   = box_split_left(&canvas, ui.px(PIANO_STRIP_WIDTH));
```

Draw code too:

```cpp
nvgFontSize(nvg, ui.px(12.0f));
nvgStrokeWidth(nvg, ui.px(1.0f));
nvgRoundedRect(nvg, x, y, w, h, ui.px(4.0f));
```

`ui.px()` is your explicit marker that a value is a design-space constant that should scale. It also makes auditing easy — grep for raw float literals that should have been wrapped.

### What Scales vs. What Doesn't

Not everything should go through `ui.px()`. There are two distinct categories:

**Design-space sizes — scale these.** Font sizes, padding, border widths, handle sizes, toolbar height, corner radii. These are authored in logical pixels and should grow/shrink with UI scale.

**Data-space sizes — don't scale these.** `pixels_per_beat`, `lane_height` in the camera, zoom levels. These are values the *user* controls independently via scroll/zoom gestures. A user can zoom the piano roll vertically without wanting the toolbar to get bigger. The camera already separates these correctly — just stay deliberate about it as new widgets are added.

### Clamping

On very small scale values things can become illegible. Clamp early:

```cpp
ui_scale = std::clamp(ui_scale, 0.5f, 3.0f);
```

Note: the existing `HANDLE_WIDTH = 8.0f` constant in `piano_roll.cpp` is a design-space size — it should eventually become `ui.px(8.0f)`. There's already a comment in the code noting that tiny notes can't be resized; on a HiDPI screen at raw pixels this gets worse.

### Responding to Monitor Changes at Runtime

If the user drags the window to a different monitor, `pixel_ratio` can change. GLFW has a callback:

```cpp
glfwSetWindowContentScaleCallback(window.handle, [](GLFWwindow*, float x, float y) {
    // x and y are usually equal — use x
    // update ui.pixel_ratio or ui.scale accordingly
});
```

Because layout reruns every frame from scratch, no invalidation is needed. The new scale is picked up automatically on the next frame — a quiet win of the immediate-mode layout approach.

### Practical Migration Path

There are raw float literals scattered through the codebase right now (`32.0f`, `60.0f`, `8.0f`). Don't fix them all at once. The low-friction path:

1. Add `UIContext` with `px()` and hardcode `scale = 1.0f`
2. Wrap constants in `ui.px()` as you touch code for other reasons
3. When ready to test, change the hardcoded value and see what breaks
4. Wire to a user preference last, once the structure is solid

---

## Icons

Not yet needed, but worth planning for. Two approaches suit the codebase:

### Icon Font (TTF)

NanoVG already renders fonts via stb_truetype. An icon font is a TTF where each glyph is an icon mapped to a Unicode private-use codepoint. Loading is identical to the existing UI font:

```cpp
nvgCreateFont(nvg, "icons", "assets/fonts/trixie-icons.ttf");

nvgFontFace(nvg, "icons");
nvgFontSize(nvg, ui.px(18.0f));
nvgFillColor(nvg, nvgRGBf(1, 1, 1));
nvgText(nvg, x, y, "\xEE\x80\x80", nullptr); // U+E000 = your "play" icon
```

**Pros:** scales perfectly, trivially recolorable, one draw call, batches with text rendering, monochrome is fine for toolbar icons.

**Cons:** monochrome only. Authoring requires SVG → IcoMoon/Fontello → TTF pipeline.

**The centering problem:** font metrics center on the full line height (ascender to descender), not the visual ink. For icons this causes misalignment. Fix by measuring the actual glyph bounds and offsetting:

```cpp
float bounds[4];
nvgTextBounds(nvg, 0, 0, "\xEE\x80\x80", nullptr, bounds);
float glyph_cx = (bounds[0] + bounds[2]) * 0.5f;
float glyph_cy = (bounds[1] + bounds[3]) * 0.5f;
nvgText(nvg, cx - glyph_cx, cy - glyph_cy, "\xEE\x80\x80", nullptr);
```

Do this once per glyph at startup, cache the offsets, apply at draw time.

### Texture Atlas (Bitmap Sprites)

A PNG sprite sheet loaded as an NanoVG image, drawn with `nvgImagePattern`. Good for multicolor or illustrative icons. Doesn't scale cleanly unless authored at @2x or higher.

The team has discussed building a small internal tool to compile a folder of PNGs into a properly arranged and mipmapped atlas — a straightforward project that would also serve the piano strip skinning idea noted in `piano_roll.cpp`.

**Best fit:** icon font for UI chrome (toolbar buttons, handles, mode indicators), texture atlas for illustrative content (instrument category art, skin elements).

**For now:** placeholder colored rectangles are fine. Add icon infrastructure when toolbar buttons are actually being built and the layout system is stable.

---

*This document reflects the state of the codebase as of the initial architecture conversation. When in doubt: keep it flat, keep it explicit, recompute every frame.*
