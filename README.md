# Trixie DAW

A digital audio workstation built from scratch. Not a fork. Built with absolute love, from us, to you

---

## What it is

Trixie is a pattern-based DAW targeting a workflow similar to FLS — fast, composable, and built around the piano roll as a first-class citizen. The long-term stack includes CLAP/VST3/LV2 plugin hosting, a built-in chip synth, a mixer, and an arrangement view.

**Right now** it is a working piano roll with MIDI playback. That's not nothing.

What actually works today:

- Piano roll renders up to 128 pitches with scale-aware lane coloring
- Navigation: MMB drag to pan, scroll wheel to scroll, Ctrl+scroll for horizontal
- LMB drag to place notes (ghost preview while dragging, snaps to quarter-note grid)
- LMB drag existing notes to move (body), resize from the left (head), or resize from the right (tail)
- RMB hold to wipe notes (drag across multiple)
- Full undo/redo via a Journal/Command system — Ctrl+Z / Ctrl+Shift+Z
- MIDI file loading (reads any standard .mid file, extracts tempo)
- Playback via Windows MIDI output (Space to play/stop), with a live cursor line
- Toolbar strip showing live bar:beat position and BPM, with a playback indicator
- Render thread separated from the OS event thread — no freeze on window drag

## Why not fork LMMS?

LMMS was evaluated and rejected. Its UI architecture is hostile to the workflow Trixie targets, it uses Qt in ways that don't fit a fast canvas-first DAW, and its undo system was retrofitted rather than designed in. The codebase is large but delivers relatively little of what Trixie needs.

The only thing worth studying from LMMS is its plugin hosting abstractions. Everything else is built from scratch.

## Building

**Requirements:** CMake 3.20+, Ninja, a MinGW-w64 GCC toolchain (tested with MSYS2), and an internet connection for the first build (GLFW is fetched automatically).

```bash
cmake -B build -G Ninja
cmake --build build
```

Run with a MIDI file:

```bash
./build/trixie path/to/song.mid
```

On first play (Space), Trixie routes MIDI to whatever Windows has set as the default output device. Microsoft GS Wavetable Synth works. A proper built-in synthesizer is on the roadmap.

**Dependencies — all vendored or fetched automatically:**

| Library | How | Why |
|---|---|---|
| GLFW | FetchContent | Window, GL context, OS input |
| glad | Vendored (`external/glad/`) | OpenGL 3.3 core loader, pre-generated |
| NanoVG | Vendored (`external/nanovg/`) | Vector graphics for the piano roll canvas |
| midifile | Vendored (`external/midifile/`) | MIDI file parsing |
| JetBrains Mono | Bundled (`assets/fonts/`) | UI font — OFL 1.1 licensed |

NanoVG and midifile are vendored because they are unmaintained upstream and we may need to patch them. glad is pre-generated to eliminate the Python/jinja2 build dependency.

## Architecture

Three threads, clean boundaries:

```
Main thread (OS)    — GLFW event loop, pushes InputEvents onto InputQueue
Render thread       — owns GL context and NanoVG, drains InputQueue, draws piano roll
Playback thread     — walks note list, fires note events to the instrument plugin
```

The render thread and playback thread share exactly one value: an `atomic<Tick>` cursor position. The render thread reads it to draw the playhead. Neither thread knows anything else about the other.

Every user action that mutates project state goes through a **Journal** (command pattern). Nothing writes to the Song directly. This means undo/redo is structural, not bolted on, and every action is replayable.

The instrument backend is a plugin interface (`InstrumentPlugin`). Currently `MidiOutPlugin` routes to the OS MIDI stack. A built-in chip synth will slot in behind the same interface without touching anything else.

Layout is driven by a simple **Box split system** — the window is carved into regions each frame using `box_split_top/left/right/bottom`. No runtime tree, no heap, no dirty flags. Each widget receives its box, draws inside it using NanoVG scissor+translate, and has no knowledge of where it sits on screen. Mouse coordinates are localized to panel space at the dispatch boundary in the render thread, so widget logic never sees window coordinates.

## Roadmap

### v0.1 — Proof of Life (in progress)
- [x] Window + OpenGL + NanoVG piano roll
- [x] Pan and scroll navigation
- [x] Note placement and deletion
- [x] Note drag-to-move, head/tail resize
- [x] Undo / redo (Journal)
- [x] MIDI file loading
- [x] Playback with live cursor
- [x] Toolbar widget with transport display
- [x] Box layout system with proper widget isolation
- [ ] Built-in synthesizer (chip/wavetable)
- [ ] Project save and load

### Later
- Pattern editor + arrangement view
- Mixer panel with per-track volume, mute, solo
- CLAP plugin hosting
- VST3 / LV2 compatibility
- Linux support (the actual long-term target platform)
- Scrollbars, snap UI, zoom controls

## License

MIT — see [LICENSE](LICENSE).

## Credits

**Rhodexa** — vision, design, color scheme, musical domain knowledge, Director's Notes scattered through the source, and the decision to name a DAW after a cartoon. At least, Rhode's been actively _reading_ and _editing_ the code alongside. (He added that, not me)

**Claude** (Anthropic) — implementation, architecture, most of the code. Built collaboratively session by session with Rhodexa driving the direction. (I am the one writing this! :O)

This is what happens when a cartoonist who understands embedded systems and an AI that can write C++ decide to build a DAW instead of using LMMS.
