// wm.h
// Window manager layer — mirrors Blender's wm module.
//
// Three concepts:
//   wmEvent      — a unified, editor-level input event (translated from raw OS events)
//   OperatorType — static descriptor: poll / invoke / modal / cancel callbacks
//   wmKeyMap     — declarative table binding events to operators
//
// Adding a new interaction means: write an OperatorType, add a wmKeyMapItem.
// Nothing else changes.

#pragma once

#include "editor.h"       // ARegion
#include "input_queue.h"  // InputEvent (raw events from GLFW layer)
#include <memory>
#include <optional>

// Forward-declare editor-specific types so wm.h stays generic.
struct SpacePianoRoll;
struct Song;
class  Journal;

// ── wmEvent ──────────────────────────────────────────────────────────────────
// One unified event type. Translated from raw InputEvent at the region boundary.

enum class EventType {
    None,
    LeftMouse, RightMouse, MiddleMouse,
    WheelUp, WheelDown,
    MouseMove,
    Key,
};

enum class EventValue {
    Nothing,   // scroll, move
    Press,
    Release,
};

struct wmEvent {
    EventType  type  = EventType::None;
    EventValue value = EventValue::Nothing;
    int        mods  = 0;        // InputMod flags
    float      x = 0, y = 0;    // cursor position (screen-space)
    float      dx = 0, dy = 0;  // delta: mouse move distance or scroll amount
    int        key = 0;          // GLFW_KEY_* (Key events only)
};

wmEvent wm_event_from_input(const InputEvent& raw);

// ── Operator protocol ─────────────────────────────────────────────────────────
// Mirrors Blender's wmOperatorType / wmOperator split:
//   OperatorType  — static, one per operation kind
//   wmOperator    — live instance, owns its own state while running

enum class OpResult {
    Finished,    // op completed normally — event consumed
    Cancelled,   // op aborted            — event consumed
    Running,     // op started modal      — event consumed
    Pass,        // op didn't handle it   — try next keymap item
};

// Heap-allocated operator state. Each op defines its own subtype.
struct OperatorState {
    virtual ~OperatorState() = default;
};

// Context passed to every operator callback — everything an op could need.
struct wmOpContext {
    ARegion&        region;
    SpacePianoRoll& space;
    Song&           song;
    Journal&        journal;
};

struct wmOperator;

// Static descriptor for one kind of operation.
struct OperatorType {
    const char* idname;

    // Returns false → op is unavailable in this context (skipped by dispatch).
    bool     (*poll)  (const wmOpContext&);

    // Called on the triggering event. Return Running to enter modal.
    // Return Pass to fall through to the next keymap item.
    OpResult (*invoke)(wmOperator&, wmOpContext&, const wmEvent&);

    // Called for every subsequent event while modal. Return Finished/Cancelled to exit.
    OpResult (*modal) (wmOperator&, wmOpContext&, const wmEvent&);

    // Called on Escape / forced cancel. Should revert any in-progress changes.
    void     (*cancel)(wmOperator&, wmOpContext&);
};

// Live operator instance.
struct wmOperator {
    const OperatorType*            type;
    std::unique_ptr<OperatorState> state;
};

// ── wmKeyMap ──────────────────────────────────────────────────────────────────
// Declarative binding table. Items are checked in order — put most-specific
// modifier combos first so they match before less-specific catch-alls.

struct wmKeyMapItem {
    EventType         type;
    EventValue        value;
    int               mods;   // required InputMod flags (subset match)
    int               key;    // GLFW_KEY_* for Key events; 0 = not checked
    const OperatorType* op;
};

struct wmKeyMap {
    const wmKeyMapItem* items;
    int                 count;
};

// Returns true if ev satisfies item's type, value, mods, and key constraints.
bool wm_event_matches(const wmEvent& ev, const wmKeyMapItem& item);

// Full invoke→modal dispatch for one event.
// If an op starts Running, it is stored in active_modal_op.
// Returns true if the event was consumed (Finished / Cancelled / Running).
bool wm_handle_event(wmOpContext& ctx, const wmEvent& ev,
                     const wmKeyMap& keymap,
                     std::optional<wmOperator>& active_modal_op);
