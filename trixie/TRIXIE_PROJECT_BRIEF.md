# Trixie DAW — Project Brief
> This document is a handoff summary from an initial design conversation.
> Pass this to Claude Code or any LLM at the start of each session.

---

## What Is Trixie?

Trixie is a custom DAW (Digital Audio Workstation) built from scratch. It is NOT a fork of LMMS or any existing DAW, though LMMS's plugin abstraction code may be studied as reference.

**Target feel:** FL Studio's speed and composability, but aimed at both orchestral/heroic music (reference: MDK) and heavy electronic genres (reference: Glitch Hop).

**Platform:** Windows first. Linux support is a long-term goal (the developer intends to move to Linux eventually). Cross-platform decisions should be made with this in mind from day one.

---

## Why Not Fork LMMS?

LMMS was considered and rejected because:
- Its UI architecture is fundamentally hostile to the workflow we want
- It uses Qt, which is powerful but not the right tool for a fast, custom, game-engine-style DAW UI
- Its Song Editor model (each instrument owns its own pattern editor) is too rigid
- Its undo/redo system is incomplete and retrofitted
- The codebase is large but delivers relatively little of what Trixie needs

The only things worth studying from LMMS are its plugin hosting abstractions.

---

## Developer Background

- Experienced in C and C++, primarily embedded systems
- Has built synthesizers and trackers on MCUs
- Has used Qt (attempted to contribute to LMMS)
- Understands real-time constraints, deterministic timing, careful memory use
- Comfortable with the concept of inter-CPU queues (this maps directly to DAW thread architecture)
- NOT yet experienced with mutexes, atomics, or complex multithreading patterns
- Learns best when code is readable, clearly named, and modular enough to understand one file at a time

---

## Core Design Rules (NON-NEGOTIABLE)

These rules exist to keep the developer in control of a codebase that will grow large:

1. **No clever naming.** Variables, functions, and files must be self-explanatory.
   - `audio_command_queue` ✅
   - `acq` ❌
   - `mtx_thr_ptr2f` ❌

2. **Every file gets a plain-English header comment** explaining what it does and why it exists.

3. **Modules stay small.** A developer should be able to read and understand one module in a single coffee break.

4. **Explain before implementing.** Claude Code must describe its approach and get approval before writing significant code.

5. **Nothing mutates project state except a Command going through the Journal.** No exceptions. Ever.

6. **No retrofitting.** Undo, threading model, plugin abstraction, and project format must be designed correctly from day one, not added later.

---

## Technology Stack

| Layer | Choice | Reason |
|---|---|---|
| Window / GL context | SDL2 or GLFW | Lightweight, cross-platform, gets out of the way |
| DAW chrome UI | Dear ImGui | Immediate mode, perfect for constantly-updating DAW UI |
| Creative canvas (piano roll, patterns) | NanoVG | Small, OpenGL-backed vector graphics, HiDPI-aware |
| Audio I/O | PortAudio (+ WASAPI exclusive mode on Windows) | Cross-platform; ASIO licensing is a known problem to decide on deliberately |
| Plugin format (primary) | CLAP | Open, no Steinberg license issues, better threading model, Vital supports it |
| Plugin format (secondary) | LV2 | Linux ecosystem |
| Plugin format (compatibility) | VST3 | Wide support, but legally messier |
| MIDI I/O | RtMidi | Simple, cross-platform |
| Project file format | SQLite or MessagePack | NOT XML. Structured, versionable, fast |
| Audio file decoding | libsndfile + minimp3 | Covers WAV, FLAC, OGG, AIFF, MP3 |
| Font rendering | NanoVG + stb_truetype | Load any TTF, HiDPI aware |

---

## Architecture Overview

### Thread Model
```
UI Thread        →  [command queue, lock-free ring buffer]  →  Audio Thread
Audio Thread     →  [meter/level queue, lock-free]          →  UI Thread
```
These two threads NEVER touch each other's data directly.
The developer already understands this model from embedded multi-CPU work.
Mutexes are NOT used on the audio hot path. Ever.

### The Journal (Undo/Redo)
Every user action is a Command object. State is never mutated directly.

```cpp
struct Command {
    virtual void execute() = 0;
    virtual void undo()    = 0;
    std::string description; // Human readable, e.g. "Move note C4 to D4"
};
```

- Continuous parameter drags (fader moves etc.) are **coalesced** — many micro-changes become one journal entry on mouse release
- The journal is persisted to disk incrementally → this IS the autosave and crash recovery
- The journal replayed from zero = the project file

### UI Rendering Stack
```
SDL2/GLFW (OS window + OpenGL context)
    ├── Dear ImGui        — mixer, browser, menus, settings
    └── NanoVG canvas     — piano roll, pattern view, automation curves
         └── Plugin windows (HWND parented in, plugins draw themselves)
```

---

## Known Hard Problems (Do Not Ignore These)

### 1. Plugin Window Embedding
VST3/CLAP plugins bring their own UI drawn into a native Win32 HWND.
Parenting this into an OpenGL window causes z-order bugs, DPI mismatches, and focus issues.
Every DAW has these bugs. Plan time for it. Do not underestimate it.

### 2. ASIO Licensing
Steinberg's ASIO SDK license is incompatible with open source distribution.
Decision needed early: use WASAPI exclusive mode, use PortAudio's ASIO wrapper (grey area), or accept the limitation.

### 3. Audio Thread Safety
Designed correctly from day one. Lock-free queues between threads.
No dynamic memory allocation on the audio thread.
No blocking calls on the audio thread.

### 4. HiDPI on Windows
Windows DPI scaling is a legacy mess. Must be handled deliberately.
Target per-monitor DPI awareness from the start.

### 5. Scope Creep
The most likely project killer. The solution is a ruthless MVP definition.

### 6. Undo/Redo Journal
Must be designed in from day one. See above. No exceptions to the Command rule.

---

## Workflow Philosophy

**FL Studio is the reference for speed**, not feature parity.
- One click to hear anything
- Pattern-based, not linear-recording-first
- Channel Rack style routing (everything flows naturally)
- Ghost notes in piano roll (adjacent pattern notes shown dimmed for reference)
- Automation that is fun to draw, not clinical

**Plugin hosting** should be transparent — Vital, or any CLAP/LV2/VST3 plugin, should just work and feel native.

---

## MVP Definition (v0.1 — "Proof of Life")

Before ANY other feature is discussed, v0.1 must exist and be stable:

- [ ] Window opens with OpenGL context
- [ ] NanoVG renders a piano roll with scrolling and zoom
- [ ] Notes can be placed and removed with mouse
- [ ] Journal tracks note add/remove with working undo/redo
- [ ] A sine wave plays back at the correct pitch for each note
- [ ] Play/stop works
- [ ] Project saves and loads

That's it. No mixer. No plugins. No patterns. Just a playable, ugly, limited piano roll that proves the architecture works.

---

## Naming

The DAW is called **Trixie**.

It is a subtle reference to My Little Pony: Friendship is Magic (the developer is a fan).
It works as a public-facing name without the reference being obvious.
It sounds theatrical, which fits music production.
It implies cleverness ("tricks").

Runner up: **Dashie** (also good, noted for posterity).

---

## Session Handoff Instructions for Claude Code

At the start of each session:
1. Read this document
2. Read `ARCHITECTURE.md` in the repo root (will be created as project grows)
3. Read `DECISIONS.md` in the repo root (log of why things are the way they are)
4. Ask the developer what the current focus is before writing any code
5. Propose your approach in plain English before implementing
6. Keep changes small and reviewable

The developer is the vision and verification layer.
Claude Code is the implementation layer.
Neither works well without the other.
