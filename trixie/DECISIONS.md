# Trixie — Design Decisions

A running log of non-obvious choices: why something is the way it is,
and what was rejected. Add an entry whenever a decision is made that
isn't obvious from the code.

---

## Navigation Model (Piano Roll)

**Date:** 2026-04-28
**Status:** Partially implemented (MMB pan + wheel scroll)

### Canvas interaction

| Input | Action |
|---|---|
| Scroll wheel over roll | Translate camera vertically (scroll_y) |
| Ctrl + scroll wheel over roll | Translate camera horizontally (scroll_x) |
| Middle button drag | Pan: translate both axes freely |
| Shift + middle button drag | Scale: left/right drag = horizontal zoom, up/down drag = vertical zoom |

### Scrollbars (two — horizontal top, vertical right)

Each scrollbar thumb represents both position and zoom on its axis:
- **Thumb drag** → translate (move camera)
- **Thumb edge drag** → zoom (narrow/widen FOV)
- **Scroll wheel over scrollbar** → zoom that axis
- **Shift + scroll wheel over scrollbar** → translate that axis

### Rationale

Borrowed from FL Studio (speed, one-handed navigation) and Blender
(MMB as the universal "move around the viewport" key).
Shift+MMB drag for scaling mirrors Blender's viewport scale gesture.

The scrollbar design (thumb = position AND zoom) is deliberate:
it avoids separate zoom controls and makes the relationship between
the thumb size and the visible range visually obvious.

### Not yet implemented

- Scrollbars (visual + interaction)
- Pinch-to-zoom (touch / trackpad)
- "Draw viewport" rectangle tool (FL Studio magnifying glass)

---

## Timing Representation

**Date:** 2026-04-28
**Status:** Implemented

Integer ticks, not floats. PPQ taken verbatim from the MIDI file.
Avoids float precision issues and makes equality checks exact.
Snapping is integer division at interaction time — the stored value is exact.

Uses `int64_t` (Tick) so song length is never a concern at any BPM.

---

## Vendored Dependencies

**Date:** 2026-04-28
**Status:** Implemented

NanoVG, midifile, and glad are vendored in `external/`.
Both NanoVG and midifile are unmaintained upstream.
Vendoring means we can patch them freely and the build has no network dependency.

GLFW is still fetched (actively maintained, no compatibility issues).
glad is pre-generated into `external/glad/` — eliminates the Python/jinja2
build dependency. Regenerate with:
```
python -m glad --out-path external/glad --api gl:core=3.3 --reproducible c
```

---

## Two-LUT System (Piano Roll Lanes)

**Date:** 2026-04-28
**Status:** Implemented

`IS_BLACK_KEY[12]` — fixed chromatic layout, used only for the piano strip keys.
`SCALE_LUT[12]`    — which semitones are "outside" the active scale, used for lane backgrounds.

Defaults to a copy of IS_BLACK_KEY (chromatic = all black keys dimmed).
Later: set_scale() changes SCALE_LUT without touching the keyboard strip.

---

## Song vs Project Hierarchy

**Date:** 2026-04-28
**Status:** Pending (only Song exists today)

```
Project   — saved to disk; owns layout, mixer state, plugin instances
  └── Song — musical content: ppq, tempo map, tracks, notes
```

`Song` is currently the top-level object. `Project` is introduced when
there is layout or non-musical state worth persisting alongside the notes.

---

## Note Editor Interaction Model

**Date:** 2026-04-28
**Status:** Partially implemented (LMB place, RMB delete)

### Mouse actions on the piano roll canvas

| Input | Action |
|---|---|
| LMB (empty cell) | Place note at snapped position using current note properties |
| LMB (over note) | Copy that note's properties (pitch, duration, velocity, pan) — next placement pastes a note with those same properties |
| RMB (over note) | Delete note |
| RMB + drag | "Vaporization beam" — continuously deletes notes while dragging |
| Drag note body | Displace note (moves pitch + time together) |
| Drag note tail | Stretch duration, snapped to grid |
| Alt + drag note tail | Stretch duration in raw ticks (no snap) |
| Drag bounding box tail (multi-select) | Stretch all selected notes together |
| Shift + drag bounding box tail | Stretch in integer multiples only |

### Snap options (not yet implemented)
Common values: 1/3, 1/4, 1/8, 1/16, none.
Snap is per-axis (time only; pitch always snaps to semitone lane).

### Deferred
- Per-note pitch bend / microtone handle (Alt + up/down drag): deferred indefinitely.
- Selection bounding box (draw, move, stretch).
- RMB drag wipe.
- Note property copy-on-LMB-hover.

### Rationale
LMB = build, RMB = destroy mirrors most tile/grid editors.
Property copy on LMB-over-note avoids a separate "eyedropper" tool —
the active tool never changes, the cursor state changes.
Shift-integer-multiple stretch is borrowed from sequencer tools where
halving/doubling a selection is a common operation.
