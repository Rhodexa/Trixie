// piano_roll_ops.cpp
// All Piano Roll operators and the keymap that binds them.
//
// Layout:
//   view2d.*          — viewport navigation (scroll, zoom, pan)
//   piano_roll.*      — note editing (add, drag/resize, erase)
//   piano_roll_keymap — the binding table

#include "piano_roll_ops.h"
#include "piano_roll.h"        // coordinate utilities
#include "piano_roll_space.h"  // SpacePianoRoll (complete type)
#include "song.h"
#include "journal.h"
#include "note_commands.h"

#include <algorithm>
#include <memory>

// ═══════════════════════════════════════════════════════════════════════════════
// view2d.scroll_y  —  plain scroll wheel → vertical pan
// ═══════════════════════════════════════════════════════════════════════════════

static OpResult view2d_scroll_y_invoke(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    Viewport& vp    = ctx.space.viewport;
    float     zoom  = vp_zoom_y(vp);
    float     delta = ev.dy * 30.0f / zoom;
    float     range = vp.top - vp.bottom;
    float     new_t = std::min(128.0f, vp.top + delta);
    vp.top = new_t;
    vp.bottom = new_t - range;
    return OpResult::Finished;
}

static OperatorType VIEW2D_SCROLL_Y_OT = {
    "view2d.scroll_y", nullptr, view2d_scroll_y_invoke, nullptr, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// view2d.scroll_x  —  Ctrl+scroll → horizontal pan
// ═══════════════════════════════════════════════════════════════════════════════

static OpResult view2d_scroll_x_invoke(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    Viewport& vp    = ctx.space.viewport;
    float     zoom  = vp_zoom_x(vp);
    float     delta = ev.dy * 60.0f / zoom;
    float     range = vp.right - vp.left;
    float     new_l = std::max(0.0f, vp.left - delta);
    vp.left = new_l;
    vp.right = new_l + range;
    return OpResult::Finished;
}

static OperatorType VIEW2D_SCROLL_X_OT = {
    "view2d.scroll_x", nullptr, view2d_scroll_x_invoke, nullptr, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// view2d.zoom_y  —  Shift+scroll → vertical zoom toward cursor
// ═══════════════════════════════════════════════════════════════════════════════
// ToDo: Make zoom out clamp to content — 100% zoom out when viewport matches content bounding box.
static OpResult view2d_zoom_y_invoke(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    const Box& b    = ctx.region.winrct;
    Viewport&  vp   = ctx.space.viewport;
    float wy        = vp_to_world_y(vp, ev.y - b.y);
    float factor    = (ev.dy > 0.0f) ? 1.15f : (1.0f / 1.15f);
    float screen_h  = vp.screen_b - vp.screen_t;
    float old_range = vp.top - vp.bottom;
    float new_range = std::clamp(old_range / factor, screen_h / 80.0f, screen_h / 4.0f);
    vp_zoom_at_y(vp, wy, old_range / new_range);
    if (vp.top > 128.0f) {
        float diff = vp.top - 128.0f;
        vp.top  = 128.0f;
        vp.bottom -= diff;
    }
    return OpResult::Finished;
}

static OperatorType VIEW2D_ZOOM_Y_OT = {
    "view2d.zoom_y", nullptr, view2d_zoom_y_invoke, nullptr, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// view2d.zoom_x  —  Ctrl+Shift+scroll → horizontal zoom toward cursor
// ═══════════════════════════════════════════════════════════════════════════════

static OpResult view2d_zoom_x_invoke(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    const Box& b    = ctx.region.winrct;
    Viewport&  vp   = ctx.space.viewport;
    float wx        = vp_to_world_x(vp, ev.x - b.x);
    float factor    = (ev.dy > 0.0f) ? 1.15f : (1.0f / 1.15f);
    float screen_w  = vp.screen_r - vp.screen_l;
    float old_range = vp.right - vp.left;
    float new_range = std::clamp(old_range / factor, screen_w / 1000.0f, screen_w / 10.0f);
    vp_zoom_at_x(vp, wx, old_range / new_range);
    if (vp.left < 0.0f) {
        float diff = -vp.left;
        vp.left  = 0.0f;
        vp.right += diff;
    }
    return OpResult::Finished;
}

static OperatorType VIEW2D_ZOOM_X_OT = {
    "view2d.zoom_x", nullptr, view2d_zoom_x_invoke, nullptr, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// view2d.pan  —  MMB drag → free pan
// ═══════════════════════════════════════════════════════════════════════════════

struct PanState : OperatorState {};

static OpResult view2d_pan_invoke(wmOperator& op, wmOpContext&, const wmEvent&) {
    op.state = std::make_unique<PanState>();
    return OpResult::Running;
}

static OpResult view2d_pan_modal(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    if (ev.type == EventType::MouseMove) {
        Viewport& vp      = ctx.space.viewport;
        float     range_x = vp.right - vp.left;
        float     range_y = vp.top - vp.bottom;
        float     new_l   = std::max(0.0f, vp.left - ev.dx / vp_zoom_x(vp));
        float     new_t   = std::min(128.0f, vp.top + ev.dy / vp_zoom_y(vp));
        vp.left = new_l;
        vp.right = new_l + range_x;
        vp.top = new_t;
        vp.bottom = new_t - range_y;
        return OpResult::Running;
    }
    if (ev.type == EventType::MiddleMouse && ev.value == EventValue::Release)
        return OpResult::Finished;
    return OpResult::Running;
}

static OperatorType VIEW2D_PAN_OT = {
    "view2d.pan", nullptr, view2d_pan_invoke, view2d_pan_modal, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// piano_roll.note_drag  —  LMB on an existing note → move or resize
// Returns Pass if the cursor isn't over a note (falls through to note_add).
// ═══════════════════════════════════════════════════════════════════════════════

struct NoteDragState : OperatorState {
    int      track, note_idx;
    NotePart part;
    Note     original;
    Tick     tick_offset;
    Tick     snap;
};

static OpResult note_drag_invoke(wmOperator& op, wmOpContext& ctx, const wmEvent& ev) {
    const Box& b  = ctx.region.winrct;
    float cx = ev.x - b.x, cy = ev.y - b.y;

    auto hit = piano_roll_hit_test(ctx.song, ctx.space, cx, cy);
    if (!hit) return OpResult::Pass;   // no note under cursor → try next item

    auto s = std::make_unique<NoteDragState>();
    s->track       = hit->track;
    s->note_idx    = hit->note_idx;
    s->part        = hit->part;
    s->original    = ctx.song.tracks[hit->track].notes[hit->note_idx];
    s->snap        = ctx.song.ppq;
    s->tick_offset = piano_roll_x_to_tick(ctx.song, ctx.space, cx, 0) - s->original.start;
    op.state = std::move(s);
    return OpResult::Running;
}

static OpResult note_drag_modal(wmOperator& op, wmOpContext& ctx, const wmEvent& ev) {
    auto& s        = *static_cast<NoteDragState*>(op.state.get());
    const Box& b   = ctx.region.winrct;

    if (ev.type == EventType::MouseMove) {
        float cx = ev.x - b.x, cy = ev.y - b.y;
        Note& note         = ctx.song.tracks[s.track].notes[s.note_idx];
        Tick  original_end = s.original.start + s.original.duration;

        if (s.part == NotePart::BODY) {
            Tick raw   = piano_roll_x_to_tick(ctx.song, ctx.space, cx, 0) - s.tick_offset;
            note.start = std::max(0LL, snap_to_nearest(raw, s.snap));
            note.pitch = piano_roll_y_to_pitch(ctx.space, cy);

        } else if (s.part == NotePart::TAIL) {
            /*
                Rhode: ok, let me understand this logic:
                    * raw_end is where the note's tail is in _ticks_
                    * snapped_end would be the predicted position of the tail once snapped <- this is my current point of interest
                        (scrapped) raw_end / snap_ticks does basically a floor. This is kind of a flaw in the logic, let me think, i believe snapping of tail should happen away from 0
                            Turns out rounding from the midpoint is better.
                    * min_end makes sense: a note cannot be smaller than this, basically.
                        Thinking about it, I think there are _two_ minimum ends:
                            1. this one, which is literally the smallest unit of time
                            2. the configured minimum.
                            Let me think.
                            So, FL Studio, for example, does not limit notes to a minimum tick, instead it has a somewhat larger limit. When a note is _that_ small it turns it into a beat (uses a different rendering style for the note, to signal it cannot be smaller)
                            This is useful for percussives. Perhaps, the whole start + snap_ticks is enough for now, but this should be able to be even smaller: by holding alt
                            So there _are_ at least two mins, just like for the tail: where you can have raw and snapped, min can be start + snapping, or start + n_ticks (when holding alt). "n_ticks" is just a placeholder, could be 1, 2, 8... idk. Emperical testing would reveal
            */
            Tick raw_end     = piano_roll_x_to_tick(ctx.song, ctx.space, cx, 0);
            Tick snapped_end = snap_to_nearest(raw_end, s.snap);
            note.duration    = std::max(snapped_end, note.start + s.snap) - note.start;

        } else { // HEAD
            Tick snapped  = snap_to_nearest(piano_roll_x_to_tick(ctx.song, ctx.space, cx, 0), s.snap);
            note.start    = std::clamp(snapped, 0LL, original_end - s.snap);
            note.duration = original_end - note.start;
        }
        return OpResult::Running;
    }

    if (ev.type == EventType::LeftMouse && ev.value == EventValue::Release) {
        Note& cur = ctx.song.tracks[s.track].notes[s.note_idx];
        if (cur.start    != s.original.start ||
            cur.pitch    != s.original.pitch  ||
            cur.duration != s.original.duration) {
            Note final_note = cur;
            ctx.journal.commit(std::make_unique<EditNoteCommand>(
                ctx.song, s.track, s.note_idx, s.original, final_note,
                s.part == NotePart::BODY ? "Move note" :
                s.part == NotePart::TAIL ? "Resize note tail" : "Resize note head"));
        }
        return OpResult::Finished;
    }

    return OpResult::Running;
}

static void note_drag_cancel(wmOperator& op, wmOpContext& ctx) {
    auto& s = *static_cast<NoteDragState*>(op.state.get());
    ctx.song.tracks[s.track].notes[s.note_idx] = s.original;
}

static OperatorType NOTE_DRAG_OT = {
    "piano_roll.note_drag", nullptr,
    note_drag_invoke, note_drag_modal, note_drag_cancel
};

// ═══════════════════════════════════════════════════════════════════════════════
// piano_roll.note_add  —  LMB on empty canvas → place a new note
// poll: requires at least one track to exist.
// ═══════════════════════════════════════════════════════════════════════════════

static bool note_add_poll(const wmOpContext& ctx) {
    return !ctx.song.tracks.empty();
}

struct NotePlaceState : OperatorState {
    Note current;
    Tick snap;
};

static OpResult note_add_invoke(wmOperator& op, wmOpContext& ctx, const wmEvent& ev) {
    const Box& b = ctx.region.winrct;
    float cx = ev.x - b.x, cy = ev.y - b.y;
    Tick  snap = ctx.song.ppq;

    auto note = piano_roll_make_note(ctx.song, ctx.space, cx, cy, snap, snap, 100);
    if (!note) return OpResult::Cancelled;

    auto s = std::make_unique<NotePlaceState>();
    s->current           = *note;
    s->snap              = snap;
    ctx.space.pending_note = *note;
    op.state = std::move(s);
    return OpResult::Running;
}

static OpResult note_add_modal(wmOperator& op, wmOpContext& ctx, const wmEvent& ev) {
    auto& s    = *static_cast<NotePlaceState*>(op.state.get());
    const Box& b = ctx.region.winrct;

    if (ev.type == EventType::MouseMove) {
        float cx = ev.x - b.x, cy = ev.y - b.y;
        auto note = piano_roll_make_note(ctx.song, ctx.space, cx, cy, s.snap, s.snap, 100);
        if (note) { s.current = *note; ctx.space.pending_note = *note; }
        return OpResult::Running;
    }

    if (ev.type == EventType::LeftMouse && ev.value == EventValue::Release) {
        ctx.journal.commit(std::make_unique<AddNoteCommand>(
            ctx.song, 0, s.current, "Add note"));
        ctx.space.pending_note = std::nullopt;
        return OpResult::Finished;
    }

    return OpResult::Running;
}

static void note_add_cancel(wmOperator&, wmOpContext& ctx) {
    ctx.space.pending_note = std::nullopt;
}

static OperatorType NOTE_ADD_OT = {
    "piano_roll.note_add", note_add_poll,
    note_add_invoke, note_add_modal, note_add_cancel
};

// ═══════════════════════════════════════════════════════════════════════════════
// piano_roll.note_erase  —  RMB drag → erase notes under cursor
// ═══════════════════════════════════════════════════════════════════════════════

struct EraseState : OperatorState {};

static void erase_at(wmOpContext& ctx, float cx, float cy) {
    auto hit = piano_roll_hit_test(ctx.song, ctx.space, cx, cy);
    if (hit)
        ctx.journal.commit(std::make_unique<RemoveNoteCommand>(
            ctx.song, hit->track, hit->note_idx, "Remove note"));
}

static OpResult note_erase_invoke(wmOperator& op, wmOpContext& ctx, const wmEvent& ev) {
    const Box& b = ctx.region.winrct;
    erase_at(ctx, ev.x - b.x, ev.y - b.y);
    op.state = std::make_unique<EraseState>();
    return OpResult::Running;
}

static OpResult note_erase_modal(wmOperator&, wmOpContext& ctx, const wmEvent& ev) {
    if (ev.type == EventType::MouseMove) {
        const Box& b = ctx.region.winrct;
        erase_at(ctx, ev.x - b.x, ev.y - b.y);
        return OpResult::Running;
    }
    if (ev.type == EventType::RightMouse && ev.value == EventValue::Release)
        return OpResult::Finished;
    return OpResult::Running;
}

static OperatorType NOTE_ERASE_OT = {
    "piano_roll.note_erase", nullptr,
    note_erase_invoke, note_erase_modal, nullptr
};

// ═══════════════════════════════════════════════════════════════════════════════
// Piano Roll keymap
//
// Rules:
//   - More-specific modifier combos must appear before less-specific ones.
//   - note_drag comes before note_add (drag returns Pass if no note hit,
//     which causes the dispatch to fall through to note_add).
// ═══════════════════════════════════════════════════════════════════════════════

static const wmKeyMapItem PIANO_ROLL_KEYMAP_ITEMS[] = {
    //  type                  value                mods               key  op
    // Shift → vertical zoom
    { EventType::WheelUp,   EventValue::Nothing, INPUT_MOD_CONTROL, 0, &VIEW2D_ZOOM_Y_OT   },
    { EventType::WheelDown, EventValue::Nothing, INPUT_MOD_CONTROL, 0, &VIEW2D_ZOOM_Y_OT   },

    // Ctrl → horizontal zoom
    { EventType::WheelUp,   EventValue::Nothing, INPUT_MOD_SHIFT,   0, &VIEW2D_ZOOM_X_OT   },
    { EventType::WheelDown, EventValue::Nothing, INPUT_MOD_SHIFT,   0, &VIEW2D_ZOOM_X_OT   },

    // Alt → horizontal scroll
    { EventType::WheelUp,   EventValue::Nothing, INPUT_MOD_ALT,     0, &VIEW2D_SCROLL_X_OT },
    { EventType::WheelDown, EventValue::Nothing, INPUT_MOD_ALT,     0, &VIEW2D_SCROLL_X_OT },

    // plain → vertical scroll (must be last — mods=0 matches everything)
    { EventType::WheelUp,   EventValue::Nothing, 0,                 0, &VIEW2D_SCROLL_Y_OT },
    { EventType::WheelDown, EventValue::Nothing, 0,                 0, &VIEW2D_SCROLL_Y_OT },

    // Pan (MMB)
    { EventType::MiddleMouse, EventValue::Press, 0,                                 0, &VIEW2D_PAN_OT      },

    // Note editing — drag tried first; returns Pass on miss, falls to add
    { EventType::LeftMouse,   EventValue::Press, 0,                                 0, &NOTE_DRAG_OT       },
    { EventType::LeftMouse,   EventValue::Press, 0,                                 0, &NOTE_ADD_OT        },

    // Erase (RMB)
    { EventType::RightMouse,  EventValue::Press, 0,                                 0, &NOTE_ERASE_OT      },
};

const wmKeyMap& piano_roll_keymap() {
    static const wmKeyMap km = {
        PIANO_ROLL_KEYMAP_ITEMS,
        (int)(sizeof(PIANO_ROLL_KEYMAP_ITEMS) / sizeof(PIANO_ROLL_KEYMAP_ITEMS[0]))
    };
    return km;
}
